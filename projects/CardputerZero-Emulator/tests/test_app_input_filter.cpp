/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "lvgl/lvgl.h"
#include "keyboard_input.h"
#include "ui_screensaver.h"

#include <cstdio>
#include <cstdlib>

extern "C" void ui_init(void);
extern "C" void ui_deinit(void);

namespace {
cp0_keyboard_key_filter_t installed;
cp0_keyboard_key_filter_t battery_previous;
bool locked, battery_visible;
unsigned lock_calls, battery_calls, base_calls, battery_restores;

void check(bool condition, const char *message)
{
    if (condition) return;
    std::fprintf(stderr, "input-filter test failed: %s\n", message);
    std::exit(1);
}

int base_filter(const key_item *)
{
    ++base_calls;
    return 3;
}

int battery_filter(const key_item *item)
{
    ++battery_calls;
    return battery_visible ? 2 : battery_previous ? battery_previous(item) : 0;
}

int foreign_filter(const key_item *) { return 4; }

void reset_counts() { lock_calls = battery_calls = base_calls = 0; }
}

// Only the UI initialization calls are stubbed. The test links the production
// app_launch_entry.cpp, uses the real public headers, and exercises its filters.
extern "C" lv_display_t *lv_display_get_default(void) { return nullptr; }
extern "C" lv_obj_t *lv_display_get_screen_active(lv_display_t *) { return nullptr; }
extern "C" lv_obj_t *lv_screen_active(void) { return nullptr; }
extern "C" lv_result_t lv_obj_invalidate(const lv_obj_t *) { return LV_RESULT_OK; }
extern "C" void lv_refr_now(lv_display_t *) {}
extern "C" void ui_screensaver_init(void) {}
extern "C" int ui_screensaver_filter_key(const key_item *)
{
    ++lock_calls;
    return locked ? 1 : 0;
}
extern "C" void cp0_keyboard_set_key_filter(cp0_keyboard_key_filter_t filter)
{
    installed = filter;
}
extern "C" cp0_keyboard_key_filter_t cp0_keyboard_get_key_filter(void)
{
    return installed;
}

namespace launcher_ui {
void init()
{
    // Model the low-battery reminder's nested installation during app init.
    battery_previous = cp0_keyboard_get_key_filter();
    cp0_keyboard_set_key_filter(battery_filter);
}

void deinit()
{
    // Same guarded unwind used by shutdown_warning()/restore_key_filter().
    if (cp0_keyboard_get_key_filter() == battery_filter) {
        ++battery_restores;
        cp0_keyboard_set_key_filter(battery_previous);
    }
    battery_previous = nullptr;
}
}

int main()
{
    key_item item{};
    item.key_state = KBD_KEY_PRESSED;
    for (unsigned cycle = 0; cycle != 3; ++cycle) {
        installed = base_filter;
        locked = battery_visible = true;
        const unsigned restores_before = battery_restores;
        ui_init();
        reset_counts();
#if defined(__APPLE__)
        check(installed != battery_filter, "Darwin shim installed after battery filter");
        check(installed(&item) == 1, "lock screen consumes before visible reminder");
        check(lock_calls == 1 && battery_calls == 0 && base_calls == 0,
              "locked input never reaches reminder or underlying UI");
#else
        check(installed == battery_filter, "non-Darwin chain remains unchanged");
#endif
        locked = false;
        reset_counts();
        check(installed(&item) == 2, "unlocked input reaches visible reminder");
        check(battery_calls == 1 && base_calls == 0, "reminder consumes before base filter");
        battery_visible = false;
        reset_counts();
        check(installed(&item) == 3, "unlocked input reaches original filter");
        check(battery_calls == 1 && base_calls == 1, "predecessors called once");
        locked = battery_visible = true;
        ui_deinit();
        check(battery_restores == restores_before + 1,
              "app shim restored reminder before shutdown_warning unwinds it");
        check(installed == base_filter, "reset restores original filter with both overlays active");
    }

    installed = base_filter;
    ui_init();
    installed = foreign_filter;
    const unsigned restores_before = battery_restores;
    ui_deinit();
    check(installed == foreign_filter, "teardown preserves another owner's replacement");
    check(battery_restores == restores_before, "reminder guard also preserves replacement");
    std::puts("app input filter: priority, nested teardown, reset, and ownership passed");
}
