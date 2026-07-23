/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_lvgl_app_runner.hpp"
#include "keyboard_input.h"
#include "sample_log.h"
#include "ui/ui.h"
#include "ui/ui_screensaver.h"

#include <utility>

#if CONFIG_BACKWARD_CPP_ENABLED
#define BACKWARD_HAS_DW 1
#include "backward.hpp"
#include "backward.h"
#endif

int main(void)
{
    Cp0LvglRunOptions options;
    options.setup = []() {
        SLOGI("[BOOT] cp0_lvgl initialized");
        if (LV_EVENT_KEYBOARD == 0)
            LV_EVENT_KEYBOARD = lv_event_register_id();
        launcher_ui::init();
        ui_screensaver_init();
        return true;
    };
    options.teardown = []() { launcher_ui::deinit(); };
    return cp0_lvgl_run(std::move(options));
}
