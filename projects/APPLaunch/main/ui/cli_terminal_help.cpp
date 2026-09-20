/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cli_terminal_help.hpp"

lv_obj_t *create_cli_help(lv_obj_t *parent)
{
    if (!parent) return nullptr;

    lv_obj_t *overlay = lv_obj_create(parent);
    if (!overlay) return nullptr;
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 320, 150);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x10151C), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(overlay, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(overlay, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *label = lv_label_create(overlay);
    if (!label) {
        lv_obj_delete(overlay);
        return nullptr;
    }
    lv_obj_set_pos(label, 6, 1);
    lv_obj_set_size(label, 308, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF0F6FC), 0);
    lv_obj_set_style_text_line_space(label, 0, 0);
    lv_label_set_text(label,
                      "A terminal for command-line tools.\n\n"
                      "Supports two display modes: Fit to Width, which may be cramped on the "
                      "small screen, and Viewport, which lets you pan around a larger terminal "
                      "area.\n\n"
                      "fn + Z / C: move cursor left / right\n"
                      "fn + F / X: previous / next command\n"
                      "fn + K: move cursor to line start (Home)\n"
                      "fn + N: move cursor to line end (End)\n"
                      "shift + fn + L: page up\n"
                      "shift + fn + M: page down\n\n"
                      "F6 (fn + 6): enter / exit Viewport mode\n"
                      "F4/F5/F7/F8 (fn + 4/5/7/8): pan the viewport");
    return overlay;
}
