/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "../ui_app_page.hpp"
#include "compat/input_keys.h"
#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "keyboard_input.h"
#include "lvgl/lvgl.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <list>
#include <string>
#include <utility>

namespace lora_app_detail {

constexpr uint32_t kPollIntervalMs         = 300;
constexpr lv_coord_t kScreenWidth          = 320;
constexpr lv_coord_t kContentHeight        = 150;
constexpr std::size_t kMessageHistoryLimit = 64;
constexpr int32_t kMessageScrollStep       = 36;
constexpr uint32_t kViewTransitionMs       = 150;
constexpr lv_coord_t kBubbleTailWidth      = 6;
constexpr lv_coord_t kBubbleTailDrop       = 3;

inline const char *safe_text(const char *text, const char *fallback = "")
{
    return text && text[0] ? text : fallback;
}

inline bool is_printable_ascii(uint32_t key)
{
    return key >= 0x20 && key <= 0x7e;
}

inline char key_to_ascii(uint32_t key)
{
    return is_printable_ascii(key) ? static_cast<char>(key) : '\0';
}

inline int call_lora_api(const std::list<std::string> &args, cp0_lora_info_t *info = nullptr)
{
    int result = -1;
    cp0_signal_lora_api(args, [&](int code, std::string data) {
        result = code;
        if (info && data.size() == sizeof(*info)) {
            std::memcpy(info, data.data(), sizeof(*info));
        }
    });
    return result;
}

inline bool is_menu_prev_key(uint32_t key)
{
    return key == LV_KEY_LEFT || key == LV_KEY_PREV || key == 'z' || key == 'Z';
}

inline bool is_menu_next_key(uint32_t key)
{
    return key == LV_KEY_RIGHT || key == LV_KEY_NEXT || key == 'c' || key == 'C';
}

}  // namespace lora_app_detail

class UILoraPage : public AppPage {
public:
    UILoraPage() : AppPage()
    {
        set_page_title("LORA");
        create_ui();
        bind_events();
        init_lora();
    }

    ~UILoraPage() override
    {
        app_active_ = false;
        cancel_view_animations();
        if (poll_timer_) {
            lv_timer_delete(poll_timer_);
            poll_timer_ = nullptr;
        }
    }

private:
    enum class View {
        Messages,
        Info,
        Send,
    };

    struct ChatMessage {
        ChatMessage(std::string value, bool is_outgoing, float message_rssi, float message_snr)
            : text(std::move(value)), outgoing(is_outgoing), rssi(message_rssi), snr(message_snr)
        {
        }

        std::string text;
        bool outgoing;
        float rssi;
        float snr;
    };

    View current_view_             = View::Messages;
    bool app_active_               = false;
    bool scroll_to_latest_pending_ = false;
    char tx_input_[128]            = "";
    char send_status_[64]          = "";
    cp0_lora_info_t lora_info_{};
    std::deque<ChatMessage> messages_;

    lv_timer_t *poll_timer_             = nullptr;
    lv_obj_t *page_root_                = nullptr;
    lv_obj_t *messages_view_            = nullptr;
    lv_obj_t *message_list_             = nullptr;
    lv_obj_t *empty_message_label_      = nullptr;
    lv_obj_t *empty_message_hint_label_ = nullptr;
    lv_obj_t *last_message_row_         = nullptr;
    lv_obj_t *info_view_                = nullptr;
    lv_obj_t *info_status_dot_          = nullptr;
    lv_obj_t *info_status_label_        = nullptr;
    lv_obj_t *info_device_value_        = nullptr;
    lv_obj_t *info_rssi_value_          = nullptr;
    lv_obj_t *info_snr_value_           = nullptr;
    lv_obj_t *info_link_value_          = nullptr;
    lv_obj_t *info_diag_value_          = nullptr;
    lv_obj_t *send_view_                = nullptr;
    lv_obj_t *send_input_bubble_        = nullptr;
    lv_obj_t *send_input_label_         = nullptr;
    lv_obj_t *send_status_label_        = nullptr;
    lv_obj_t *send_cancel_button_       = nullptr;
    lv_obj_t *send_confirm_button_      = nullptr;
    lv_obj_t *page_indicator_           = nullptr;
    lv_obj_t *page_dots_[2]             = {nullptr, nullptr};
    lv_obj_t *active_view_              = nullptr;

    static void set_visible(lv_obj_t *object, bool visible)
    {
        if (!object) return;
        if (visible)
            lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }

    static lv_obj_t *make_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height,
                                lv_color_t color, lv_opa_t opacity, lv_coord_t radius)
    {
        lv_obj_t *panel = lv_obj_create(parent);
        lv_obj_set_pos(panel, x, y);
        lv_obj_set_size(panel, width, height);
        lv_obj_set_style_bg_color(panel, color, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel, opacity, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(panel, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        return panel;
    }

    static lv_obj_t *make_plain_container(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                          lv_coord_t height)
    {
        return make_panel(parent, x, y, width, height, lv_color_hex(0x000000), LV_OPA_TRANSP, 0);
    }

    static lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                                lv_coord_t height, const lv_font_t *font, lv_color_t color, lv_text_align_t align)
    {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_label_set_text(label, text ? text : "");
        lv_obj_set_pos(label, x, y);
        lv_obj_set_size(label, width, height);
        lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(label, align, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        return label;
    }

    static lv_obj_t *make_divider(lv_obj_t *parent, lv_coord_t y)
    {
        return make_panel(parent, 8, y, 304, 1, lv_color_hex(0x25272B), LV_OPA_COVER, 0);
    }

    static void bubble_tail_draw_cb(lv_event_t *event)
    {
        lv_obj_t *row     = lv_event_get_target_obj(event);
        lv_layer_t *layer = lv_event_get_layer(event);
        if (!row || !layer) return;

        lv_obj_t *bubble = lv_obj_get_child(row, 0);
        if (!bubble) return;

        lv_area_t area;
        lv_obj_get_coords(bubble, &area);

        lv_draw_triangle_dsc_t draw_dsc;
        lv_draw_triangle_dsc_init(&draw_dsc);
        draw_dsc.color         = lv_obj_get_style_bg_color(bubble, LV_PART_MAIN);
        draw_dsc.opa           = lv_obj_get_style_bg_opa(bubble, LV_PART_MAIN);
        lv_opa_t recursive_opa = lv_obj_get_style_opa_recursive(bubble, LV_PART_MAIN);
        if (recursive_opa < LV_OPA_MAX) draw_dsc.opa = LV_OPA_MIX2(draw_dsc.opa, recursive_opa);

        constexpr lv_coord_t kTailRise     = 9;
        constexpr lv_coord_t kTailShoulder = 10;
        bool outgoing                      = lv_obj_has_flag(row, LV_OBJ_FLAG_USER_1);
        lv_coord_t side_x                  = outgoing ? area.x2 : area.x1;
        lv_coord_t shoulder_x              = outgoing ? area.x2 - kTailShoulder : area.x1 + kTailShoulder;
        lv_coord_t tip_x =
            outgoing ? area.x2 + lora_app_detail::kBubbleTailWidth : area.x1 - lora_app_detail::kBubbleTailWidth;
        draw_dsc.p[0].x = static_cast<lv_value_precise_t>(side_x);
        draw_dsc.p[0].y = static_cast<lv_value_precise_t>(area.y2 - kTailRise);
        draw_dsc.p[1].x = static_cast<lv_value_precise_t>(shoulder_x);
        draw_dsc.p[1].y = static_cast<lv_value_precise_t>(area.y2);
        draw_dsc.p[2].x = static_cast<lv_value_precise_t>(tip_x);
        draw_dsc.p[2].y = static_cast<lv_value_precise_t>(area.y2 + lora_app_detail::kBubbleTailDrop);
        lv_draw_triangle(layer, &draw_dsc);
    }

    void create_ui()
    {
        page_root_ = make_panel(ui_APP_Container, 0, 0, lora_app_detail::kScreenWidth, lora_app_detail::kContentHeight,
                                lv_color_hex(0x0B0C0E), LV_OPA_COVER, 0);
        create_messages_view();
        create_info_view();
        create_send_view();
        create_page_indicator();
    }

    void create_messages_view()
    {
        messages_view_ = make_plain_container(page_root_, 0, 0, 320, 150);
        message_list_  = make_plain_container(messages_view_, 0, 0, 320, 150);
        lv_obj_set_flex_flow(message_list_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(message_list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_left(message_list_, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(message_list_, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(message_list_, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(message_list_, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_row(message_list_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(message_list_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(message_list_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(message_list_, LV_SCROLLBAR_MODE_OFF);

        empty_message_label_ = make_label(messages_view_, "No messages yet", 0, 50, 320, 16, &lv_font_montserrat_12,
                                          lv_color_hex(0xB2B2B2), LV_TEXT_ALIGN_CENTER);
        empty_message_hint_label_ = make_label(messages_view_, "Type anything to send", 0, 68, 320, 14,
                                               &lv_font_montserrat_12, lv_color_hex(0x5FE492), LV_TEXT_ALIGN_CENTER);

        lv_obj_t *title_notch = make_panel(messages_view_, 108, -8, 104, 28, lv_color_hex(0x0B0C0E), LV_OPA_COVER, 8);
        make_label(title_notch, "Messages", 0, 8, 104, 18, &lv_font_montserrat_14, lv_color_hex(0xE4E4E4),
                   LV_TEXT_ALIGN_CENTER);
    }

    void create_info_view()
    {
        info_view_ = make_plain_container(page_root_, 0, 0, 320, 150);
        make_label(info_view_, "LoRa Info", 0, 0, 320, 18, &lv_font_montserrat_14, lv_color_hex(0xE4E4E4),
                   LV_TEXT_ALIGN_CENTER);

        info_status_dot_ = make_panel(info_view_, 8, 23, 6, 6, lv_color_hex(0x69AD80), LV_OPA_COVER, LV_RADIUS_CIRCLE);
        info_status_label_ = make_label(info_view_, "", 20, 20, 180, 16, &lv_font_montserrat_10, lv_color_hex(0xAEB2B8),
                                        LV_TEXT_ALIGN_LEFT);
        make_label(info_view_, "CLIENT", 240, 20, 72, 16, &lv_font_montserrat_10, lv_color_hex(0x777B82),
                   LV_TEXT_ALIGN_RIGHT);
        make_divider(info_view_, 41);

        make_label(info_view_, "DEVICE", 8, 48, 148, 11, &lv_font_montserrat_10, lv_color_hex(0x777B82),
                   LV_TEXT_ALIGN_LEFT);
        make_label(info_view_, "RSSI", 166, 48, 66, 11, &lv_font_montserrat_10, lv_color_hex(0x777B82),
                   LV_TEXT_ALIGN_LEFT);
        make_label(info_view_, "SNR", 240, 48, 72, 11, &lv_font_montserrat_10, lv_color_hex(0x777B82),
                   LV_TEXT_ALIGN_LEFT);

        info_device_value_ = make_label(info_view_, "", 8, 60, 148, 17, &lv_font_montserrat_12, lv_color_hex(0xDDE0E4),
                                        LV_TEXT_ALIGN_LEFT);
        info_rssi_value_   = make_label(info_view_, "", 166, 60, 66, 17, &lv_font_montserrat_12, lv_color_hex(0xDDE0E4),
                                        LV_TEXT_ALIGN_LEFT);
        info_snr_value_    = make_label(info_view_, "", 240, 60, 72, 17, &lv_font_montserrat_12, lv_color_hex(0xDDE0E4),
                                        LV_TEXT_ALIGN_LEFT);
        lv_label_set_long_mode(info_device_value_, LV_LABEL_LONG_DOT);

        make_divider(info_view_, 81);
        make_label(info_view_, "LINK", 8, 88, 304, 11, &lv_font_montserrat_10, lv_color_hex(0x777B82),
                   LV_TEXT_ALIGN_LEFT);
        info_link_value_ = make_label(info_view_, "", 8, 100, 304, 16, &lv_font_montserrat_12, lv_color_hex(0xDDE0E4),
                                      LV_TEXT_ALIGN_LEFT);
        info_diag_value_ = make_label(info_view_, "", 8, 120, 304, 13, &lv_font_montserrat_10, lv_color_hex(0x777B82),
                                      LV_TEXT_ALIGN_LEFT);
        lv_label_set_long_mode(info_link_value_, LV_LABEL_LONG_DOT);
        lv_label_set_long_mode(info_diag_value_, LV_LABEL_LONG_DOT);
    }

    void create_send_view()
    {
        send_view_ = make_plain_container(page_root_, 0, 0, 320, 150);
        make_label(send_view_, "New Message", 0, 0, 320, 18, &lv_font_montserrat_14, lv_color_hex(0xE4E4E4),
                   LV_TEXT_ALIGN_CENTER);

        send_input_bubble_ = make_panel(send_view_, 0, 0, 286, 80, lv_color_hex(0x555555), LV_OPA_COVER, 8);
        lv_obj_align(send_input_bubble_, LV_ALIGN_CENTER, 0, -12);
        send_input_label_ = make_label(send_input_bubble_, "", 10, 8, 266, 64, &lv_font_montserrat_14,
                                       lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_LEFT);

        send_status_label_ = make_label(send_input_bubble_, "", 10, 54, 266, 16, &lv_font_montserrat_14,
                                        lv_color_hex(0xFED40D), LV_TEXT_ALIGN_RIGHT);
        set_visible(send_status_label_, false);

        send_cancel_button_  = make_action_button(send_view_, 83, 113, 110, "ESC: Cancel", lv_color_hex(0x6D6D6D),
                                                  lv_color_hex(0xF3F3F3), &UILoraPage::static_cancel_button_cb);
        send_confirm_button_ = make_action_button(send_view_, 203, 113, 100, "Enter: Send", lv_color_hex(0xFED40D),
                                                  lv_color_hex(0x5E4D00), &UILoraPage::static_send_button_cb);
    }

    lv_obj_t *make_action_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, const char *text,
                                 lv_color_t background, lv_color_t foreground, lv_event_cb_t callback)
    {
        lv_obj_t *button = make_panel(parent, x, y, width, 26, background, LV_OPA_COVER, 5);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, this);
        lv_obj_t *label = make_label(button, text, 0, 0, LV_SIZE_CONTENT, LV_SIZE_CONTENT, &lv_font_montserrat_14,
                                     foreground, LV_TEXT_ALIGN_CENTER);
        lv_obj_center(label);
        return button;
    }

    void create_page_indicator()
    {
        page_indicator_ = make_panel(page_root_, 141, 137, 38, 24, lv_color_hex(0x0B0C0E), LV_OPA_COVER, 7);
        page_dots_[0] =
            make_panel(page_indicator_, 11, 3, 5, 5, lv_color_hex(0xE4E4E4), LV_OPA_COVER, LV_RADIUS_CIRCLE);
        page_dots_[1] =
            make_panel(page_indicator_, 22, 3, 5, 5, lv_color_hex(0x4E5157), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    }

    void bind_events()
    {
        lv_obj_add_event_cb(root_screen_, &UILoraPage::static_key_event_cb,
                            static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), this);
    }

    void init_lora()
    {
        app_active_               = true;
        current_view_             = View::Messages;
        scroll_to_latest_pending_ = false;
        tx_input_[0]              = '\0';
        send_status_[0]           = '\0';
        messages_.clear();
        lv_obj_clean(message_list_);
        last_message_row_ = nullptr;
        set_visible(empty_message_label_, true);
        set_visible(empty_message_hint_label_, true);

        (void)lora_app_detail::call_lora_api({"Init"});
        refresh_lora_info(true);
        lora_info_.rx_event = 0;
        lora_info_.tx_event = 0;
        current_view_       = lora_info_.hw_ready ? View::Messages : View::Info;
        render_current_view();
        if (lora_info_.hw_ready) {
            (void)lora_app_detail::call_lora_api({"StartReceive"});
        }
        poll_timer_ = lv_timer_create(&UILoraPage::static_poll_timer_cb, lora_app_detail::kPollIntervalMs, this);
    }

    void refresh_lora_info(bool poll)
    {
        (void)lora_app_detail::call_lora_api({poll ? "Poll" : "Info"}, &lora_info_);
    }

    void update_page_indicator()
    {
        bool messages_active = current_view_ == View::Messages;
        lv_obj_set_style_bg_color(page_dots_[0], lv_color_hex(messages_active ? 0xE4E4E4 : 0x4E5157),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(page_dots_[1], lv_color_hex(messages_active ? 0x4E5157 : 0xE4E4E4),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void update_info_content()
    {
        const char *state_text = "RECEIVING";
        uint32_t state_color   = 0x69AD80;
        if (!lora_info_.hw_ready) {
            state_text  = "RADIO OFF";
            state_color = 0xD96C6C;
        } else if (lora_info_.tx_in_progress) {
            state_text  = "SENDING";
            state_color = 0xC9A45C;
        } else if (lora_info_.tx_mode) {
            state_text  = "TX MODE";
            state_color = 0xC9A45C;
        }
        lv_label_set_text(info_status_label_, state_text);
        lv_obj_set_style_bg_color(info_status_dot_, lv_color_hex(state_color), LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_label_set_text(info_device_value_, lora_app_detail::safe_text(lora_info_.spi_device, "Unavailable"));

        char value[64];
        std::snprintf(value, sizeof(value), "%.0f dBm", lora_info_.rssi);
        lv_label_set_text(info_rssi_value_, value);
        std::snprintf(value, sizeof(value), "%.1f dB", lora_info_.snr);
        lv_label_set_text(info_snr_value_, value);

        lv_label_set_text(info_link_value_,
                          lora_app_detail::safe_text(lora_info_.probe_display, "Link configuration unavailable"));
        lv_label_set_text(info_diag_value_, lora_app_detail::safe_text(lora_info_.diag, "No diagnostics"));
        lv_obj_set_style_text_color(info_diag_value_, lv_color_hex(lora_info_.hw_ready ? 0x777B82 : 0xD96C6C),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void update_send_content()
    {
        char display[sizeof(tx_input_) + 2];
        std::snprintf(display, sizeof(display), "%s|", tx_input_);
        lv_label_set_text(send_input_label_, display);
        lv_obj_set_style_text_color(send_input_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

        bool has_status = send_status_[0] != '\0';
        lv_label_set_text(send_status_label_, has_status ? send_status_ : "");
        set_visible(send_status_label_, has_status);
    }

    void scroll_to_latest(lv_anim_enable_t animation)
    {
        if (!last_message_row_) {
            scroll_to_latest_pending_ = false;
            return;
        }
        lv_obj_update_layout(message_list_);
        lv_obj_scroll_to_view(last_message_row_, animation);
        scroll_to_latest_pending_ = false;
    }

    static void view_opa_exec_cb(void *object, int32_t opacity)
    {
        lv_obj_set_style_opa(static_cast<lv_obj_t *>(object), static_cast<lv_opa_t>(opacity),
                             LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    static void hide_view_after_fade_cb(lv_anim_t *animation)
    {
        auto *view = static_cast<lv_obj_t *>(lv_anim_get_user_data(animation));
        if (!view) return;
        set_visible(view, false);
        lv_obj_set_style_opa(view, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    static void cancel_view_animation(lv_obj_t *view)
    {
        if (view) lv_anim_del(view, view_opa_exec_cb);
    }

    void cancel_view_animations()
    {
        cancel_view_animation(messages_view_);
        cancel_view_animation(info_view_);
        cancel_view_animation(send_view_);
    }

    static void animate_view_opacity(lv_obj_t *view, lv_opa_t start, lv_opa_t end, bool hide_after_fade)
    {
        if (!view) return;
        cancel_view_animation(view);
        set_visible(view, true);
        view_opa_exec_cb(view, start);

        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, view);
        lv_anim_set_values(&animation, start, end);
        lv_anim_set_time(&animation, lora_app_detail::kViewTransitionMs);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
        lv_anim_set_exec_cb(&animation, view_opa_exec_cb);
        if (hide_after_fade) {
            lv_anim_set_user_data(&animation, view);
            lv_anim_set_completed_cb(&animation, hide_view_after_fade_cb);
        }
        lv_anim_start(&animation);
    }

    void transition_to_view(lv_obj_t *target)
    {
        if (!target || target == active_view_) return;

        if (!active_view_) {
            lv_obj_t *views[] = {messages_view_, info_view_, send_view_};
            for (lv_obj_t *view : views) {
                cancel_view_animation(view);
                lv_obj_set_style_opa(view, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
                set_visible(view, view == target);
            }
            active_view_ = target;
            return;
        }

        lv_obj_t *outgoing      = active_view_;
        lv_opa_t outgoing_start = lv_obj_get_style_opa(outgoing, LV_PART_MAIN);
        lv_opa_t incoming_start =
            lv_obj_has_flag(target, LV_OBJ_FLAG_HIDDEN) ? LV_OPA_TRANSP : lv_obj_get_style_opa(target, LV_PART_MAIN);
        animate_view_opacity(outgoing, outgoing_start, LV_OPA_TRANSP, true);
        animate_view_opacity(target, incoming_start, LV_OPA_COVER, false);
        active_view_ = target;
    }

    void render_current_view()
    {
        bool show_messages = current_view_ == View::Messages;
        bool show_info     = current_view_ == View::Info;
        bool show_send     = current_view_ == View::Send;

        if (show_info) update_info_content();
        if (show_send) update_send_content();
        transition_to_view(show_messages ? messages_view_ : (show_info ? info_view_ : send_view_));
        set_visible(page_indicator_, !show_send);
        if (!show_send) update_page_indicator();
        if (show_messages && scroll_to_latest_pending_) {
            scroll_to_latest(LV_ANIM_OFF);
        }
    }

    lv_obj_t *append_message_row(const ChatMessage &message)
    {
        char metadata[64] = "";
        if (!message.outgoing) {
            std::snprintf(metadata, sizeof(metadata), "%.0f dBm  /  %.1f dB", message.rssi, message.snr);
        }

        constexpr int32_t kHorizontalPadding = 10;
        constexpr int32_t kMaxTextWidth      = 224;
        constexpr int32_t kMaxBubbleWidth    = 244;
        lv_point_t text_size{};
        lv_text_get_size(&text_size, message.text.c_str(), &lv_font_montserrat_12, 0, 0, kMaxTextWidth,
                         LV_TEXT_FLAG_NONE);
        int32_t content_width = text_size.x;
        if (metadata[0]) {
            lv_point_t metadata_size{};
            lv_text_get_size(&metadata_size, metadata, &lv_font_montserrat_10, 0, 0, kMaxTextWidth, LV_TEXT_FLAG_NONE);
            content_width = std::max(content_width, metadata_size.x);
        }
        int32_t bubble_width =
            std::max<int32_t>(64, std::min<int32_t>(kMaxBubbleWidth, content_width + kHorizontalPadding * 2));

        lv_obj_t *row = make_plain_container(message_list_, 0, 0, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, message.outgoing ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        if (message.outgoing) lv_obj_add_flag(row, LV_OBJ_FLAG_USER_1);
        lv_obj_add_event_cb(row, bubble_tail_draw_cb, LV_EVENT_DRAW_MAIN_END, nullptr);

        lv_color_t bubble_color = lv_color_hex(message.outgoing ? 0x3FCC75 : 0xCCCCCC);
        lv_obj_t *bubble        = make_panel(row, 0, 0, bubble_width, LV_SIZE_CONTENT, bubble_color, LV_OPA_COVER, 8);
        lv_obj_set_style_margin_left(bubble, message.outgoing ? 0 : lora_app_detail::kBubbleTailWidth,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_margin_right(bubble, message.outgoing ? lora_app_detail::kBubbleTailWidth : 0,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_margin_bottom(bubble, lora_app_detail::kBubbleTailDrop, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(bubble, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_left(bubble, kHorizontalPadding, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(bubble, kHorizontalPadding, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(bubble, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(bubble, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_row(bubble, 3, LV_PART_MAIN | LV_STATE_DEFAULT);

        make_label(bubble, message.text.c_str(), 0, 0, bubble_width - kHorizontalPadding * 2, LV_SIZE_CONTENT,
                   &lv_font_montserrat_12, lv_color_hex(0x000000), LV_TEXT_ALIGN_LEFT);
        if (metadata[0]) {
            make_label(bubble, metadata, 0, 0, bubble_width - kHorizontalPadding * 2, LV_SIZE_CONTENT,
                       &lv_font_montserrat_10, lv_color_hex(0x7E7E7E), LV_TEXT_ALIGN_RIGHT);
        }
        return row;
    }

    void append_chat_message(const char *text, bool outgoing, float rssi, float snr)
    {
        const char *display_text = text && text[0] ? text : "<empty>";
        if (messages_.size() >= lora_app_detail::kMessageHistoryLimit) {
            messages_.pop_front();
            lv_obj_t *oldest_row = lv_obj_get_child(message_list_, 0);
            if (oldest_row) lv_obj_delete(oldest_row);
        }

        messages_.emplace_back(display_text, outgoing, rssi, snr);
        last_message_row_ = append_message_row(messages_.back());
        set_visible(empty_message_label_, false);
        set_visible(empty_message_hint_label_, false);

        if (current_view_ == View::Messages) {
            scroll_to_latest(LV_ANIM_ON);
        } else {
            scroll_to_latest_pending_ = true;
        }
    }

    void open_send_view(uint32_t first_key)
    {
        current_view_   = View::Send;
        send_status_[0] = '\0';
        tx_input_[0]    = '\0';

        char ch = lora_app_detail::key_to_ascii(first_key);
        if (ch != '\0') {
            tx_input_[0] = ch;
            tx_input_[1] = '\0';
        }
        render_current_view();
    }

    void scroll_messages(int32_t amount)
    {
        if (message_list_) {
            lv_obj_scroll_by_bounded(message_list_, 0, amount, LV_ANIM_ON);
        }
    }

    void cancel_send()
    {
        current_view_   = View::Messages;
        tx_input_[0]    = '\0';
        send_status_[0] = '\0';
        render_current_view();
    }

    bool handle_send_key(uint32_t key)
    {
        if (key == LV_KEY_ESC) {
            cancel_send();
            return true;
        }

        if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
            std::size_t length = std::strlen(tx_input_);
            if (length > 0) tx_input_[length - 1] = '\0';
            send_status_[0] = '\0';
            update_send_content();
            return true;
        }

        if (key == LV_KEY_ENTER) {
            send_current_text();
            return true;
        }

        if (lora_app_detail::is_printable_ascii(key)) {
            append_text_key(key);
            update_send_content();
            return true;
        }

        return true;
    }

    bool handle_navigation_key(uint32_t key)
    {
        if (lora_app_detail::is_menu_prev_key(key)) {
            current_view_ = View::Messages;
            render_current_view();
            return true;
        }
        if (lora_app_detail::is_menu_next_key(key)) {
            current_view_ = View::Info;
            render_current_view();
            return true;
        }

        if (current_view_ == View::Messages) {
            if (key == LV_KEY_UP) {
                scroll_messages(lora_app_detail::kMessageScrollStep);
                return true;
            }
            if (key == LV_KEY_DOWN) {
                scroll_messages(-lora_app_detail::kMessageScrollStep);
                return true;
            }
        } else {
            if (key == LV_KEY_UP) {
                current_view_ = View::Messages;
                render_current_view();
                return true;
            }
            if (key == LV_KEY_DOWN) {
                current_view_ = View::Info;
                render_current_view();
                return true;
            }
        }

        if (key == LV_KEY_ENTER) {
            open_send_view(0);
            return true;
        }
        if (lora_app_detail::is_printable_ascii(key) && key != 'z' && key != 'Z' && key != 'c' && key != 'C') {
            open_send_view(key);
            return true;
        }
        return false;
    }

    bool handle_key(uint32_t key)
    {
        if (current_view_ == View::Send) {
            return handle_send_key(key);
        }

        if (key == LV_KEY_ESC || key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
            if (navigate_home) navigate_home();
            return true;
        }

        return handle_navigation_key(key);
    }

    void append_text_key(uint32_t key)
    {
        std::size_t length = std::strlen(tx_input_);
        if (length + 1 < sizeof(tx_input_)) {
            tx_input_[length]     = lora_app_detail::key_to_ascii(key);
            tx_input_[length + 1] = '\0';
        }
        send_status_[0] = '\0';
    }

    void send_current_text()
    {
        if (!tx_input_[0]) {
            std::snprintf(send_status_, sizeof(send_status_), "Message is empty :(");
            update_send_content();
            return;
        }

        std::string sent_text(tx_input_);
        if (lora_app_detail::call_lora_api({"SendText", sent_text}) == 0) {
            refresh_lora_info(false);
            append_chat_message(sent_text.c_str(), true, 0.0f, 0.0f);
            current_view_   = View::Messages;
            tx_input_[0]    = '\0';
            send_status_[0] = '\0';
            render_current_view();
        } else {
            std::snprintf(send_status_, sizeof(send_status_), "Send failed");
            update_send_content();
        }
    }

    uint32_t normalize_key(const key_item *key_event) const
    {
        uint32_t key       = key_event->key_code;
        uint32_t codepoint = key_event->codepoint;

        if (current_view_ != View::Send) {
            if (key == KEY_F) return LV_KEY_UP;
            if (key == KEY_X) return LV_KEY_DOWN;
        }
        if (lora_app_detail::is_printable_ascii(codepoint)) return codepoint;

        if (key == KEY_UP) return LV_KEY_UP;
        if (key == KEY_DOWN) return LV_KEY_DOWN;
        if (key == KEY_LEFT) return LV_KEY_LEFT;
        if (key == KEY_RIGHT) return LV_KEY_RIGHT;
        if (key == KEY_ENTER || key == KEY_KPENTER) return LV_KEY_ENTER;
        if (key == KEY_ESC) return LV_KEY_ESC;
        if (key == KEY_BACKSPACE) return LV_KEY_BACKSPACE;
        if (key == KEY_DELETE) return LV_KEY_DEL;
        return key;
    }

    void on_key_event(lv_event_t *event)
    {
        auto *key_event = static_cast<key_item *>(lv_event_get_param(event));
        if (!key_event || key_event->key_state == KBD_KEY_RELEASED) return;
        (void)handle_key(normalize_key(key_event));
    }

    void on_poll_timer()
    {
        if (!app_active_) return;

        refresh_lora_info(true);
        if (lora_info_.rx_event) {
            append_chat_message(lora_info_.last_rx, false, lora_info_.rssi, lora_info_.snr);
        }
        if (current_view_ == View::Info) {
            update_info_content();
        }
    }

    static void static_cancel_button_cb(lv_event_t *event)
    {
        auto *self = static_cast<UILoraPage *>(lv_event_get_user_data(event));
        if (self) self->cancel_send();
    }

    static void static_send_button_cb(lv_event_t *event)
    {
        auto *self = static_cast<UILoraPage *>(lv_event_get_user_data(event));
        if (self) self->send_current_text();
    }

    static void static_key_event_cb(lv_event_t *event)
    {
        if (lv_event_get_code(event) != static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD)) return;
        auto *self = static_cast<UILoraPage *>(lv_event_get_user_data(event));
        if (self) self->on_key_event(event);
    }

    static void static_poll_timer_cb(lv_timer_t *timer)
    {
        auto *self = static_cast<UILoraPage *>(lv_timer_get_user_data(timer));
        if (self) self->on_poll_timer();
    }
};
