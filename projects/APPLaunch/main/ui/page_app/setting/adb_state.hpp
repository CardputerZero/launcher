#pragma once

#include "cp0_lvgl_app.h"

namespace setting {

struct AdbStatus {
    bool valid = false;
    bool active = false;
    bool enabled = false;
};

AdbStatus parse_adb_status(const char *output);
bool adb_toggle_succeeded(cp0_sudo_result_t result, int exit_code);
bool adb_reboot_required(cp0_sudo_result_t result, int exit_code);
bool adb_state_after_failure(const AdbStatus &status, bool previous);

} // namespace setting
