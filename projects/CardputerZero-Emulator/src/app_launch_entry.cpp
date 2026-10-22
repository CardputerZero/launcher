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
    // Darwin's two-level namespace binds this call to the app's definition.
    // This shim is Apple-only: the host's weak symbol can preempt it on ELF.
    const int consumed = ui_screensaver_filter_key(item);
    if (consumed) return consumed;
    return previous_key_filter ? previous_key_filter(item) : 0;
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
    // Unwind in reverse installation order. launcher_ui::init() installed the
    // low-battery filter before this shim. Restore it first, then let
    // launcher_ui::deinit() -> shutdown_warning() restore its predecessor.
    // A filter installed by another owner must remain untouched by both guards.
    if (cp0_keyboard_get_key_filter() == app_key_filter)
        cp0_keyboard_set_key_filter(previous_key_filter);
    previous_key_filter = nullptr;
#endif
    launcher_ui::deinit();
}
