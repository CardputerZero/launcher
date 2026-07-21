/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "main.h"
#include "lvgl/lvgl.h"
#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_file.hpp"
#include "keyboard_input.h"
#include "zclaw_client.h"
#include "zclaw_fonts.hpp"
#include "zclaw_provider_store.h"

#include <linux/input.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <pthread.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

volatile sig_atomic_t g_quit_requested = 0;

inline const struct key_item *keyboard_item(lv_event_t *event)
{
    return static_cast<const struct key_item *>(lv_event_get_param(event));
}

inline uint32_t keyboard_key(lv_event_t *event)
{
    const struct key_item *item = keyboard_item(event);
    return item ? item->key_code : 0;
}

class ZClawApp
{
    static constexpr lv_coord_t SCREEN_W = 320;
    static constexpr lv_coord_t SCREEN_H = 170;

    static constexpr uint32_t COLOR_BG = 0x0F0F1A;
    static constexpr uint32_t COLOR_BAR = 0x16162A;
    static constexpr uint32_t COLOR_PANEL = 0x1E1E35;
    static constexpr uint32_t COLOR_PANEL_LINE = 0x2A2A45;
    static constexpr uint32_t COLOR_MUTED = 0xA1A1AA;
    static constexpr uint32_t COLOR_DIM = 0x52525B;
    static constexpr uint32_t COLOR_TEXT = 0xF4F4F5;
    static constexpr uint32_t COLOR_ONLINE = 0x10B981;
    static constexpr uint32_t COLOR_PURPLE = 0x8B5CF6;
    static constexpr uint32_t COLOR_INDIGO = 0x6366F1;
    static constexpr lv_coord_t USER_BUBBLE_MAX_W = 198;
    static constexpr lv_coord_t USER_BUBBLE_MIN_W = 38;
    static constexpr lv_coord_t USER_BUBBLE_PAD_X = 10;
    static constexpr lv_coord_t USER_BUBBLE_PAD_Y = 6;
    static constexpr lv_coord_t CHAT_ROW_W = 296;
    static constexpr int SETTINGS_ROW_MAX = 5;

    enum class SettingsView {
        Setup,
        SetupProviders,
        Authorization,
        Main,
        Providers,
        ProviderDetail
    };

    enum class ProviderEditField {
        None,
        Alias,
        Family,
        Model,
        Uri,
        ApiKey
    };

    enum class InputMode {
        Chat,
        SetupEdit,
        ProviderEdit,
        PairingCode
    };

    enum class SetupEditField {
        None,
        Uri,
        ApiKey,
        Model
    };

    enum class StartupState {
        CheckingNetwork,
        Offline,
        Ready
    };

    struct AsyncResult {
        ZClawApp *self = nullptr;
        std::string text;
        bool ok = false;
        UiConfig config;
    };

    struct ApprovalPromptData {
        ZClawApp *self = nullptr;
        ZClawApprovalRequest request;
    };

    struct NetworkResult {
        ZClawApp *self = nullptr;
        bool online = false;
        std::string error;
    };

    struct SetupProgressData {
        ZClawApp *self = nullptr;
        ZClawSetupProgress progress;
    };

    lv_obj_t *screen_ = nullptr;
    lv_obj_t *startup_overlay_ = nullptr;
    lv_obj_t *startup_title_ = nullptr;
    lv_obj_t *startup_detail_ = nullptr;
    lv_obj_t *chat_container_ = nullptr;
    lv_obj_t *scroll_track_ = nullptr;
    lv_obj_t *scroll_thumb_ = nullptr;
    lv_obj_t *user_bubble_ = nullptr;
    lv_obj_t *user_label_ = nullptr;
    lv_obj_t *reply_bubble_ = nullptr;
    lv_obj_t *reply_label_ = nullptr;
    lv_obj_t *input_bar_ = nullptr;
    lv_obj_t *input_box_ = nullptr;
    lv_obj_t *input_label_ = nullptr;
    lv_obj_t *input_sparkle_ = nullptr;
    lv_obj_t *send_button_ = nullptr;
    lv_obj_t *input_dialog_ = nullptr;
    lv_obj_t *input_textarea_ = nullptr;
    lv_obj_t *approval_dialog_ = nullptr;
    lv_obj_t *approval_tool_label_ = nullptr;
    lv_obj_t *approval_summary_label_ = nullptr;
    lv_obj_t *approval_buttons_[3] = {};
    lv_obj_t *settings_panel_ = nullptr;
    lv_obj_t *settings_header_label_ = nullptr;
    lv_obj_t *settings_hint_label_ = nullptr;
    lv_obj_t *setup_spinner_ = nullptr;
    lv_obj_t *setup_status_label_ = nullptr;
    lv_obj_t *setup_detail_label_ = nullptr;
    lv_obj_t *setup_progress_bar_ = nullptr;
    lv_obj_t *setup_percent_label_ = nullptr;
    lv_obj_t *setup_speed_label_ = nullptr;
    lv_obj_t *settings_rows_[SETTINGS_ROW_MAX] = {};
    lv_obj_t *settings_values_[SETTINGS_ROW_MAX] = {};
    lv_style_t textarea_cursor_style_;
    bool textarea_cursor_style_inited_ = false;
    bool settings_animating_ = false;
    bool settings_closing_ = false;
    SettingsView settings_view_ = SettingsView::Main;
    int settings_selected_ = 0;
    int settings_row_count_ = 0;
    int provider_selected_ = 0;
    int provider_scroll_ = 0;
    int provider_detail_index_ = -1;
    ProviderEditField provider_edit_field_ = ProviderEditField::None;
    SetupEditField setup_edit_field_ = SetupEditField::None;
    InputMode input_mode_ = InputMode::Chat;
    StartupState startup_state_ = StartupState::CheckingNetwork;
    UiConfig ui_config_;
    std::vector<ProviderConfig> providers_;
    ProviderConfig setup_provider_;
    int setup_provider_selected_ = 0;
    int setup_provider_scroll_ = 0;
    std::string avatar_path_;
    std::string sparkles_path_;
    std::string send_button_path_;
    uint32_t reply_seed_ = 0;
    bool request_in_flight_ = false;
    bool setup_in_flight_ = false;
    std::mutex approval_mutex_;
    std::condition_variable approval_cv_;
    std::string pending_approval_id_;
    std::string pending_approval_tool_;
    std::string pending_approval_decision_;
    bool approval_waiting_ = false;
    int approval_selected_ = 0;
    ZClawClient client_;
    zclaw::FontManager fonts_;

public:
    ZClawApp()
    {
        root_screen_ = lv_screen_active();
        fonts_.init();
        avatar_path_ = cp0_file_path("zclaw_avatar_16.png");
        sparkles_path_ = cp0_file_path("zclaw_sparkles_10.png");
        send_button_path_ = cp0_file_path("zclaw_send_button_18.png");
        load_providers();
        load_ui_config();
        setup_provider_ = providers_.empty() ? provider_preset(0) : providers_[0];
        create_ui();
        event_handler_init();
        show_network_check();
        start_network_check();
    }

private:
    lv_obj_t *root_screen_ = nullptr;
    static lv_obj_t *make_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                              uint32_t color, lv_coord_t radius = 0)
    {
        lv_obj_t *obj = lv_obj_create(parent);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_size(obj, w, h);
        lv_obj_set_style_radius(obj, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        return obj;
    }

    static lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                                lv_coord_t w, lv_coord_t h, const lv_font_t *font, uint32_t color,
                                lv_text_align_t align = LV_TEXT_ALIGN_LEFT)
    {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, text ? text : "");
        lv_obj_set_pos(label, x, y);
        lv_obj_set_size(label, w, h);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(label, align, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
        return label;
    }

    static void apply_vertical_gradient(lv_obj_t *obj, uint32_t top, uint32_t bottom)
    {
        lv_obj_set_style_bg_color(obj, lv_color_hex(top), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(obj, lv_color_hex(bottom), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    static lv_coord_t centered_y(lv_coord_t container_h, lv_coord_t item_h)
    {
        return (container_h - item_h) / 2;
    }

    static ProviderConfig provider_preset(int index)
    {
        switch (index) {
        case 0:
            return {"zclaw", "openai", "gpt-4.1-mini", "https://api.openai.com/v1", ""};
        case 1:
            return {"zclaw", "openrouter", "openrouter/auto", "https://openrouter.ai/api/v1", ""};
        case 2:
            return {"zclaw", "anthropic", "claude-sonnet-4", "https://api.anthropic.com", ""};
        case 3:
            return {"zclaw", "ollama", "llama3.1", "http://127.0.0.1:11434", ""};
        case 4:
            return {"zclaw", "deepseek", "deepseek-chat", "https://api.deepseek.com", ""};
        default:
            return {"zclaw", "custom", "", "https://api.example.com/v1", ""};
        }
    }

    static const char *provider_display_name(const std::string &family)
    {
        if (family == "openai") return "OpenAI";
        if (family == "openrouter") return "OpenRouter";
        if (family == "anthropic") return "Anthropic";
        if (family == "ollama") return "Ollama";
        if (family == "deepseek") return "DeepSeek";
        return "Custom";
    }

    static std::string display_text_compat(const std::string &text)
    {
        return text;
    }

    bool first_run_needed() const
    {
        return ZClawClient::first_run_needed(ui_config_);
    }

    void load_providers()
    {
        zclaw::load_provider_configs(ZClawClient::providers_config_path(), &providers_);

        if (providers_.empty()) {
            for (int i = 0; i < 6; ++i)
                providers_.push_back(provider_preset(i));
        }
    }

    bool save_providers(std::string *error = nullptr)
    {
        ZClawClient::ensure_storage_dir();
        return zclaw::save_provider_configs(ZClawClient::providers_config_path(), providers_, error);
    }

    void report_save_error(const std::string &error)
    {
        append_ai_message(error.empty() ? "Could not save settings." : error.c_str());
    }

    bool save_setup_provider()
    {
        std::vector<ProviderConfig> candidate = providers_;
        if (candidate.empty())
            candidate.push_back(setup_provider_);
        else
            candidate[0] = setup_provider_;
        std::string error;
        ZClawClient::ensure_storage_dir();
        if (!zclaw::save_provider_configs(ZClawClient::providers_config_path(), candidate, &error)) {
            report_save_error(error);
            return false;
        }
        providers_ = std::move(candidate);
        return true;
    }

    void load_ui_config()
    {
        std::ifstream file(ZClawClient::ui_config_path());
        std::string line;
        while (std::getline(file, line)) {
            std::vector<std::string> fields = zclaw::split_config_line(line);
            if (fields.size() < 2)
                continue;
            const std::string key = zclaw::decode_config_field(fields[0]);
            const std::string value = zclaw::decode_config_field(fields[1]);
            if (key == "webhook_url")
                ui_config_.webhook_url = value;
            else if (key == "agent_alias")
                ui_config_.agent_alias = value.empty() ? "zclaw" : value;
            else if (key == "webhook_secret")
                ui_config_.webhook_secret = value;
            else if (key == "bearer_token")
                ui_config_.bearer_token = value;
            else if (key == "setup_complete")
                ui_config_.setup_complete = value == "1";
        }
    }

    bool save_ui_config()
    {
        ZClawClient::ensure_storage_dir();
        std::string contents;
        contents += zclaw::encode_config_field("webhook_url") + '\t' + zclaw::encode_config_field(ui_config_.webhook_url) + '\n';
        contents += zclaw::encode_config_field("agent_alias") + '\t' + zclaw::encode_config_field(ui_config_.agent_alias) + '\n';
        contents += zclaw::encode_config_field("webhook_secret") + '\t' + zclaw::encode_config_field(ui_config_.webhook_secret) + '\n';
        contents += zclaw::encode_config_field("bearer_token") + '\t' + zclaw::encode_config_field(ui_config_.bearer_token) + '\n';
        contents += zclaw::encode_config_field("setup_complete") + '\t' + (ui_config_.setup_complete ? "1" : "0") + '\n';
        std::string error;
        if (zclaw::atomic_write_config(ZClawClient::ui_config_path(), contents, &error))
            return true;
        report_save_error(error);
        return false;
    }

    static const char *provider_field_name(ProviderEditField field)
    {
        switch (field) {
        case ProviderEditField::Alias:
            return "Alias";
        case ProviderEditField::Family:
            return "Family";
        case ProviderEditField::Model:
            return "Model";
        case ProviderEditField::Uri:
            return "URI";
        case ProviderEditField::ApiKey:
            return "API Key";
        case ProviderEditField::None:
        default:
            return "";
        }
    }

    std::string &provider_field_value(ProviderConfig &provider, ProviderEditField field)
    {
        switch (field) {
        case ProviderEditField::Alias:
            return provider.alias;
        case ProviderEditField::Family:
            return provider.family;
        case ProviderEditField::Model:
            return provider.model;
        case ProviderEditField::Uri:
            return provider.uri;
        case ProviderEditField::ApiKey:
        case ProviderEditField::None:
        default:
            return provider.api_key;
        }
    }

    lv_obj_t *make_zclaw_avatar(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t size)
    {
        lv_obj_t *avatar = lv_img_create(parent);
        lv_img_set_src(avatar, avatar_path_.c_str());
        lv_obj_set_pos(avatar, x, y);
        if (size != 16) {
            lv_img_set_zoom(avatar, (uint16_t)((size * 256) / 16));
        }
        lv_obj_clear_flag(avatar, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        return avatar;
    }

    void show_network_check()
    {
        startup_state_ = StartupState::CheckingNetwork;
        startup_overlay_ = make_box(screen_, 0, 0, SCREEN_W, SCREEN_H, COLOR_BG);
        lv_obj_move_foreground(startup_overlay_);
        make_label(startup_overlay_, "ZCLAW", 16, 24, 288, 20, fonts_.font_12(), COLOR_PURPLE,
                   LV_TEXT_ALIGN_CENTER);
        startup_title_ = make_label(startup_overlay_, "Checking network...", 16, 68, 288, 20,
                                    fonts_.font_12(), COLOR_TEXT, LV_TEXT_ALIGN_CENTER);
        startup_detail_ = make_label(startup_overlay_, "Please wait", 16, 96, 288, 16,
                                     fonts_.font_10(), COLOR_MUTED, LV_TEXT_ALIGN_CENTER);
    }

    void show_offline(const std::string &error)
    {
        startup_state_ = StartupState::Offline;
        lv_label_set_text(startup_title_, "No internet connection");
        const std::string detail = error.empty() ? "Check Wi-Fi and try again." :
                                   "Check Wi-Fi and try again.\n" + error;
        lv_label_set_text(startup_detail_, detail.c_str());
        lv_obj_set_height(startup_detail_, 34);
        make_label(startup_overlay_, "OK  EXIT", 16, 142, 288, 14, fonts_.font_10(),
                   COLOR_PURPLE, LV_TEXT_ALIGN_CENTER);
    }

    static void network_async_done(void *data)
    {
        NetworkResult *result = static_cast<NetworkResult *>(data);
        if (!result)
            return;
        if (result->self) {
            if (!result->online) {
                result->self->show_offline(result->error);
            } else {
                result->self->startup_state_ = StartupState::Ready;
                if (result->self->startup_overlay_) {
                    lv_obj_del(result->self->startup_overlay_);
                    result->self->startup_overlay_ = nullptr;
                    result->self->startup_title_ = nullptr;
                    result->self->startup_detail_ = nullptr;
                }
                if (result->self->first_run_needed())
                    result->self->open_setup_panel();
            }
        }
        delete result;
    }

    static void *network_thread_main(void *data)
    {
        NetworkResult *result = static_cast<NetworkResult *>(data);
        if (!result)
            return nullptr;
        result->online = ZClawClient::has_internet_connection(&result->error);
        lv_async_call(network_async_done, result);
        return nullptr;
    }

    void start_network_check()
    {
        NetworkResult *result = new NetworkResult();
        result->self = this;
        pthread_t thread_id{};
        if (pthread_create(&thread_id, nullptr, network_thread_main, result) != 0) {
            result->error = "Could not start network check.";
            network_async_done(result);
            return;
        }
        pthread_detach(thread_id);
    }

    void create_ui()
    {
        screen_ = make_box(root_screen_, 0, 0, SCREEN_W, SCREEN_H, COLOR_BG);
        lv_obj_move_foreground(screen_);

        create_top_bar();
        create_chat_area();
        create_input_bar();
    }

    void create_top_bar()
    {
        static constexpr lv_coord_t BAR_H = 20;
        static constexpr lv_coord_t AVATAR_SIZE = 16;
        static constexpr lv_coord_t STATUS_SIZE = 6;
        static constexpr lv_coord_t DOT_SIZE = 2;
        const lv_coord_t name_h = lv_font_get_line_height(fonts_.font_12());
        const lv_coord_t status_h = lv_font_get_line_height(fonts_.font_10());

        lv_obj_t *bar = make_box(screen_, 0, 0, SCREEN_W, BAR_H, COLOR_BAR);
        make_zclaw_avatar(bar, 12, centered_y(BAR_H, AVATAR_SIZE), AVATAR_SIZE);

        make_box(bar, 34, centered_y(BAR_H, STATUS_SIZE), STATUS_SIZE, STATUS_SIZE, COLOR_ONLINE, STATUS_SIZE / 2);
        make_label(bar, "ZClaw", 47, centered_y(BAR_H, name_h), 42, name_h, fonts_.font_12(), COLOR_TEXT);
        make_label(bar, "Online", 89, centered_y(BAR_H, status_h), 48, status_h, fonts_.font_10(), COLOR_ONLINE);

        const lv_coord_t ellipsis_y = centered_y(BAR_H, DOT_SIZE);
        make_box(bar, 295, ellipsis_y, DOT_SIZE, DOT_SIZE, COLOR_MUTED, DOT_SIZE / 2);
        make_box(bar, 300, ellipsis_y, DOT_SIZE, DOT_SIZE, COLOR_MUTED, DOT_SIZE / 2);
        make_box(bar, 305, ellipsis_y, DOT_SIZE, DOT_SIZE, COLOR_MUTED, DOT_SIZE / 2);
    }

    void create_chat_area()
    {
        make_box(screen_, 0, 20, SCREEN_W, 128, COLOR_BG);

        chat_container_ = lv_obj_create(screen_);
        lv_obj_set_pos(chat_container_, 0, 20);
        lv_obj_set_size(chat_container_, 312, 128);
        lv_obj_set_style_bg_opa(chat_container_, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(chat_container_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(chat_container_, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(chat_container_, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(chat_container_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(chat_container_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_row(chat_container_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_flex_flow(chat_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(chat_container_, LV_DIR_VER);
        lv_obj_add_flag(chat_container_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(chat_container_, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLL_ELASTIC |
                                                           LV_OBJ_FLAG_SCROLL_MOMENTUM |
                                                           LV_OBJ_FLAG_SCROLL_CHAIN));
        lv_obj_set_scrollbar_mode(chat_container_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_event_cb(chat_container_, ZClawApp::static_chat_scroll_handler, LV_EVENT_SCROLL, this);

        append_ai_message("Hello! I am ZClaw.\nReady to help with your device.");

        scroll_track_ = make_box(screen_, 312, 20, 3, 126, 0x33335A, 2);
        scroll_thumb_ = make_box(screen_, 312, 28, 3, 38, 0x818CF8, 2);
        apply_vertical_gradient(scroll_thumb_, 0xC4B5FD, 0x818CF8);
        update_scrollbar();
    }

    void create_input_bar()
    {
        input_bar_ = make_box(screen_, 0, 148, SCREEN_W, 22, COLOR_BAR);
        make_box(input_bar_, 0, 0, SCREEN_W, 1, COLOR_PANEL_LINE);

        input_box_ = make_box(input_bar_, 10, 3, 274, 16, COLOR_PANEL, 8);
        input_sparkle_ = make_sparkle(input_box_, 8, 3);
        input_label_ = make_label(input_box_, "Press Enter to ask", 26, 3, 180, 10,
                                  fonts_.font_10(), COLOR_DIM);

        send_button_ = make_send_button(input_bar_, 292, 2);
    }

    lv_obj_t *make_settings_row(lv_obj_t *parent, int index, lv_coord_t y, const char *title, const char *value)
    {
        lv_obj_t *row = make_box(parent, 12, y, 296, 26, COLOR_PANEL, 8);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(row, lv_color_hex(COLOR_PANEL), LV_PART_MAIN | LV_STATE_DEFAULT);
        make_label(row, title, 10, 5, 160, 14, fonts_.font_10(), COLOR_TEXT);
        settings_values_[index] = make_label(row, value, 168, 5, 118, 14, fonts_.font_10(),
                                             COLOR_MUTED, LV_TEXT_ALIGN_RIGHT);
        make_box(row, 8, 23, 280, 1, COLOR_PANEL_LINE);
        settings_rows_[index] = row;
        return row;
    }

    void update_settings_selection()
    {
        for (int i = 0; i < SETTINGS_ROW_MAX; ++i) {
            if (!settings_rows_[i])
                continue;
            const bool selected = i == settings_selected_;
            lv_obj_set_style_border_color(settings_rows_[i],
                                          lv_color_hex(selected ? COLOR_PURPLE : COLOR_PANEL),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(settings_rows_[i],
                                      lv_color_hex(selected ? 0x252542 : COLOR_PANEL),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            if (settings_values_[i])
                lv_obj_set_style_text_color(settings_values_[i],
                                            lv_color_hex(selected ? COLOR_TEXT : COLOR_MUTED),
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void clear_settings_rows()
    {
        for (auto *&row : settings_rows_) {
            if (row)
                lv_obj_del(row);
            row = nullptr;
        }
        for (auto *&value : settings_values_)
            value = nullptr;
        settings_row_count_ = 0;
    }

    void add_settings_row(const char *title, const char *value)
    {
        if (settings_row_count_ >= SETTINGS_ROW_MAX)
            return;
        const lv_coord_t y = 32 + settings_row_count_ * 28;
        make_settings_row(settings_panel_, settings_row_count_, y, title, value);
        ++settings_row_count_;
    }

    void set_settings_header(const char *title, const char *hint)
    {
        if (settings_header_label_)
            lv_label_set_text(settings_header_label_, title ? title : "");
        if (settings_hint_label_)
            lv_label_set_text(settings_hint_label_, hint ? hint : "");
    }

    void render_settings_main()
    {
        settings_view_ = SettingsView::Main;
        clear_settings_rows();
        set_settings_header("ZClaw Settings", "Tab / Esc");
        add_settings_row("Setup", ui_config_.setup_complete ? "Done" : "Run");
        add_settings_row("Authorization", ui_config_.bearer_token.empty() ? "Pair" : "Paired");
        add_settings_row("Providers", "Manage");
        add_settings_row("Agent", ui_config_.agent_alias.c_str());
        add_settings_row("Transport", ui_config_.bearer_token.empty() ? "Webhook" : "WS");
        if (settings_selected_ >= settings_row_count_)
            settings_selected_ = settings_row_count_ - 1;
        if (settings_selected_ < 0)
            settings_selected_ = 0;
        update_settings_selection();
    }

    void render_authorization()
    {
        settings_view_ = SettingsView::Authorization;
        clear_settings_rows();
        set_settings_header("Authorization", "Enter / Esc");
        add_settings_row("Pair Code", "Enter");
        add_settings_row("Token", ui_config_.bearer_token.empty() ? "Missing" : "Saved");
        add_settings_row("Agent", ui_config_.agent_alias.c_str());
        add_settings_row("Approvals", ui_config_.bearer_token.empty() ? "Off" : "WS");
        add_settings_row("Clear Token", "-");
        settings_selected_ = 0;
        update_settings_selection();
    }

    void render_setup()
    {
        settings_view_ = SettingsView::Setup;
        clear_settings_rows();
        clear_setup_progress_objects();
        if (setup_in_flight_) {
            set_settings_header("Quickstart", "Please wait");
            setup_spinner_ = lv_spinner_create(settings_panel_);
            lv_obj_set_size(setup_spinner_, 26, 26);
            lv_obj_set_pos(setup_spinner_, 18, 40);
            lv_spinner_set_anim_params(setup_spinner_, 900, 70);
            lv_obj_set_style_arc_width(setup_spinner_, 3, LV_PART_MAIN);
            lv_obj_set_style_arc_width(setup_spinner_, 3, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(setup_spinner_, lv_color_hex(COLOR_PANEL_LINE), LV_PART_MAIN);
            lv_obj_set_style_arc_color(setup_spinner_, lv_color_hex(COLOR_PURPLE), LV_PART_INDICATOR);

            setup_status_label_ = make_label(settings_panel_, "Preparing Quickstart", 54, 39, 248, 16,
                                             fonts_.font_12(), COLOR_TEXT);
            setup_detail_label_ = make_label(settings_panel_, "Starting...", 54, 58, 248, 14,
                                             fonts_.font_10(), COLOR_MUTED);
            setup_progress_bar_ = lv_bar_create(settings_panel_);
            lv_obj_set_pos(setup_progress_bar_, 18, 88);
            lv_obj_set_size(setup_progress_bar_, 284, 10);
            lv_bar_set_range(setup_progress_bar_, 0, 100);
            lv_bar_set_value(setup_progress_bar_, 0, LV_ANIM_OFF);
            lv_obj_set_style_radius(setup_progress_bar_, 3, LV_PART_MAIN);
            lv_obj_set_style_radius(setup_progress_bar_, 3, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(setup_progress_bar_, lv_color_hex(COLOR_PANEL), LV_PART_MAIN);
            lv_obj_set_style_bg_color(setup_progress_bar_, lv_color_hex(COLOR_PURPLE), LV_PART_INDICATOR);
            setup_percent_label_ = make_label(settings_panel_, "0%", 18, 105, 70, 14,
                                              fonts_.font_10(), COLOR_TEXT);
            setup_speed_label_ = make_label(settings_panel_, "", 92, 105, 210, 14,
                                            fonts_.font_10(), COLOR_MUTED, LV_TEXT_ALIGN_RIGHT);
            settings_selected_ = 0;
            return;
        }
        set_settings_header("Model Settings", "Enter / Esc");
        add_settings_row("Provider", provider_display_name(setup_provider_.family));
        if (setup_provider_.family == "custom")
            add_settings_row("API URL", setup_provider_.uri.empty() ? "Required" : "Set");
        if (setup_provider_.family != "ollama")
            add_settings_row("API Key", setup_provider_.api_key.empty() ? "Required" : "Set");
        if (setup_provider_.family == "custom")
            add_settings_row("API Model", setup_provider_.model.empty() ? "Required" : "Set");
        add_settings_row("Confirm", "Quickstart");
        if (settings_selected_ >= settings_row_count_)
            settings_selected_ = settings_row_count_ - 1;
        if (settings_selected_ < 0)
            settings_selected_ = 0;
        update_settings_selection();
    }

    void clear_setup_progress_objects()
    {
        lv_obj_t **objects[] = {&setup_spinner_, &setup_status_label_, &setup_detail_label_,
                                &setup_progress_bar_, &setup_percent_label_, &setup_speed_label_};
        for (lv_obj_t **object : objects) {
            if (*object)
                lv_obj_del(*object);
            *object = nullptr;
        }
    }

    static std::string format_transfer_detail(const ZClawSetupProgress &progress)
    {
        if (progress.total_bytes == 0)
            return progress.status;
        char buffer[96];
        const double downloaded_mb = (double)progress.downloaded_bytes / (1024.0 * 1024.0);
        const double total_mb = (double)progress.total_bytes / (1024.0 * 1024.0);
        std::snprintf(buffer, sizeof(buffer), "%.1f / %.1f MB", downloaded_mb, total_mb);
        return buffer;
    }

    static std::string format_transfer_speed(double bytes_per_second)
    {
        if (bytes_per_second <= 0.0)
            return "Waiting for data";
        char buffer[64];
        if (bytes_per_second >= 1024.0 * 1024.0)
            std::snprintf(buffer, sizeof(buffer), "%.1f MB/s", bytes_per_second / (1024.0 * 1024.0));
        else
            std::snprintf(buffer, sizeof(buffer), "%.0f KB/s", bytes_per_second / 1024.0);
        return buffer;
    }

    void update_setup_progress(const ZClawSetupProgress &progress)
    {
        if (!setup_in_flight_ || !setup_progress_bar_)
            return;
        const int percent = progress.percent < 0 ? 0 : (progress.percent > 100 ? 100 : progress.percent);
        lv_label_set_text(setup_status_label_, progress.status.c_str());
        const std::string detail = format_transfer_detail(progress);
        lv_label_set_text(setup_detail_label_, detail.c_str());
        lv_bar_set_value(setup_progress_bar_, percent, LV_ANIM_ON);
        const std::string percent_text = std::to_string(percent) + "%";
        lv_label_set_text(setup_percent_label_, percent_text.c_str());
        const std::string speed = progress.total_bytes > 0 ?
                                  format_transfer_speed(progress.bytes_per_second) : "";
        lv_label_set_text(setup_speed_label_, speed.c_str());
    }

    void render_setup_providers()
    {
        settings_view_ = SettingsView::SetupProviders;
        clear_settings_rows();
        set_settings_header("Model Provider", "Enter / Esc");
        if (setup_provider_selected_ < setup_provider_scroll_)
            setup_provider_scroll_ = setup_provider_selected_;
        if (setup_provider_selected_ >= setup_provider_scroll_ + SETTINGS_ROW_MAX)
            setup_provider_scroll_ = setup_provider_selected_ - SETTINGS_ROW_MAX + 1;
        for (int i = 0; i < SETTINGS_ROW_MAX && setup_provider_scroll_ + i < 6; ++i) {
            const int item = setup_provider_scroll_ + i;
            const ProviderConfig preset = provider_preset(item);
            add_settings_row(provider_display_name(preset.family),
                             item == setup_provider_selected_ ? "Selected" : "");
        }
        settings_selected_ = setup_provider_selected_ - setup_provider_scroll_;
        update_settings_selection();
    }

    int setup_initialize_row() const
    {
        return settings_row_count_ - 1;
    }

    SetupEditField selected_setup_field() const
    {
        if (settings_selected_ == 0)
            return SetupEditField::None;
        int row = 1;
        if (setup_provider_.family == "custom") {
            if (settings_selected_ == row++) return SetupEditField::Uri;
        }
        if (setup_provider_.family != "ollama") {
            if (settings_selected_ == row++) return SetupEditField::ApiKey;
        }
        if (setup_provider_.family == "custom" && settings_selected_ == row)
            return SetupEditField::Model;
        return SetupEditField::None;
    }

    void edit_selected_setup_field()
    {
        setup_edit_field_ = selected_setup_field();
        const char *title = "";
        const std::string *value = nullptr;
        if (setup_edit_field_ == SetupEditField::Uri) {
            title = "API URL";
            value = &setup_provider_.uri;
        } else if (setup_edit_field_ == SetupEditField::ApiKey) {
            title = "API Key";
            value = &setup_provider_.api_key;
        } else if (setup_edit_field_ == SetupEditField::Model) {
            title = "API Model Name";
            value = &setup_provider_.model;
        }
        if (!value)
            return;
        open_text_dialog(title, value->c_str(), InputMode::SetupEdit);
    }

    void apply_setup_edit(const std::string &value)
    {
        const ProviderConfig previous = setup_provider_;
        if (setup_edit_field_ == SetupEditField::Uri)
            setup_provider_.uri = value;
        else if (setup_edit_field_ == SetupEditField::ApiKey)
            setup_provider_.api_key = value;
        else if (setup_edit_field_ == SetupEditField::Model)
            setup_provider_.model = value;
        setup_edit_field_ = SetupEditField::None;
        if (!save_setup_provider())
            setup_provider_ = previous;
        render_setup();
    }

    bool validate_setup(std::string *error)
    {
        if (setup_provider_.family != "ollama" && setup_provider_.api_key.empty()) {
            settings_selected_ = setup_provider_.family == "custom" ? 2 : 1;
            *error = "API Key is required.";
            return false;
        }
        if (setup_provider_.family == "custom") {
            if (setup_provider_.uri.rfind("http://", 0) != 0 && setup_provider_.uri.rfind("https://", 0) != 0) {
                settings_selected_ = 1;
                *error = "API URL must start with http:// or https://.";
                return false;
            }
            if (setup_provider_.model.empty()) {
                settings_selected_ = 3;
                *error = "API Model Name is required.";
                return false;
            }
        }
        return true;
    }

    void render_settings_providers()
    {
        settings_view_ = SettingsView::Providers;
        clear_settings_rows();
        set_settings_header("Providers", "Enter / Esc");

        if (provider_selected_ < 0)
            provider_selected_ = 0;
        const int total_rows = (int)providers_.size() + 1;
        if (provider_selected_ >= total_rows)
            provider_selected_ = total_rows - 1;
        if (provider_selected_ < provider_scroll_)
            provider_scroll_ = provider_selected_;
        if (provider_selected_ >= provider_scroll_ + SETTINGS_ROW_MAX)
            provider_scroll_ = provider_selected_ - SETTINGS_ROW_MAX + 1;
        if (provider_scroll_ < 0)
            provider_scroll_ = 0;

        for (int i = 0; i < SETTINGS_ROW_MAX && provider_scroll_ + i < total_rows; ++i) {
            const int item = provider_scroll_ + i;
            if (item == 0) {
                add_settings_row("Add Provider", "+");
            } else {
                const ProviderConfig &provider = providers_[item - 1];
                add_settings_row(provider.alias.c_str(), provider.model.c_str());
            }
        }

        settings_selected_ = provider_selected_ - provider_scroll_;
        update_settings_selection();
    }

    void render_provider_detail()
    {
        settings_view_ = SettingsView::ProviderDetail;
        clear_settings_rows();
        set_settings_header("Provider", "Enter / Del");

        if (provider_detail_index_ < 0 || provider_detail_index_ >= (int)providers_.size()) {
            render_settings_providers();
            return;
        }

        ProviderConfig &provider = providers_[provider_detail_index_];
        add_settings_row("Alias", provider.alias.c_str());
        add_settings_row("Family", provider.family.c_str());
        add_settings_row("Model", provider.model.c_str());
        add_settings_row("URI", provider.uri.c_str());
        add_settings_row("API Key", provider.api_key.empty() ? "(empty)" : "set");
        if (settings_selected_ >= settings_row_count_)
            settings_selected_ = settings_row_count_ - 1;
        if (settings_selected_ < 0)
            settings_selected_ = 0;
        update_settings_selection();
    }

    void create_settings_panel()
    {
        settings_panel_ = make_box(screen_, SCREEN_W, 0, SCREEN_W, SCREEN_H, COLOR_BG);
        lv_obj_move_foreground(settings_panel_);

        static constexpr lv_coord_t BAR_H = 20;
        lv_obj_t *bar = make_box(settings_panel_, 0, 0, SCREEN_W, BAR_H, COLOR_BAR);
        settings_header_label_ = make_label(bar, "ZClaw Settings", 12, 4, 160, 12,
                                            fonts_.font_12(), COLOR_TEXT);
        settings_hint_label_ = make_label(bar, "Tab / Esc", 214, 5, 94, 10,
                                          fonts_.font_10(), COLOR_DIM, LV_TEXT_ALIGN_RIGHT);

        settings_selected_ = 0;
        render_settings_main();
    }

    void sync_setup_provider_selection()
    {
        if (setup_provider_.family == "openrouter") setup_provider_selected_ = 1;
        else if (setup_provider_.family == "anthropic") setup_provider_selected_ = 2;
        else if (setup_provider_.family == "ollama") setup_provider_selected_ = 3;
        else if (setup_provider_.family == "deepseek") setup_provider_selected_ = 4;
        else if (setup_provider_.family == "custom") setup_provider_selected_ = 5;
        else setup_provider_selected_ = 0;
        setup_provider_scroll_ = 0;
    }

    void open_setup_panel()
    {
        if (settings_panel_open() || settings_animating_)
            return;
        close_input_dialog();
        sync_setup_provider_selection();
        create_settings_panel();
        if (first_run_needed())
            render_setup_providers();
        else
            render_setup();
        animate_settings_panel(SCREEN_W, 0, false);
    }

    static void static_settings_anim_done(lv_anim_t *a)
    {
        ZClawApp *self = static_cast<ZClawApp *>(lv_anim_get_user_data(a));
        if (self)
            self->settings_anim_done();
    }

    void settings_anim_done()
    {
        settings_animating_ = false;
        if (settings_closing_) {
            if (settings_panel_) {
                lv_obj_del(settings_panel_);
                settings_panel_ = nullptr;
                settings_header_label_ = nullptr;
                settings_hint_label_ = nullptr;
                setup_spinner_ = nullptr;
                setup_status_label_ = nullptr;
                setup_detail_label_ = nullptr;
                setup_progress_bar_ = nullptr;
                setup_percent_label_ = nullptr;
                setup_speed_label_ = nullptr;
                for (auto *&row : settings_rows_)
                    row = nullptr;
                for (auto *&value : settings_values_)
                    value = nullptr;
            }
            settings_closing_ = false;
        }
    }

    void animate_settings_panel(lv_coord_t from_x, lv_coord_t to_x, bool closing)
    {
        if (!settings_panel_)
            return;

        settings_animating_ = true;
        settings_closing_ = closing;
        lv_anim_del(settings_panel_, nullptr);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, settings_panel_);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_set_values(&a, from_x, to_x);
        lv_anim_set_time(&a, 200);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_completed_cb(&a, static_settings_anim_done);
        lv_anim_set_user_data(&a, this);
        lv_anim_start(&a);
    }

    bool settings_panel_open() const
    {
        return settings_panel_ != nullptr;
    }

    void open_settings_panel()
    {
        if (settings_panel_open() || settings_animating_)
            return;
        close_input_dialog();
        create_settings_panel();
        animate_settings_panel(SCREEN_W, 0, false);
    }

    void close_settings_panel()
    {
        if (!settings_panel_open() || settings_animating_)
            return;
        animate_settings_panel(lv_obj_get_x(settings_panel_), SCREEN_W, true);
    }

    void move_settings_selection(int delta)
    {
        if (!settings_panel_open())
            return;
        if (settings_view_ == SettingsView::SetupProviders) {
            setup_provider_selected_ += delta;
            if (setup_provider_selected_ < 0)
                setup_provider_selected_ = 0;
            if (setup_provider_selected_ >= 6)
                setup_provider_selected_ = 5;
            render_setup_providers();
            return;
        }
        settings_selected_ += delta;
        if (settings_selected_ < 0)
            settings_selected_ = 0;
        if (settings_selected_ >= settings_row_count_)
            settings_selected_ = settings_row_count_ - 1;
        update_settings_selection();
    }

    lv_obj_t *make_sparkle(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
    {
        lv_obj_t *sparkles = lv_img_create(parent);
        lv_img_set_src(sparkles, sparkles_path_.c_str());
        lv_obj_set_pos(sparkles, x, y);
        lv_obj_clear_flag(sparkles, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        return sparkles;
    }

    lv_obj_t *make_send_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
    {
        lv_obj_t *send = lv_img_create(parent);
        lv_img_set_src(send, send_button_path_.c_str());
        lv_obj_set_pos(send, x, y);
        lv_obj_clear_flag(send, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        return send;
    }

    bool input_dialog_open() const
    {
        return input_dialog_ && input_textarea_;
    }

    void close_input_dialog()
    {
        if (!input_dialog_)
            return;
        lv_obj_del(input_dialog_);
        input_dialog_ = nullptr;
        input_textarea_ = nullptr;
    }

    void close_approval_dialog()
    {
        if (!approval_dialog_)
            return;
        lv_obj_del(approval_dialog_);
        approval_dialog_ = nullptr;
        approval_tool_label_ = nullptr;
        approval_summary_label_ = nullptr;
        for (auto *&button : approval_buttons_)
            button = nullptr;
        approval_selected_ = 0;
    }

    void clear_pending_approval()
    {
        {
            std::lock_guard<std::mutex> lock(approval_mutex_);
            pending_approval_id_.clear();
            pending_approval_tool_.clear();
            pending_approval_decision_.clear();
            approval_waiting_ = false;
        }
        approval_cv_.notify_all();
        close_approval_dialog();
    }

    void clear_obj(lv_obj_t *&obj)
    {
        if (!obj)
            return;
        lv_obj_del(obj);
        obj = nullptr;
    }

    lv_obj_t *make_chat_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                              lv_coord_t w, lv_coord_t h, uint32_t color,
                              lv_text_align_t align = LV_TEXT_ALIGN_LEFT)
    {
        lv_obj_t *label = make_label(parent, text, x, y, w, h, fonts_.font_12(), color, align);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        return label;
    }

    static lv_point_t measure_text_box(const char *text, const lv_font_t *font, lv_coord_t max_w)
    {
        lv_point_t size{};
        lv_text_get_size(&size, text ? text : "", font, 0, 0, max_w, LV_TEXT_FLAG_NONE);
        return size;
    }

    lv_obj_t *make_message_row(lv_coord_t h, lv_flex_align_t main_align)
    {
        lv_obj_t *row = lv_obj_create(chat_container_);
        lv_obj_set_size(row, CHAT_ROW_W, h);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_column(row, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(row, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        return row;
    }

    lv_obj_t *make_bubble(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, uint32_t color, bool user)
    {
        lv_obj_t *bubble = make_box(parent, 0, 0, w, h, color, 10);
        if (user)
            apply_vertical_gradient(bubble, COLOR_PURPLE, COLOR_INDIGO);
        return bubble;
    }

    void scroll_chat_to_bottom()
    {
        if (chat_container_) {
            lv_obj_scroll_to_y(chat_container_, LV_COORD_MAX, LV_ANIM_ON);
            update_scrollbar();
        }
    }

    void append_ai_message(const char *text)
    {
        static constexpr lv_coord_t bubble_w = 190;
        static constexpr lv_coord_t text_w = 168;
        static constexpr lv_coord_t pad_x = 10;
        static constexpr lv_coord_t pad_y = 6;
        const std::string display_text = display_text_compat(text ? text : "");
        const lv_point_t text_size = measure_text_box(display_text.c_str(), fonts_.font_12(), text_w);
        lv_coord_t bubble_h = text_size.y + pad_y * 2;
        if (bubble_h < 41)
            bubble_h = 41;

        lv_obj_t *row = make_message_row(bubble_h, LV_FLEX_ALIGN_START);
        make_zclaw_avatar(row, 0, 0, 16);
        reply_bubble_ = make_bubble(row, bubble_w, bubble_h, COLOR_PANEL, false);
        reply_label_ = make_chat_label(reply_bubble_, display_text.c_str(), pad_x, pad_y, text_w,
                                       bubble_h - pad_y * 2, 0xFFFFFF);
        scroll_chat_to_bottom();
    }

    void append_user_message(const std::string &text)
    {
        const std::string display_text = display_text_compat(text);
        const lv_coord_t text_max_w = USER_BUBBLE_MAX_W - USER_BUBBLE_PAD_X * 2;
        const lv_point_t text_size = measure_text_box(display_text.c_str(), fonts_.font_12(), text_max_w);
        lv_coord_t bubble_w = text_size.x + USER_BUBBLE_PAD_X * 2;
        lv_coord_t bubble_h = text_size.y + USER_BUBBLE_PAD_Y * 2;
        if (bubble_w < USER_BUBBLE_MIN_W)
            bubble_w = USER_BUBBLE_MIN_W;
        if (bubble_w > USER_BUBBLE_MAX_W)
            bubble_w = USER_BUBBLE_MAX_W;
        if (bubble_h < 27)
            bubble_h = 27;

        lv_obj_t *row = make_message_row(bubble_h, LV_FLEX_ALIGN_END);
        user_bubble_ = make_bubble(row, bubble_w, bubble_h, COLOR_INDIGO, true);
        user_label_ = make_chat_label(user_bubble_, display_text.c_str(), USER_BUBBLE_PAD_X, USER_BUBBLE_PAD_Y,
                                      bubble_w - USER_BUBBLE_PAD_X * 2,
                                      bubble_h - USER_BUBBLE_PAD_Y * 2,
                                      0xFFFFFF, LV_TEXT_ALIGN_LEFT);
        scroll_chat_to_bottom();
    }

    void open_input_dialog()
    {
        if (input_dialog_open())
            return;

        static constexpr lv_coord_t dialog_h = SCREEN_H * 2 / 3;
        input_dialog_ = lv_msgbox_create(lv_layer_top());
        lv_obj_set_size(input_dialog_, 300, dialog_h);
        lv_obj_align(input_dialog_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_radius(input_dialog_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(input_dialog_, lv_color_hex(COLOR_BAR), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(input_dialog_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(input_dialog_, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(input_dialog_, lv_color_hex(COLOR_PANEL_LINE), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(input_dialog_, lv_color_hex(COLOR_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(input_dialog_, fonts_.font_10(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(input_dialog_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *content = lv_msgbox_get_content(input_dialog_);
        lv_obj_set_style_pad_all(content, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

        input_textarea_ = lv_textarea_create(content);
        lv_obj_set_size(input_textarea_, 290, dialog_h - 10);
        lv_textarea_set_placeholder_text(input_textarea_, "Type your message...");
        lv_textarea_set_one_line(input_textarea_, false);
        lv_textarea_set_cursor_click_pos(input_textarea_, false);
        lv_obj_set_style_radius(input_textarea_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(input_textarea_, lv_color_hex(COLOR_PANEL), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(input_textarea_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(input_textarea_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(input_textarea_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(input_textarea_, fonts_.font_10(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(input_textarea_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        if (!textarea_cursor_style_inited_) {
            lv_style_init(&textarea_cursor_style_);
            lv_style_set_bg_opa(&textarea_cursor_style_, LV_OPA_TRANSP);
            lv_style_set_border_color(&textarea_cursor_style_, lv_color_hex(COLOR_PURPLE));
            lv_style_set_border_side(&textarea_cursor_style_, LV_BORDER_SIDE_LEFT);
            lv_style_set_border_width(&textarea_cursor_style_, 2);
            lv_style_set_pad_left(&textarea_cursor_style_, -2);
            lv_style_set_pad_right(&textarea_cursor_style_, 0);
            textarea_cursor_style_inited_ = true;
        }
        lv_obj_add_style(input_textarea_, &textarea_cursor_style_, LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_add_state(input_textarea_, LV_STATE_FOCUSED);
        lv_obj_send_event(input_textarea_, LV_EVENT_FOCUSED, nullptr);
        lv_textarea_set_cursor_pos(input_textarea_, LV_TEXTAREA_CURSOR_LAST);
    }

    void open_text_dialog(const char *placeholder, const char *initial_text, InputMode mode)
    {
        input_mode_ = mode;
        open_input_dialog();
        if (!input_dialog_open())
            return;
        lv_textarea_set_placeholder_text(input_textarea_, placeholder ? placeholder : "");
        lv_textarea_set_text(input_textarea_, initial_text ? initial_text : "");
        lv_textarea_set_cursor_pos(input_textarea_, LV_TEXTAREA_CURSOR_LAST);
    }

    void show_sent_message(const std::string &text)
    {
        append_user_message(text);
    }

    void publish_approval_prompt(const ZClawApprovalRequest &request)
    {
        {
            std::lock_guard<std::mutex> lock(approval_mutex_);
            pending_approval_id_ = request.request_id;
            pending_approval_tool_ = request.tool;
        }

        close_approval_dialog();
        approval_dialog_ = make_box(lv_layer_top(), 42, 30, 236, 110, COLOR_BAR, 8);
        lv_obj_set_style_border_width(approval_dialog_, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(approval_dialog_, lv_color_hex(COLOR_PURPLE), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_move_foreground(approval_dialog_);

        make_box(approval_dialog_, 0, 0, 236, 20, COLOR_PANEL, 8);
        make_label(approval_dialog_, "Permission", 10, 5, 120, 12, fonts_.font_12(), COLOR_TEXT);
        make_label(approval_dialog_, "Esc", 194, 6, 28, 10, fonts_.font_10(), COLOR_DIM, LV_TEXT_ALIGN_RIGHT);

        approval_tool_label_ = make_label(approval_dialog_, request.tool.c_str(), 12, 27, 212, 14,
                                          fonts_.font_12(), COLOR_TEXT);
        approval_summary_label_ = make_label(approval_dialog_, request.summary.c_str(), 12, 45, 212, 30,
                                             fonts_.font_10(), COLOR_MUTED);
        make_box(approval_dialog_, 0, 78, 236, 1, COLOR_PANEL_LINE);

        static constexpr const char *labels[] = {"Yes", "Always", "No"};
        static constexpr uint32_t colors[] = {COLOR_ONLINE, COLOR_PURPLE, COLOR_DIM};
        for (int i = 0; i < 3; ++i) {
            approval_buttons_[i] = make_box(approval_dialog_, 12 + i * 72, 86, 68, 17, COLOR_PANEL, 5);
            lv_obj_set_style_border_width(approval_buttons_[i], 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            make_label(approval_buttons_[i], labels[i], 0, 4, 68, 10,
                       fonts_.font_10(), colors[i], LV_TEXT_ALIGN_CENTER);
        }
        approval_selected_ = 0;
        update_approval_buttons();
    }

    void update_approval_buttons()
    {
        for (int i = 0; i < 3; ++i) {
            if (!approval_buttons_[i])
                continue;
            const bool selected = i == approval_selected_;
            lv_obj_set_style_bg_color(approval_buttons_[i],
                                      lv_color_hex(selected ? 0x2B2B4A : COLOR_PANEL),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(approval_buttons_[i],
                                          lv_color_hex(selected ? COLOR_TEXT : COLOR_PANEL_LINE),
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void move_approval_selection(int delta)
    {
        approval_selected_ += delta;
        if (approval_selected_ < 0)
            approval_selected_ = 2;
        if (approval_selected_ > 2)
            approval_selected_ = 0;
        update_approval_buttons();
    }

    const char *selected_approval_decision() const
    {
        if (approval_selected_ == 1)
            return "always";
        if (approval_selected_ == 2)
            return "deny";
        return "approve";
    }

    static void approval_prompt_async(void *data)
    {
        ApprovalPromptData *prompt = static_cast<ApprovalPromptData *>(data);
        if (!prompt)
            return;
        if (prompt->self)
            prompt->self->publish_approval_prompt(prompt->request);
        delete prompt;
    }

    void post_approval_prompt(const ZClawApprovalRequest &request)
    {
        ApprovalPromptData *prompt = new ApprovalPromptData();
        prompt->self = this;
        prompt->request = request;
        lv_async_call(approval_prompt_async, prompt);
    }

    static void approval_close_async(void *data)
    {
        ZClawApp *self = static_cast<ZClawApp *>(data);
        if (self)
            self->clear_pending_approval();
    }

    void post_approval_close()
    {
        lv_async_call(approval_close_async, this);
    }

    bool answer_pending_approval(const char *decision)
    {
        {
            std::lock_guard<std::mutex> lock(approval_mutex_);
            if (pending_approval_id_.empty())
                return false;
            pending_approval_decision_ = decision ? decision : "deny";
            approval_waiting_ = false;
            pending_approval_id_.clear();
            pending_approval_tool_.clear();
        }
        approval_cv_.notify_all();
        close_approval_dialog();
        return true;
    }

    std::string await_approval_decision(const ZClawApprovalRequest &request)
    {
        const int timeout_secs = request.timeout_secs <= 0 ? 120 : request.timeout_secs;
        {
            std::lock_guard<std::mutex> lock(approval_mutex_);
            approval_waiting_ = true;
            pending_approval_decision_.clear();
        }
        post_approval_prompt(request);

        std::unique_lock<std::mutex> lock(approval_mutex_);
        if (timeout_secs <= 0)
            return "deny";
        const bool answered = approval_cv_.wait_for(
            lock, std::chrono::seconds(timeout_secs),
            [&] { return !approval_waiting_; });
        std::string decision = answered ? pending_approval_decision_ : "deny";
        approval_waiting_ = false;
        pending_approval_decision_.clear();
        pending_approval_id_.clear();
        pending_approval_tool_.clear();
        lock.unlock();
        if (!answered) {
            post_approval_close();
        }
        return decision.empty() ? "deny" : decision;
    }

    static void chat_async_done(void *data)
    {
        AsyncResult *result = static_cast<AsyncResult *>(data);
        if (!result)
            return;
        if (result->self)
            result->self->finish_chat_result(result->text);
        delete result;
    }

    static void *chat_thread_main(void *data)
    {
        AsyncResult *result = static_cast<AsyncResult *>(data);
        if (!result || !result->self)
            return nullptr;
        ZClawClientResult client_result = result->self->client_.send_chat(
            result->self->ui_config_, result->text,
            [self = result->self](const ZClawApprovalRequest &request) {
                return self->await_approval_decision(request);
            });
        result->text = client_result.text;
        result->ok = client_result.ok;
        result->config = client_result.config;
        lv_async_call(chat_async_done, result);
        return nullptr;
    }

    void request_webhook_reply(const std::string &message)
    {
        if (request_in_flight_) {
            append_ai_message("I am still waiting for ZeroClaw.");
            return;
        }
        if (first_run_needed()) {
            append_ai_message("Run quickstart before chatting.");
            open_setup_panel();
            return;
        }

        request_in_flight_ = true;
        append_ai_message("Thinking...");
        AsyncResult *result = new AsyncResult();
        result->self = this;
        result->text = message;
        pthread_t thread_id{};
        if (pthread_create(&thread_id, nullptr, chat_thread_main, result) != 0) {
            delete result;
            request_in_flight_ = false;
            append_ai_message("Could not start webhook request.");
            return;
        }
        pthread_detach(thread_id);
    }

    void finish_chat_result(const std::string &text)
    {
        request_in_flight_ = false;
        append_ai_message(text.c_str());
    }

    static void pairing_async_done(void *data)
    {
        AsyncResult *result = static_cast<AsyncResult *>(data);
        if (!result)
            return;
        if (result->self)
            result->self->finish_pairing_result(result->ok, result->text, result->config);
        delete result;
    }

    static void *pairing_thread_main(void *data)
    {
        AsyncResult *result = static_cast<AsyncResult *>(data);
        if (!result || !result->self)
            return nullptr;
        ZClawClientResult client_result = result->self->client_.pair_with_code(
            result->self->ui_config_, result->text);
        result->text = client_result.text;
        result->ok = client_result.ok;
        result->config = client_result.config;
        lv_async_call(pairing_async_done, result);
        return nullptr;
    }

    void start_pairing(const std::string &code)
    {
        append_ai_message("Pairing with ZeroClaw...");
        AsyncResult *result = new AsyncResult();
        result->self = this;
        result->text = code;
        pthread_t thread_id{};
        if (pthread_create(&thread_id, nullptr, pairing_thread_main, result) != 0) {
            delete result;
            append_ai_message("Could not start pairing request.");
            return;
        }
        pthread_detach(thread_id);
    }

    void finish_pairing_result(bool ok, const std::string &text, const UiConfig &config)
    {
        if (ok)
            append_ai_message(text.c_str());
        if (ok) {
            const UiConfig previous = ui_config_;
            ui_config_ = config;
            if (!save_ui_config()) {
                ui_config_ = previous;
            }
        }
        if (!ok)
            append_ai_message(text.c_str());
        if (settings_panel_open() && settings_view_ == SettingsView::Authorization)
            render_authorization();
    }

    void scroll_chat(int delta)
    {
        if (!chat_container_)
            return;

        const int32_t top = lv_obj_get_scroll_top(chat_container_);
        const int32_t bottom = lv_obj_get_scroll_bottom(chat_container_);
        if ((delta > 0 && top <= 0) || (delta < 0 && bottom <= 0))
            return;

        int32_t applied = delta;
        if (delta > top)
            applied = top;
        if (-delta > bottom)
            applied = -bottom;
        lv_obj_scroll_by(chat_container_, 0, applied, LV_ANIM_ON);
        update_scrollbar();
    }

    void update_scrollbar()
    {
        if (!chat_container_ || !scroll_track_ || !scroll_thumb_)
            return;

        static constexpr lv_coord_t track_y = 20;
        static constexpr lv_coord_t track_h = 126;
        const int32_t top = lv_obj_get_scroll_top(chat_container_);
        const int32_t bottom = lv_obj_get_scroll_bottom(chat_container_);
        const int32_t range = top + bottom;
        if (range <= 0) {
            lv_obj_set_pos(scroll_thumb_, 312, track_y);
            lv_obj_set_size(scroll_thumb_, 3, track_h);
            return;
        }

        lv_coord_t thumb_h = (lv_coord_t)((int32_t)track_h * track_h / (track_h + range));
        if (thumb_h < 18)
            thumb_h = 18;
        if (thumb_h > track_h)
            thumb_h = track_h;

        const lv_coord_t travel = track_h - thumb_h;
        const lv_coord_t thumb_y = track_y + (lv_coord_t)((int32_t)travel * top / range);
        lv_obj_set_pos(scroll_thumb_, 312, thumb_y);
        lv_obj_set_size(scroll_thumb_, 3, thumb_h);
    }

    static void static_chat_scroll_handler(lv_event_t *e)
    {
        ZClawApp *self = static_cast<ZClawApp *>(lv_event_get_user_data(e));
        if (self)
            self->update_scrollbar();
    }

    void add_provider()
    {
        const int next = (int)providers_.size() + 1;
        ProviderConfig provider;
        provider.alias = "provider" + std::to_string(next);
        provider.family = "openai-compatible";
        provider.model = "model";
        provider.uri = "https://api.example.com/v1";
        const std::vector<ProviderConfig> previous = providers_;
        providers_.push_back(provider);
        std::string error;
        if (!save_providers(&error)) {
            providers_ = previous;
            report_save_error(error);
            return;
        }
        provider_selected_ = (int)providers_.size();
        provider_detail_index_ = (int)providers_.size() - 1;
        settings_selected_ = 0;
        render_provider_detail();
    }

    void delete_current_provider()
    {
        if (provider_detail_index_ < 0 || provider_detail_index_ >= (int)providers_.size())
            return;
        const std::vector<ProviderConfig> previous_providers = providers_;
        const ProviderConfig previous_setup = setup_provider_;
        zclaw::erase_provider_config(&providers_, &setup_provider_,
                                     static_cast<size_t>(provider_detail_index_), provider_preset(0));
        std::string error;
        if (!save_providers(&error)) {
            providers_ = previous_providers;
            setup_provider_ = previous_setup;
            report_save_error(error);
            return;
        }
        provider_detail_index_ = -1;
        if (provider_selected_ > (int)providers_.size())
            provider_selected_ = (int)providers_.size();
        render_settings_providers();
    }

    ProviderEditField selected_provider_field() const
    {
        switch (settings_selected_) {
        case 0:
            return ProviderEditField::Alias;
        case 1:
            return ProviderEditField::Family;
        case 2:
            return ProviderEditField::Model;
        case 3:
            return ProviderEditField::Uri;
        case 4:
            return ProviderEditField::ApiKey;
        default:
            return ProviderEditField::None;
        }
    }

    void edit_selected_provider_field()
    {
        if (provider_detail_index_ < 0 || provider_detail_index_ >= (int)providers_.size())
            return;
        provider_edit_field_ = selected_provider_field();
        if (provider_edit_field_ == ProviderEditField::None)
            return;

        ProviderConfig &provider = providers_[provider_detail_index_];
        const std::string &value = provider_field_value(provider, provider_edit_field_);
        open_text_dialog(provider_field_name(provider_edit_field_), value.c_str(), InputMode::ProviderEdit);
    }

    void apply_provider_edit(const std::string &value)
    {
        if (provider_detail_index_ < 0 || provider_detail_index_ >= (int)providers_.size())
            return;
        if (provider_edit_field_ == ProviderEditField::None)
            return;

        const std::vector<ProviderConfig> previous_providers = providers_;
        const ProviderConfig previous_setup = setup_provider_;
        ProviderConfig provider = providers_[provider_detail_index_];
        provider_field_value(provider, provider_edit_field_) = value;
        provider_edit_field_ = ProviderEditField::None;
        zclaw::replace_provider_config(&providers_, &setup_provider_,
                                       static_cast<size_t>(provider_detail_index_), provider);
        std::string error;
        if (!save_providers(&error)) {
            providers_ = previous_providers;
            setup_provider_ = previous_setup;
            report_save_error(error);
        }
        render_provider_detail();
    }

    static void setup_async_done(void *data)
    {
        AsyncResult *result = static_cast<AsyncResult *>(data);
        if (!result)
            return;
        if (result->self)
            result->self->finish_setup_result(result->ok, result->text, result->config);
        delete result;
    }

    static void setup_progress_async(void *data)
    {
        SetupProgressData *update = static_cast<SetupProgressData *>(data);
        if (!update)
            return;
        if (update->self)
            update->self->update_setup_progress(update->progress);
        delete update;
    }

    void post_setup_progress(const ZClawSetupProgress &progress)
    {
        SetupProgressData *update = new SetupProgressData();
        update->self = this;
        update->progress = progress;
        lv_async_call(setup_progress_async, update);
    }

    static void *setup_thread_main(void *data)
    {
        AsyncResult *result = static_cast<AsyncResult *>(data);
        if (!result || !result->self)
            return nullptr;
        ZClawClientResult client_result = result->self->client_.run_setup(
            result->self->ui_config_, result->self->setup_provider_,
            [self = result->self](const ZClawSetupProgress &progress) {
                self->post_setup_progress(progress);
            });
        result->text = client_result.text;
        result->ok = client_result.ok;
        result->config = client_result.config;
        lv_async_call(setup_async_done, result);
        return nullptr;
    }

    void start_setup()
    {
        if (setup_in_flight_)
            return;
        std::string validation_error;
        if (!validate_setup(&validation_error)) {
            append_ai_message(validation_error.c_str());
            render_setup();
            return;
        }
        setup_in_flight_ = true;
        append_ai_message("Configuring ZeroClaw service...");
        render_setup();
        AsyncResult *result = new AsyncResult();
        result->self = this;
        pthread_t thread_id{};
        if (pthread_create(&thread_id, nullptr, setup_thread_main, result) != 0) {
            delete result;
            setup_in_flight_ = false;
            append_ai_message("Could not start setup thread.");
            render_setup();
            return;
        }
        pthread_detach(thread_id);
    }

    void finish_setup_result(bool ok, const std::string &text, const UiConfig &config)
    {
        setup_in_flight_ = false;
        bool persisted = ok;
        if (ok)
            append_ai_message(text.c_str());
        if (ok) {
            const UiConfig previous = ui_config_;
            ui_config_ = config;
            if (!save_setup_provider() || !save_ui_config()) {
                ui_config_ = previous;
                persisted = false;
            }
        }
        if (!ok)
            append_ai_message(text.c_str());
        if (persisted && settings_panel_open() && settings_view_ == SettingsView::Setup) {
            render_setup();
            close_settings_panel();
        }
    }

    void activate_settings_selection()
    {
        if (!settings_panel_open())
            return;

        if (settings_view_ == SettingsView::Setup) {
            if (setup_in_flight_)
                return;
            if (settings_selected_ == 0) {
                sync_setup_provider_selection();
                render_setup_providers();
            } else if (settings_selected_ == setup_initialize_row()) {
                start_setup();
            } else {
                edit_selected_setup_field();
            }
            return;
        }

        if (settings_view_ == SettingsView::SetupProviders) {
            const std::vector<ProviderConfig> previous_providers = providers_;
            const ProviderConfig previous_setup = setup_provider_;
            const ProviderConfig selected = provider_preset(setup_provider_selected_);
            zclaw::activate_provider_config(&providers_, &setup_provider_, selected);
            std::string error;
            if (!save_providers(&error)) {
                providers_ = previous_providers;
                setup_provider_ = previous_setup;
                report_save_error(error);
            }
            settings_selected_ = 0;
            render_setup();
            return;
        }

        if (settings_view_ == SettingsView::Authorization) {
            if (settings_selected_ == 0) {
                open_text_dialog("Pairing code", "", InputMode::PairingCode);
            } else if (settings_selected_ == 4) {
                const UiConfig previous = ui_config_;
                ui_config_.bearer_token.clear();
                if (save_ui_config())
                    append_ai_message("Authorization token cleared.");
                else
                    ui_config_ = previous;
                render_authorization();
            }
            return;
        }

        if (settings_view_ == SettingsView::Main) {
            if (settings_selected_ == 0) {
                render_setup();
            } else if (settings_selected_ == 1) {
                render_authorization();
            } else if (settings_selected_ == 2) {
                provider_selected_ = 0;
                provider_scroll_ = 0;
                render_settings_providers();
            }
            return;
        }

        if (settings_view_ == SettingsView::Providers) {
            const int item = provider_scroll_ + settings_selected_;
            if (item == 0) {
                add_provider();
            } else if (item - 1 < (int)providers_.size()) {
                provider_detail_index_ = item - 1;
                settings_selected_ = 0;
                render_provider_detail();
            }
            return;
        }

        if (settings_view_ == SettingsView::ProviderDetail)
            edit_selected_provider_field();
    }

    void settings_back()
    {
        if (setup_in_flight_ && settings_view_ == SettingsView::Setup)
            return;
        if (settings_view_ == SettingsView::SetupProviders) {
            if (first_run_needed())
                g_quit_requested = 1;
            else {
                settings_selected_ = 0;
                render_setup();
            }
        } else if (settings_view_ == SettingsView::Setup) {
            if (first_run_needed()) {
                render_setup_providers();
            } else {
                close_settings_panel();
            }
        } else if (settings_view_ == SettingsView::Authorization) {
            close_settings_panel();
        } else if (settings_view_ == SettingsView::ProviderDetail) {
            provider_detail_index_ = -1;
            render_settings_providers();
        } else if (settings_view_ == SettingsView::Providers) {
            settings_selected_ = 0;
            render_settings_main();
        } else {
            close_settings_panel();
        }
    }

    void send_current_input()
    {
        if (!input_dialog_open())
            return;

        const char *text = lv_textarea_get_text(input_textarea_);
        if (input_mode_ == InputMode::SetupEdit) {
            const std::string value = text ? text : "";
            close_input_dialog();
            apply_setup_edit(value);
            input_mode_ = InputMode::Chat;
            return;
        }
        if (input_mode_ == InputMode::ProviderEdit) {
            const std::string value = text ? text : "";
            close_input_dialog();
            apply_provider_edit(value);
            input_mode_ = InputMode::Chat;
            return;
        }

        if (input_mode_ == InputMode::PairingCode) {
            const std::string value = text ? text : "";
            close_input_dialog();
            input_mode_ = InputMode::Chat;
            if (!value.empty())
                start_pairing(value);
            return;
        }

        if (!text || !text[0]) {
            close_input_dialog();
            return;
        }

        const std::string sent = text;
        close_input_dialog();
        show_sent_message(sent);
        request_webhook_reply(sent);
    }

    void append_input(const char *utf8)
    {
        if (!input_dialog_open() || !utf8 || !utf8[0])
            return;
        const std::string display_text = display_text_compat(utf8);
        lv_textarea_add_text(input_textarea_, display_text.c_str());
    }

    void event_handler_init()
    {
        lv_obj_add_event_cb(root_screen_, ZClawApp::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        ZClawApp *self = static_cast<ZClawApp *>(lv_event_get_user_data(e));
        if (self)
            self->event_handler(e);
    }

    void event_handler(lv_event_t *e)
    {
        if (lv_event_get_code(e) != static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD))
            return;

        const struct key_item *item = keyboard_item(e);
        if (!item)
            return;

        uint32_t key = keyboard_key(e);
        if (startup_state_ != StartupState::Ready) {
            if (startup_state_ == StartupState::Offline &&
                item->key_state == KBD_KEY_RELEASED && key == KEY_ENTER)
                g_quit_requested = 1;
            return;
        }
        const bool shift_down = (item->mods & KBD_MOD_SHIFT) != 0;
        if (item->key_state == KBD_KEY_PRESSED || item->key_state == KBD_KEY_REPEATED) {
            if (!input_dialog_open())
                return;

            switch (key) {
            case KEY_ENTER:
                if (shift_down)
                    lv_textarea_add_char(input_textarea_, '\n');
                break;
            case KEY_BACKSPACE:
                lv_textarea_delete_char(input_textarea_);
                break;
            case KEY_DELETE:
                lv_textarea_delete_char_forward(input_textarea_);
                break;
            case KEY_LEFT:
                lv_textarea_cursor_left(input_textarea_);
                break;
            case KEY_RIGHT:
                lv_textarea_cursor_right(input_textarea_);
                break;
            case KEY_UP:
                lv_textarea_cursor_up(input_textarea_);
                break;
            case KEY_DOWN:
                lv_textarea_cursor_down(input_textarea_);
                break;
            default:
                if (item->utf8[0] && (unsigned char)item->utf8[0] >= 0x20)
                    append_input(item->utf8);
                break;
            }
            return;
        }

        if (item->key_state != KBD_KEY_RELEASED)
            return;

        if (input_dialog_open()) {
            if (key == KEY_ESC) {
                close_input_dialog();
                input_mode_ = InputMode::Chat;
                return;
            }
            if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT)
                return;
            if (key == KEY_ENTER) {
                if (shift_down)
                    return;
                send_current_input();
                return;
            }
            return;
        }

        if (key == KEY_Z)
            key = KEY_LEFT;
        else if (key == KEY_X)
            key = KEY_DOWN;
        else if (key == KEY_C)
            key = KEY_RIGHT;
        else if (key == KEY_F)
            key = KEY_UP;

        {
            std::lock_guard<std::mutex> lock(approval_mutex_);
            if (!pending_approval_id_.empty()) {
                if (key == KEY_ENTER || key == KEY_Y || key == KEY_A || key == KEY_N) {
                    // Unlock before recording the decision.
                } else if (key == KEY_ESC || key == KEY_BACKSPACE) {
                    // Unlock before recording the decision.
                } else if (key == KEY_LEFT || key == KEY_RIGHT) {
                    // Unlock before moving visual selection.
                } else {
                    return;
                }
            }
        }
        if (key == KEY_LEFT) {
            move_approval_selection(-1);
            return;
        }
        if (key == KEY_RIGHT) {
            move_approval_selection(1);
            return;
        }
        if (key == KEY_ENTER && answer_pending_approval(selected_approval_decision()))
            return;
        if (key == KEY_Y && answer_pending_approval("approve"))
            return;
        if (key == KEY_A && answer_pending_approval("always"))
            return;
        if (key == KEY_N && answer_pending_approval("deny"))
            return;
        if ((key == KEY_ESC || key == KEY_BACKSPACE) && answer_pending_approval("deny"))
            return;

        if (key == KEY_TAB) {
            if (settings_panel_open())
                close_settings_panel();
            else
                open_settings_panel();
            return;
        }
        if (settings_panel_open()) {
            if (key == KEY_ESC)
                settings_back();
            else if (key == KEY_BACKSPACE)
                settings_back();
            else if (key == KEY_ENTER)
                activate_settings_selection();
            else if (key == KEY_DELETE && settings_view_ == SettingsView::ProviderDetail)
                delete_current_provider();
            else if (key == KEY_UP)
                move_settings_selection(-1);
            else if (key == KEY_DOWN)
                move_settings_selection(1);
            return;
        }
        if (key == KEY_ESC && input_dialog_open()) {
            close_input_dialog();
            return;
        }
        if (key == KEY_ESC)
            return;
        else if (key == KEY_UP)
            scroll_chat(24);
        else if (key == KEY_DOWN)
            scroll_chat(-24);
        else if (key == KEY_ENTER) {
            input_mode_ = InputMode::Chat;
            open_input_dialog();
        }
    }
};

ZClawApp *g_app = nullptr;

} // namespace

void ui_init()
{
    static ZClawApp app;
    g_app = &app;
}

bool ui_should_quit()
{
    return g_quit_requested != 0;
}

void ui_request_quit()
{
    g_quit_requested = 1;
}
