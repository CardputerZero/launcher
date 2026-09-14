/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "lvgl/lvgl.h"

// Window = device body size (scaled 2x from the real ~95x55mm card)
#define EMU_WINDOW_W  760
#define EMU_WINDOW_H  520

// LCD in the device
#define LCD_W  320
#define LCD_H  170

struct DeviceSkin {
    lv_obj_t *body;
    lv_obj_t *lcd_bezel;
    lv_obj_t *lcd_viewport;   // 320x170, real UI goes here
    lv_obj_t *keyboard_area;  // button matrix for keys
    lv_obj_t *left_btns;      // left side buttons (Aa, fn, ctrl, opt, alt)
    lv_obj_t *right_btns;     // right side (POWER, HOME, NEXT, TALK)
    lv_obj_t *status_bar;     // SYM/FN/SHIFT indicators
    lv_obj_t *lbl_sym;
    lv_obj_t *lbl_fn;
    lv_obj_t *lbl_shift;
    lv_obj_t *lbl_title;      // "M5 CARDPUTER ZERO"
};

void device_skin_create(DeviceSkin *skin);
lv_obj_t *device_skin_get_lcd(DeviceSkin *skin);
void device_skin_update_modifiers(DeviceSkin *skin, bool sym, bool fn, bool shift);
