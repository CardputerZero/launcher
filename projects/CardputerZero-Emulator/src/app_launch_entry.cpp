/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lvgl/lvgl.h"
#include "ui_screensaver.h"
#if defined(__APPLE__)
#include "keyboard_input.h"

namespace {
cp0_keyboard_key_filter_t previous_key_filter;

int app_key_filter(const struct key_item *item)
{
    // Keep the lock screen ahead of the app's other input consumers. Calling
    // it from this library also avoids the host's weak stub on Darwin.
    const int consumed = ui_screensaver_filter_key(item);
    return consumed ? consumed : previous_key_filter ? previous_key_filter(item) : 0;
}
}
#endif

namespace launcher_ui {
void init();
void deinit();
}

extern "C" void ui_init(void)
{
    launcher_ui::init();
    ui_screensaver_init();
#if defined(__APPLE__)
    previous_key_filter = cp0_keyboard_get_key_filter();
    cp0_keyboard_set_key_filter(app_key_filter);
#endif
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
}

extern "C" void ui_deinit(void)
{
#if defined(__APPLE__)
    // Restore the app's filter before its teardown removes that filter.
    if (cp0_keyboard_get_key_filter() == app_key_filter)
        cp0_keyboard_set_key_filter(previous_key_filter);
    previous_key_filter = nullptr;
#endif
    launcher_ui::deinit();
}
