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

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <list>
#include <string>

namespace lora_app_detail {

constexpr uint32_t kPollIntervalMs = 300;
constexpr uint32_t kRxPopupDurationMs = 2000;
constexpr lv_coord_t kScreenWidth = 320;
constexpr lv_coord_t kContentWidth = 304;

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

inline int call_lora_api(const std::list<std::string>& args, cp0_lora_info_t *info = nullptr)
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

} // namespace lora_app_detail

class UILoraPage : public AppPage
{
public:
    UILoraPage() : AppPage()
    {
        create_ui();
        bind_events();
        init_lora();
    }

    ~UILoraPage() override
    {
        app_active_ = false;
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

    View current_view_ = View::Messages;
    bool app_active_ = false;
    bool pending_rx_popup_ = false;
    uint32_t rx_popup_started_ms_ = 0;
    char tx_input_[128] = "";
    char send_status_[64] = "";
    cp0_lora_info_t lora_info_{};

    lv_timer_t *poll_timer_ = nullptr;
    lv_obj_t *title_label_ = nullptr;
    lv_obj_t *content_label_ = nullptr;
    lv_obj_t *info_pins_label_ = nullptr;
    lv_obj_t *info_device_label_ = nullptr;
    lv_obj_t *info_mode_label_ = nullptr;
    lv_obj_t *info_status_label_ = nullptr;
    lv_obj_t *info_hint_label_ = nullptr;
    lv_obj_t *rx_bubble_ = nullptr;
    lv_obj_t *rx_bubble_label_ = nullptr;
    lv_obj_t *tx_bubble_ = nullptr;
    lv_obj_t *tx_bubble_label_ = nullptr;

    static lv_obj_t *make_label(lv_obj_t *parent,
                                const char *text,
                                lv_coord_t x,
                                lv_coord_t y,
                                lv_coord_t w,
                                lv_coord_t h,
                                const lv_font_t *font,
                                lv_color_t color,
                                lv_text_align_t align)
    {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, text ? text : "");
        lv_obj_set_pos(label, x, y);
        lv_obj_set_size(label, w, h);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(label, align, LV_PART_MAIN | LV_STATE_DEFAULT);
        return label;
    }

    static lv_obj_t *make_bubble(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_color_t bg_color)
    {
        lv_obj_t *bubble = lv_obj_create(parent);
        lv_obj_set_pos(bubble, x, y);
        lv_obj_set_size(bubble, w, h);
        lv_obj_set_style_bg_color(bubble, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(bubble, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bubble, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(bubble, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(bubble, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(bubble, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(bubble, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(bubble, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(bubble, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(bubble, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_spread(bubble, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_ofs_x(bubble, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_ofs_y(bubble, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
        return bubble;
    }

    static lv_obj_t *make_bubble_label(lv_obj_t *parent, lv_coord_t max_width)
    {
        lv_obj_t *label = lv_label_create(parent);
        lv_obj_set_width(label, max_width);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        return label;
    }

    void create_ui()
    {
        title_label_ = make_label(ui_APP_Container, "LoRa-1262", 0, 0, lora_app_detail::kScreenWidth, 18,
                                  &lv_font_montserrat_14, lv_color_hex(0x8D44FF), LV_TEXT_ALIGN_CENTER);
        content_label_ = make_label(ui_APP_Container, "", 8, 22, lora_app_detail::kContentWidth, 90,
                                    &lv_font_montserrat_12, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_LEFT);
        info_pins_label_ = make_label(ui_APP_Container, "", 8, 22, lora_app_detail::kContentWidth, 18,
                                      &lv_font_montserrat_12, lv_color_hex(0xB8FF9C), LV_TEXT_ALIGN_LEFT);
        info_device_label_ = make_label(ui_APP_Container, "", 8, 42, lora_app_detail::kContentWidth, 18,
                                        &lv_font_montserrat_12, lv_color_hex(0xB8FF9C), LV_TEXT_ALIGN_LEFT);
        info_mode_label_ = make_label(ui_APP_Container, "", 8, 62, lora_app_detail::kContentWidth, 18,
                                      &lv_font_montserrat_12, lv_color_hex(0xB8FF9C), LV_TEXT_ALIGN_LEFT);
        info_status_label_ = make_label(ui_APP_Container, "", 8, 114, lora_app_detail::kContentWidth, 16,
                                        &lv_font_montserrat_12, lv_color_hex(0xFFD24A), LV_TEXT_ALIGN_LEFT);
        info_hint_label_ = make_label(ui_APP_Container, "", 8, 132, lora_app_detail::kContentWidth, 14,
                                      &lv_font_montserrat_10, lv_color_hex(0x8AA8FF), LV_TEXT_ALIGN_LEFT);

        rx_bubble_ = make_bubble(ui_APP_Container, 4, 20, 250, 44, lv_color_hex(0x3A7DFF));
        rx_bubble_label_ = make_bubble_label(rx_bubble_, 234);
        lv_obj_add_flag(rx_bubble_, LV_OBJ_FLAG_HIDDEN);

        tx_bubble_ = make_bubble(ui_APP_Container, 66, 68, 250, 44, lv_color_hex(0x00A854));
        tx_bubble_label_ = make_bubble_label(tx_bubble_, 234);
        lv_obj_add_flag(tx_bubble_, LV_OBJ_FLAG_HIDDEN);
    }

    void bind_events()
    {
        lv_obj_add_event_cb(root_screen_, &UILoraPage::static_key_event_cb,
                            static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), this);
    }

    void init_lora()
    {
        app_active_ = true;
        current_view_ = View::Messages;
        pending_rx_popup_ = false;
        rx_popup_started_ms_ = 0;
        tx_input_[0] = '\0';
        send_status_[0] = '\0';

        (void)lora_app_detail::call_lora_api({"Init"});
        refresh_lora_info(false);
        render_lora_page();
        poll_timer_ = lv_timer_create(&UILoraPage::static_poll_timer_cb, lora_app_detail::kPollIntervalMs, this);
    }

    void refresh_lora_info(bool poll)
    {
        (void)lora_app_detail::call_lora_api({poll ? "Poll" : "Info"}, &lora_info_);
    }

    void clear_view()
    {
        if (title_label_) lv_obj_add_flag(title_label_, LV_OBJ_FLAG_HIDDEN);
        if (content_label_) lv_obj_add_flag(content_label_, LV_OBJ_FLAG_HIDDEN);
        if (info_pins_label_) lv_obj_add_flag(info_pins_label_, LV_OBJ_FLAG_HIDDEN);
        if (info_device_label_) lv_obj_add_flag(info_device_label_, LV_OBJ_FLAG_HIDDEN);
        if (info_mode_label_) lv_obj_add_flag(info_mode_label_, LV_OBJ_FLAG_HIDDEN);
        if (info_status_label_) lv_obj_add_flag(info_status_label_, LV_OBJ_FLAG_HIDDEN);
        if (info_hint_label_) lv_obj_add_flag(info_hint_label_, LV_OBJ_FLAG_HIDDEN);
        if (rx_bubble_) lv_obj_add_flag(rx_bubble_, LV_OBJ_FLAG_HIDDEN);
        if (tx_bubble_) lv_obj_add_flag(tx_bubble_, LV_OBJ_FLAG_HIDDEN);
    }

    void reset_content_label_style()
    {
        if (!content_label_) return;
        lv_obj_set_style_text_font(content_label_, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(content_label_, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void render_lora_page()
    {
        if (!lora_info_.hw_ready) {
            render_boot_diag_view();
            return;
        }

        current_view_ = View::Messages;
        render_current_view();
        if (!lora_info_.tx_mode && !lora_info_.tx_in_progress) {
            (void)lora_app_detail::call_lora_api({"StartReceive"});
        }
    }

    void render_boot_diag_view()
    {
        clear_view();
        if (title_label_) {
            lv_obj_clear_flag(title_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(title_label_, "LoRa-1262");
        }
        if (content_label_) {
            reset_content_label_style();
            lv_obj_clear_flag(content_label_, LV_OBJ_FLAG_HIDDEN);
            char text[256];
            std::snprintf(text, sizeof(text), "SPI:%s\n%s\n%s",
                          lora_app_detail::safe_text(lora_info_.spi_device, "n/a"),
                          lora_app_detail::safe_text(lora_info_.pi4io_status, "I2C status unavailable"),
                          lora_app_detail::safe_text(lora_info_.probe_summary, "probe not started"));
            lv_label_set_text(content_label_, text);
            lv_obj_set_style_text_color(content_label_, lv_color_hex(0xFF4D4F), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (info_status_label_) {
            lv_obj_clear_flag(info_status_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(info_status_label_, lora_app_detail::safe_text(lora_info_.diag, "LoRa not ready"));
            lv_obj_set_style_text_color(info_status_label_, lv_color_hex(0xFF4D4F), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (info_hint_label_) {
            lv_obj_clear_flag(info_hint_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(info_hint_label_, lora_app_detail::safe_text(lora_info_.probe_display, "Boot diag for SPI check"));
            lv_obj_set_style_text_color(info_hint_label_, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void render_messages_view()
    {
        clear_view();
        if (title_label_) {
            lv_obj_clear_flag(title_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(title_label_, "Messages");
        }

        if (rx_bubble_ && rx_bubble_label_) {
            lv_obj_clear_flag(rx_bubble_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(rx_bubble_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(rx_bubble_, 4, 20);
            lv_obj_set_size(rx_bubble_, 250, 44);
            lv_obj_set_width(rx_bubble_label_, 234);
            if (lora_info_.last_rx[0]) {
                lv_label_set_text(rx_bubble_label_, lora_info_.last_rx);
                lv_obj_set_style_text_color(rx_bubble_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else {
                lv_label_set_text(rx_bubble_label_, "Waiting for message...");
                lv_obj_set_style_text_color(rx_bubble_label_, lv_color_hex(0xAACCFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }

        if (tx_bubble_ && tx_bubble_label_) {
            if (lora_info_.has_sent_message) {
                lv_obj_clear_flag(tx_bubble_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(tx_bubble_label_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_pos(tx_bubble_, 66, 68);
                lv_obj_set_size(tx_bubble_, 250, 44);
                lv_obj_set_width(tx_bubble_label_, 234);
                lv_label_set_text(tx_bubble_label_, lora_app_detail::safe_text(lora_info_.last_tx));
            } else {
                lv_obj_add_flag(tx_bubble_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(tx_bubble_label_, LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (info_status_label_) {
            lv_obj_clear_flag(info_status_label_, LV_OBJ_FLAG_HIDDEN);
            char text[128];
            std::snprintf(text, sizeof(text), "RSSI: %.1fdBm | SNR: %.1fdB", lora_info_.rssi, lora_info_.snr);
            lv_label_set_text(info_status_label_, text);
            lv_obj_set_style_text_color(info_status_label_, lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (info_hint_label_) {
            lv_obj_clear_flag(info_hint_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(info_hint_label_, "Type to send | C/Right: Info | ESC: Back");
            lv_obj_set_style_text_color(info_hint_label_, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void render_info_view()
    {
        clear_view();
        if (title_label_) {
            lv_obj_clear_flag(title_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(title_label_, "LoRa Info");
        }
        if (info_pins_label_) {
            lv_obj_clear_flag(info_pins_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(info_pins_label_, "Role: Client");
            lv_obj_set_style_text_color(info_pins_label_, lv_color_hex(0xB8FF9C), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (info_device_label_) {
            lv_obj_clear_flag(info_device_label_, LV_OBJ_FLAG_HIDDEN);
            char text[192];
            std::snprintf(text, sizeof(text), "Device: %s | RX:%s TX:%s",
                          lora_app_detail::safe_text(lora_info_.spi_device, "n/a"),
                          lora_info_.hw_ready ? "ready" : "off",
                          lora_info_.tx_in_progress ? "busy" : "idle");
            lv_label_set_text(info_device_label_, text);
            lv_obj_set_style_text_color(info_device_label_, lv_color_hex(0xB8FF9C), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (info_mode_label_) {
            lv_obj_clear_flag(info_mode_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(info_mode_label_, lora_app_detail::safe_text(lora_info_.probe_display, "Channel: 868MHz | BW:125kHz SF12"));
            lv_obj_set_style_text_color(info_mode_label_, lv_color_hex(0xB8FF9C), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (info_status_label_) {
            lv_obj_clear_flag(info_status_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(info_status_label_, lora_app_detail::safe_text(lora_info_.diag, "LoRa ready"));
            lv_obj_set_style_text_color(info_status_label_, lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (info_hint_label_) {
            lv_obj_clear_flag(info_hint_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(info_hint_label_, "Z/Left: Messages | Type: Send | ESC: Back");
            lv_obj_set_style_text_color(info_hint_label_, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void render_send_view()
    {
        clear_view();
        if (title_label_) {
            lv_obj_clear_flag(title_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(title_label_, "Send");
        }
        if (content_label_) {
            lv_obj_clear_flag(content_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(content_label_, tx_input_[0] ? tx_input_ : "_");
            lv_obj_set_style_text_font(content_label_, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(content_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(content_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (info_status_label_) {
            lv_obj_clear_flag(info_status_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(info_status_label_, send_status_[0] ? send_status_ : "OK Send | DEL Delete | ESC Cancel");
            lv_obj_set_style_text_color(info_status_label_, lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void render_received_popup()
    {
        clear_view();
        if (title_label_) {
            lv_obj_clear_flag(title_label_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(title_label_, "Received");
        }
        if (rx_bubble_ && rx_bubble_label_) {
            lv_obj_clear_flag(rx_bubble_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(rx_bubble_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(rx_bubble_, 4, 22);
            lv_obj_set_size(rx_bubble_, 312, 86);
            lv_obj_set_width(rx_bubble_label_, 296);
            lv_label_set_text(rx_bubble_label_, lora_app_detail::safe_text(lora_info_.last_rx, "<empty>"));
            lv_obj_set_style_text_color(rx_bubble_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (info_status_label_) {
            lv_obj_clear_flag(info_status_label_, LV_OBJ_FLAG_HIDDEN);
            char text[96];
            std::snprintf(text, sizeof(text), "SNR %.1f  RSSI %.0f", lora_info_.snr, lora_info_.rssi);
            lv_label_set_text(info_status_label_, text);
            lv_obj_set_style_text_color(info_status_label_, lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void render_current_view()
    {
        if (rx_popup_started_ms_ != 0 && lv_tick_elaps(rx_popup_started_ms_) < lora_app_detail::kRxPopupDurationMs) {
            render_received_popup();
            return;
        }

        rx_popup_started_ms_ = 0;
        if (current_view_ == View::Info) {
            render_info_view();
        } else if (current_view_ == View::Send) {
            render_send_view();
        } else {
            current_view_ = View::Messages;
            render_messages_view();
        }
    }

    void open_send_view(uint32_t first_key)
    {
        current_view_ = View::Send;
        rx_popup_started_ms_ = 0;
        send_status_[0] = '\0';
        tx_input_[0] = '\0';

        char ch = lora_app_detail::key_to_ascii(first_key);
        if (ch != '\0') {
            tx_input_[0] = ch;
            tx_input_[1] = '\0';
        }
        render_send_view();
    }

    bool handle_send_key(uint32_t key)
    {
        if (key == LV_KEY_ESC) {
            current_view_ = View::Messages;
            tx_input_[0] = '\0';
            send_status_[0] = '\0';
            show_pending_popup_if_needed();
            render_current_view();
            return true;
        }

        if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
            size_t len = std::strlen(tx_input_);
            if (len > 0) tx_input_[len - 1] = '\0';
            send_status_[0] = '\0';
            render_send_view();
            return true;
        }

        if (key == LV_KEY_ENTER) {
            send_current_text();
            return true;
        }

        if (lora_app_detail::is_printable_ascii(key)) {
            append_text_key(key);
            render_send_view();
            return true;
        }

        return true;
    }

    bool handle_navigation_key(uint32_t key)
    {
        if (lora_app_detail::is_menu_prev_key(key) || key == LV_KEY_UP) {
            current_view_ = View::Messages;
            rx_popup_started_ms_ = 0;
            render_current_view();
            return true;
        }
        if (lora_app_detail::is_menu_next_key(key) || key == LV_KEY_DOWN) {
            current_view_ = View::Info;
            rx_popup_started_ms_ = 0;
            render_current_view();
            return true;
        }
        if (key == LV_KEY_ENTER) {
            render_current_view();
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
        size_t len = std::strlen(tx_input_);
        if (len + 1 < sizeof(tx_input_)) {
            tx_input_[len] = lora_app_detail::key_to_ascii(key);
            tx_input_[len + 1] = '\0';
        }
        send_status_[0] = '\0';
    }

    void send_current_text()
    {
        if (!tx_input_[0]) {
            std::snprintf(send_status_, sizeof(send_status_), "Message is empty");
            render_send_view();
            return;
        }

        if (lora_app_detail::call_lora_api({"SendText", tx_input_}) == 0) {
            refresh_lora_info(false);
            current_view_ = View::Messages;
            rx_popup_started_ms_ = pending_rx_popup_ ? lv_tick_get() : 0;
            pending_rx_popup_ = false;
            render_current_view();
            tx_input_[0] = '\0';
            send_status_[0] = '\0';
        } else {
            std::snprintf(send_status_, sizeof(send_status_), "Send failed");
            render_send_view();
        }
    }

    void show_pending_popup_if_needed()
    {
        if (!pending_rx_popup_) return;
        pending_rx_popup_ = false;
        rx_popup_started_ms_ = lv_tick_get();
    }

    uint32_t normalize_key(const key_item *key_event) const
    {
        uint32_t key = key_event->key_code;
        uint32_t codepoint = key_event->codepoint;

        if (lora_app_detail::is_printable_ascii(codepoint)) {
            key = codepoint;
        }

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
        if (!lora_info_.hw_ready) {
            render_boot_diag_view();
            return;
        }

        if (lora_info_.rx_event) {
            if (current_view_ == View::Send) {
                pending_rx_popup_ = true;
            } else {
                current_view_ = View::Messages;
                rx_popup_started_ms_ = lv_tick_get();
            }
        }
        render_current_view();
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
