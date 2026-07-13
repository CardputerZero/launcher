#pragma once

namespace setting {

struct AdbStatus {
    bool valid = false;
    bool active = false;
    bool enabled = false;
};

AdbStatus parse_adb_status(const char *output);
bool adb_toggle_succeeded(int result, int exit_code);
bool adb_reboot_required(int result, int exit_code);

} // namespace setting
