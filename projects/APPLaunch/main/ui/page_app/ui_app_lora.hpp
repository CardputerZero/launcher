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

namespace Lora_APP
{

void ui_app_lora_create(lv_obj_t* parent, lv_obj_t* root);
void ui_app_lora_set_go_back(std::function<void(void)> go_back);
void ui_app_lora_destroy(void);
void lora_app_task();

static std::function<void(void)> g_go_back_home_fn;

void ui_app_lora_set_go_back(std::function<void(void)> go_back)
{
    g_go_back_home_fn = go_back;
}

enum LoraView {
    LORA_VIEW_MESSAGES = 0,
    LORA_VIEW_INFO,
    LORA_VIEW_SEND,
};

static LoraView g_lora_view = LORA_VIEW_MESSAGES;
static bool g_app_active = false;
static uint32_t g_lora_sent_popup_until_ms = 0;
static char g_lora_tx_input[128] = "";
static cp0_lora_info_t g_lora_info{};

static lv_obj_t *g_ui_parent = nullptr;
static lv_obj_t *g_ui_root = nullptr;
static lv_obj_t *g_title_label = nullptr;
static lv_obj_t *g_content_label = nullptr;
static lv_obj_t *g_info_pins = nullptr;
static lv_obj_t *g_info_device = nullptr;
static lv_obj_t *g_info_mode = nullptr;
static lv_obj_t *g_info_status = nullptr;
static lv_obj_t *g_info_hint = nullptr;
static lv_timer_t *g_lora_timer = nullptr;
static lv_obj_t *g_rx_bubble_bg = nullptr;
static lv_obj_t *g_rx_bubble_lbl = nullptr;
static lv_obj_t *g_tx_bubble_bg = nullptr;
static lv_obj_t *g_tx_bubble_lbl = nullptr;

static void lora_render_current_view(void);
static void lora_render_messages_view(void);
static void lora_render_info_view(void);
static void lora_render_send_view(void);
static void lora_render_sent_popup(void);
static void lora_open_send_view(uint32_t first_key);
static bool is_lora_text_key(uint32_t key);
static char lora_key_to_char(uint32_t key);
static bool handle_app_key(uint32_t key);

static const char *safe_text(const char *text, const char *fallback = "")
{
    return text && text[0] ? text : fallback;
}

static int lora_api_call(const std::list<std::string>& args, cp0_lora_info_t *info = nullptr)
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

static void refresh_lora_info(bool poll)
{
    (void)lora_api_call({poll ? "Poll" : "Info"}, &g_lora_info);
}

static void lora_ui_clear(void)
{
    if (g_title_label) lv_obj_add_flag(g_title_label, LV_OBJ_FLAG_HIDDEN);
    if (g_content_label) lv_obj_add_flag(g_content_label, LV_OBJ_FLAG_HIDDEN);
    if (g_info_pins) lv_obj_add_flag(g_info_pins, LV_OBJ_FLAG_HIDDEN);
    if (g_info_device) lv_obj_add_flag(g_info_device, LV_OBJ_FLAG_HIDDEN);
    if (g_info_mode) lv_obj_add_flag(g_info_mode, LV_OBJ_FLAG_HIDDEN);
    if (g_info_status) lv_obj_add_flag(g_info_status, LV_OBJ_FLAG_HIDDEN);
    if (g_info_hint) lv_obj_add_flag(g_info_hint, LV_OBJ_FLAG_HIDDEN);
    if (g_rx_bubble_bg) lv_obj_add_flag(g_rx_bubble_bg, LV_OBJ_FLAG_HIDDEN);
    if (g_tx_bubble_bg) lv_obj_add_flag(g_tx_bubble_bg, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t* lora_make_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                                  const lv_font_t *font, lv_color_t color, lv_text_align_t align)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text ? text : "");
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_size(lbl, w, h);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl, font ? font : &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(lbl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(lbl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl, align, LV_PART_MAIN | LV_STATE_DEFAULT);
    return lbl;
}

static lv_obj_t* lora_make_bubble(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_color_t bg_color)
{
    lv_obj_t *bg = lv_obj_create(parent);
    lv_obj_set_pos(bg, x, y);
    lv_obj_set_size(bg, w, h);
    lv_obj_set_style_bg_color(bg, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bg, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(bg, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(bg, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(bg, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(bg, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(bg, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(bg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(bg, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(bg, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(bg, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    return bg;
}

static lv_obj_t* lora_make_bubble_label(lv_obj_t *parent, lv_coord_t max_w)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_width(lbl, max_w);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(lbl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return lbl;
}

static void lora_render_boot_diag(void)
{
    lora_ui_clear();
    if (g_title_label) {
        lv_obj_clear_flag(g_title_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_title_label, "LoRa-1262");
    }
    if (g_content_label) {
        lv_obj_clear_flag(g_content_label, LV_OBJ_FLAG_HIDDEN);
        char text[256];
        snprintf(text, sizeof(text), "SPI:%s\n%s\n%s",
                 safe_text(g_lora_info.spi_device, "n/a"),
                 safe_text(g_lora_info.pi4io_status, "I2C status unavailable"),
                 safe_text(g_lora_info.probe_summary, "probe not started"));
        lv_label_set_text(g_content_label, text);
        lv_obj_set_style_text_color(g_content_label, lv_color_hex(0xFF4D4F), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_info_status) {
        lv_obj_clear_flag(g_info_status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_info_status, safe_text(g_lora_info.diag, "LoRa not ready"));
        lv_obj_set_style_text_color(g_info_status, lv_color_hex(0xFF4D4F), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_info_hint) {
        lv_obj_clear_flag(g_info_hint, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_info_hint, safe_text(g_lora_info.probe_display, "Boot diag for SPI check"));
        lv_obj_set_style_text_color(g_info_hint, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void lora_render_page(void)
{
    if (!g_ui_parent) return;
    if (!g_lora_info.hw_ready) {
        lora_render_boot_diag();
        return;
    }
    g_lora_view = LORA_VIEW_MESSAGES;
    lora_render_current_view();
    if (!g_lora_info.tx_mode && !g_lora_info.tx_in_progress) {
        (void)lora_api_call({"StartReceive"});
    }
}

static void lora_render_messages_view(void)
{
    lora_ui_clear();
    if (g_title_label) {
        lv_obj_clear_flag(g_title_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_title_label, "Messages");
    }

    if (g_rx_bubble_bg && g_rx_bubble_lbl) {
        lv_obj_clear_flag(g_rx_bubble_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_rx_bubble_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(g_rx_bubble_bg, 4, 20);
        lv_obj_set_size(g_rx_bubble_bg, 250, 44);
        lv_obj_set_width(g_rx_bubble_lbl, 234);
        if (g_lora_info.last_rx[0]) {
            lv_label_set_text(g_rx_bubble_lbl, g_lora_info.last_rx);
            lv_obj_set_style_text_color(g_rx_bubble_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_label_set_text(g_rx_bubble_lbl, "Waiting for message...");
            lv_obj_set_style_text_color(g_rx_bubble_lbl, lv_color_hex(0xAACCFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    if (g_tx_bubble_bg && g_tx_bubble_lbl) {
        if (g_lora_info.has_sent_message) {
            lv_obj_clear_flag(g_tx_bubble_bg, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(g_tx_bubble_lbl, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(g_tx_bubble_bg, 66, 68);
            lv_obj_set_size(g_tx_bubble_bg, 250, 44);
            lv_obj_set_width(g_tx_bubble_lbl, 234);
            lv_label_set_text(g_tx_bubble_lbl, safe_text(g_lora_info.last_tx));
        } else {
            lv_obj_add_flag(g_tx_bubble_bg, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(g_tx_bubble_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (g_info_status) {
        lv_obj_clear_flag(g_info_status, LV_OBJ_FLAG_HIDDEN);
        char text[128];
        snprintf(text, sizeof(text), "RSSI: %.1fdBm | SNR: %.1fdB", g_lora_info.rssi, g_lora_info.snr);
        lv_label_set_text(g_info_status, text);
        lv_obj_set_style_text_color(g_info_status, lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_info_hint) {
        lv_obj_clear_flag(g_info_hint, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_info_hint, "Type to send | C/Right: Info | ESC: Back");
        lv_obj_set_style_text_color(g_info_hint, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void lora_render_info_view(void)
{
    lora_ui_clear();
    if (g_title_label) {
        lv_obj_clear_flag(g_title_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_title_label, "LoRa Info");
    }
    if (g_info_pins) {
        lv_obj_clear_flag(g_info_pins, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_info_pins, "Role: Client");
        lv_obj_set_style_text_color(g_info_pins, lv_color_hex(0xB8FF9C), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_info_device) {
        lv_obj_clear_flag(g_info_device, LV_OBJ_FLAG_HIDDEN);
        char text[192];
        snprintf(text, sizeof(text), "Device: %s | RX:%s TX:%s",
                 safe_text(g_lora_info.spi_device, "n/a"),
                 g_lora_info.hw_ready ? "ready" : "off",
                 g_lora_info.tx_in_progress ? "busy" : "idle");
        lv_label_set_text(g_info_device, text);
        lv_obj_set_style_text_color(g_info_device, lv_color_hex(0xB8FF9C), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_info_mode) {
        lv_obj_clear_flag(g_info_mode, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_info_mode, safe_text(g_lora_info.probe_display, "Channel: 868MHz | BW:125kHz SF12"));
        lv_obj_set_style_text_color(g_info_mode, lv_color_hex(0xB8FF9C), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_info_status) {
        lv_obj_clear_flag(g_info_status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_info_status, safe_text(g_lora_info.diag, "LoRa ready"));
        lv_obj_set_style_text_color(g_info_status, lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_info_hint) {
        lv_obj_clear_flag(g_info_hint, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_info_hint, "Z/Left: Messages | Type: Send | ESC: Back");
        lv_obj_set_style_text_color(g_info_hint, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void lora_render_send_view(void)
{
    lora_ui_clear();
    if (g_title_label) {
        lv_obj_clear_flag(g_title_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_title_label, "Send");
    }
    if (g_content_label) {
        lv_obj_clear_flag(g_content_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_content_label, g_lora_tx_input[0] ? g_lora_tx_input : "_");
        lv_obj_set_style_text_font(g_content_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(g_content_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(g_content_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_info_status) {
        lv_obj_clear_flag(g_info_status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_info_status, "OK Send | DEL Delete | ESC Cancel");
        lv_obj_set_style_text_color(g_info_status, lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void lora_render_sent_popup(void)
{
    lora_ui_clear();
    if (g_title_label) {
        lv_obj_clear_flag(g_title_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_title_label, "Received");
    }
    if (g_rx_bubble_bg && g_rx_bubble_lbl) {
        lv_obj_clear_flag(g_rx_bubble_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_rx_bubble_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(g_rx_bubble_bg, 4, 22);
        lv_obj_set_size(g_rx_bubble_bg, 312, 86);
        lv_obj_set_width(g_rx_bubble_lbl, 296);
        lv_label_set_text(g_rx_bubble_lbl, safe_text(g_lora_info.last_rx, "<empty>"));
        lv_obj_set_style_text_color(g_rx_bubble_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (g_info_status) {
        lv_obj_clear_flag(g_info_status, LV_OBJ_FLAG_HIDDEN);
        char text[96];
        snprintf(text, sizeof(text), "SNR %.1f  RSSI %.0f", g_lora_info.snr, g_lora_info.rssi);
        lv_label_set_text(g_info_status, text);
        lv_obj_set_style_text_color(g_info_status, lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void lora_render_current_view(void)
{
    if (g_lora_sent_popup_until_ms != 0 && lv_tick_elaps(g_lora_sent_popup_until_ms) < 2000) {
        lora_render_sent_popup();
        return;
    }
    g_lora_sent_popup_until_ms = 0;
    if (g_lora_view != LORA_VIEW_INFO && g_lora_view != LORA_VIEW_SEND) {
        g_lora_view = LORA_VIEW_MESSAGES;
    }
    if (g_lora_view == LORA_VIEW_INFO) lora_render_info_view();
    else if (g_lora_view == LORA_VIEW_SEND) lora_render_send_view();
    else lora_render_messages_view();
}

static void lora_open_send_view(uint32_t first_key)
{
    g_lora_view = LORA_VIEW_SEND;
    g_lora_sent_popup_until_ms = 0;
    g_lora_tx_input[0] = '\0';
    char ch = lora_key_to_char(first_key);
    if (ch != '\0') {
        g_lora_tx_input[0] = ch;
        g_lora_tx_input[1] = '\0';
    }
    lora_render_send_view();
}

static bool is_lora_text_key(uint32_t key)
{
    return (key >= 'A' && key <= 'Z') ||
           (key >= 'a' && key <= 'z') ||
           (key >= '0' && key <= '9') ||
           key == ' ' || key == '-' || key == '_' || key == '.' || key == ',' ||
           key == '!' || key == '?' || key == '#';
}

static char lora_key_to_char(uint32_t key)
{
    if (key >= 'A' && key <= 'Z') return static_cast<char>(key);
    if (key >= 'a' && key <= 'z') return static_cast<char>(key);
    if ((key >= '0' && key <= '9') || key == ' ' || key == '-' || key == '_' ||
        key == '.' || key == ',' || key == '!' || key == '?' || key == '#') {
        return static_cast<char>(key);
    }
    return '\0';
}

static bool is_menu_prev_key(uint32_t key)
{
    return key == LV_KEY_LEFT || key == LV_KEY_PREV || key == 'z' || key == 'Z';
}

static bool is_menu_next_key(uint32_t key)
{
    return key == LV_KEY_RIGHT || key == LV_KEY_NEXT || key == 'c' || key == 'C';
}

static bool handle_app_key(uint32_t key)
{
    bool key_was_z = (key == 'z' || key == 'Z');
    bool key_was_c = (key == 'c' || key == 'C');

    if (key_was_z) key = LV_KEY_LEFT;
    else if (key_was_c) key = LV_KEY_RIGHT;

    if (g_lora_view == LORA_VIEW_SEND) {
        if (key == LV_KEY_ESC) {
            g_lora_view = LORA_VIEW_MESSAGES;
            g_lora_tx_input[0] = '\0';
            lora_render_current_view();
            return true;
        }
        if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
            size_t len = strlen(g_lora_tx_input);
            if (len > 0) g_lora_tx_input[len - 1] = '\0';
            lora_render_send_view();
            return true;
        }
        if (key == LV_KEY_ENTER) {
            if (lora_api_call({"SendText", g_lora_tx_input}) == 0) {
                refresh_lora_info(false);
                g_lora_view = LORA_VIEW_MESSAGES;
                g_lora_sent_popup_until_ms = 0;
                lora_render_current_view();
                g_lora_tx_input[0] = '\0';
            }
            return true;
        }
        if (is_lora_text_key(key)) {
            size_t len = strlen(g_lora_tx_input);
            if (len + 1 < sizeof(g_lora_tx_input)) {
                g_lora_tx_input[len] = lora_key_to_char(key);
                g_lora_tx_input[len + 1] = '\0';
            }
            lora_render_send_view();
            return true;
        }
        return true;
    }

    if (key == LV_KEY_ESC || key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
        if (g_go_back_home_fn) g_go_back_home_fn();
        return true;
    }

    if (is_menu_prev_key(key) || key == LV_KEY_UP) {
        g_lora_view = LORA_VIEW_MESSAGES;
        g_lora_sent_popup_until_ms = 0;
        lora_render_current_view();
        return true;
    } else if (is_menu_next_key(key) || key == LV_KEY_DOWN) {
        g_lora_view = LORA_VIEW_INFO;
        g_lora_sent_popup_until_ms = 0;
        lora_render_current_view();
        return true;
    } else if (key == LV_KEY_ENTER) {
        lora_render_current_view();
        return true;
    } else if (is_lora_text_key(key) && !key_was_z && !key_was_c) {
        lora_open_send_view(key);
        return true;
    }

    return false;
}

static void lora_key_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD)) return;
    auto *elm = static_cast<struct key_item *>(lv_event_get_param(e));
    if (!elm || elm->key_state == 0) return;

    uint32_t key = elm->key_code;
    uint32_t cp = elm->codepoint;

    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
        (cp >= '0' && cp <= '9') || cp == ' ' || cp == '-' || cp == '_' ||
        cp == '.' || cp == ',' || cp == '!' || cp == '?' || cp == '#') {
        key = cp;
    }

    if (key == KEY_UP) key = LV_KEY_UP;
    else if (key == KEY_DOWN) key = LV_KEY_DOWN;
    else if (key == KEY_LEFT) key = LV_KEY_LEFT;
    else if (key == KEY_RIGHT) key = LV_KEY_RIGHT;
    else if (key == KEY_ENTER || key == KEY_KPENTER) key = LV_KEY_ENTER;
    else if (key == KEY_ESC) key = LV_KEY_ESC;
    else if (key == KEY_BACKSPACE) key = LV_KEY_BACKSPACE;
    else if (key == KEY_DELETE) key = LV_KEY_DEL;

    (void)handle_app_key(key);
}

static void lora_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!g_app_active) return;
    refresh_lora_info(true);
    if (g_lora_info.rx_event) {
        g_lora_view = LORA_VIEW_MESSAGES;
        g_lora_sent_popup_until_ms = lv_tick_get();
    }
    lora_render_current_view();
}

void ui_app_lora_create(lv_obj_t* parent, lv_obj_t* root)
{
    if (!parent || !root) return;

    if (g_lora_timer) {
        lv_timer_delete(g_lora_timer);
        g_lora_timer = nullptr;
    }

    g_ui_parent = parent;
    g_ui_root = root;
    g_app_active = true;
    g_lora_view = LORA_VIEW_MESSAGES;
    g_lora_sent_popup_until_ms = 0;

    g_title_label = lora_make_label(parent, "LoRa-1262", 0, 0, 320, 18,
                                     &lv_font_montserrat_14, lv_color_hex(0x8D44FF), LV_TEXT_ALIGN_CENTER);
    g_content_label = lora_make_label(parent, "", 8, 22, 304, 90,
                                       &lv_font_montserrat_12, lv_color_hex(0xFFFFFF), LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(g_content_label, LV_LABEL_LONG_WRAP);
    g_info_pins = lora_make_label(parent, "", 8, 22, 304, 18,
                                   &lv_font_montserrat_12, lv_color_hex(0xB8FF9C), LV_TEXT_ALIGN_LEFT);
    g_info_device = lora_make_label(parent, "", 8, 42, 304, 18,
                                     &lv_font_montserrat_12, lv_color_hex(0xB8FF9C), LV_TEXT_ALIGN_LEFT);
    g_info_mode = lora_make_label(parent, "", 8, 62, 304, 18,
                                   &lv_font_montserrat_12, lv_color_hex(0xB8FF9C), LV_TEXT_ALIGN_LEFT);
    g_info_status = lora_make_label(parent, "", 8, 114, 304, 16,
                                     &lv_font_montserrat_12, lv_color_hex(0xFFD24A), LV_TEXT_ALIGN_LEFT);
    g_info_hint = lora_make_label(parent, "", 8, 132, 304, 14,
                                   &lv_font_montserrat_10, lv_color_hex(0x8AA8FF), LV_TEXT_ALIGN_LEFT);

    g_rx_bubble_bg = lora_make_bubble(parent, 4, 20, 250, 44, lv_color_hex(0x3A7DFF));
    g_rx_bubble_lbl = lora_make_bubble_label(g_rx_bubble_bg, 234);
    lv_obj_add_flag(g_rx_bubble_bg, LV_OBJ_FLAG_HIDDEN);
    g_tx_bubble_bg = lora_make_bubble(parent, 66, 68, 250, 44, lv_color_hex(0x00A854));
    g_tx_bubble_lbl = lora_make_bubble_label(g_tx_bubble_bg, 234);
    lv_obj_add_flag(g_tx_bubble_bg, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(root, lora_key_event_cb, static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), nullptr);

    (void)lora_api_call({"Init"});
    refresh_lora_info(false);
    lora_render_page();
    g_lora_timer = lv_timer_create(lora_timer_cb, 100, nullptr);
}

void lora_app_task()
{
    refresh_lora_info(true);
}

void ui_app_lora_destroy(void)
{
    if (g_lora_timer) {
        lv_timer_delete(g_lora_timer);
        g_lora_timer = nullptr;
    }
    g_app_active = false;
    g_ui_parent = nullptr;
    g_ui_root = nullptr;
    g_title_label = nullptr;
    g_content_label = nullptr;
    g_info_pins = nullptr;
    g_info_device = nullptr;
    g_info_mode = nullptr;
    g_info_status = nullptr;
    g_info_hint = nullptr;
    g_rx_bubble_bg = nullptr;
    g_rx_bubble_lbl = nullptr;
    g_tx_bubble_bg = nullptr;
    g_tx_bubble_lbl = nullptr;
}

} // namespace Lora_APP

class UILoraPage : public AppPage
{
public:
    UILoraPage() : AppPage()
    {
        Lora_APP::ui_app_lora_set_go_back([this]() {
            if (navigate_home) navigate_home();
        });
        Lora_APP::ui_app_lora_create(ui_APP_Container, root_screen_);
    }

    ~UILoraPage()
    {
        Lora_APP::ui_app_lora_set_go_back(nullptr);
        Lora_APP::ui_app_lora_destroy();
    }
};
