/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_lvgl_app_runner.hpp"
#include "sample_log.h"
#include "ui/launcher_media_controls.h"
#include "ui/ui.h"
#include "ui/ui_screensaver.h"

#include <cstdlib>
#include <string>
#include <utility>

#if CONFIG_BACKWARD_CPP_ENABLED
#define BACKWARD_HAS_DW 1
#include "backward.hpp"
#include "backward.h"
#endif

// Sample files are copied into ~/Downloads once per user, not whenever hello.md is missing:
// otherwise deleting the samples brings all of them back at the next boot. A home that
// already has the samples (from before this marker existed) is only marked, not re-extracted.
// The marker is written only once the samples are really there, so a missing or failed
// template.zip is retried at the next boot. unzip -n never overwrites a file.
const char *bash_init =
    "m=\"$HOME/.local/state/APPLaunch/samples-installed\"; "
    "if [ ! -e \"$m\" ] ; then "
    "if [ -f \"$HOME/Downloads/hello.md\" ] || unzip -nq /var/template.zip -d \"$HOME/Downloads/\" ; then "
    "mkdir -p \"${m%/*}\" && touch \"$m\" ; "
    "fi ; sync ; fi";

int main(void)
{
    system(bash_init);
    Cp0LvglRunOptions options;
    options.after_resource_init = []() {
        cp0_signal_settings_api({"GpioSet", "BACKLIGHT", "0"}, [](int code, std::string data) {
            if (code == 0)
                SLOGI("[BOOT] set m5ioe1 line 9 low");
            else
                SLOGE("[BOOT] failed to set m5ioe1 line 9 low: %s", data.c_str());
        });
        if (launcher_media_controls::restore_startup_backlight())
            SLOGI("[BOOT] restored working backlight brightness");
        else
            SLOGE("[BOOT] failed to restore working backlight brightness");
    };
    options.setup = []() {
        SLOGI("[BOOT] cp0_lvgl initialized");
        launcher_ui::init();
        ui_screensaver_init();
        return true;
    };
    options.teardown = []() { launcher_ui::deinit(); };
    return cp0_lvgl_run(std::move(options));
}
