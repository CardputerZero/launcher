#include "adb_state.hpp"
#include <cstring>

namespace setting {
namespace {
bool has_line(const char *output, const char *line)
{
    if (!output || !line) return false;
    const std::size_t length = std::strlen(line);
    for (const char *p = output; (p = std::strstr(p, line)) != nullptr; ++p) {
        const bool start = p == output || p[-1] == '\n' || p[-1] == '\r';
        const char next = p[length];
        const bool end = next == '\0' || next == '\n' || next == '\r';
        if (start && end) return true;
    }
    return false;
}
} // namespace

AdbStatus parse_adb_status(const char *output)
{
    AdbStatus status;
    if (!output) return status;
    status.active = has_line(output, "adbd=active");
    status.enabled = has_line(output, "enabled=enabled");
    status.valid = status.active || status.enabled || has_line(output, "adbd=inactive") ||
                   has_line(output, "adbd=failed") || has_line(output, "enabled=disabled");
    return status;
}

bool adb_toggle_succeeded(cp0_sudo_result_t result, int exit_code)
{
    return result == CP0_SUDO_RESULT_SUCCESS ||
           (result == CP0_SUDO_RESULT_EXEC_FAILED && exit_code == 10);
}

bool adb_reboot_required(cp0_sudo_result_t result, int exit_code)
{
    return result == CP0_SUDO_RESULT_EXEC_FAILED && exit_code == 10;
}

bool adb_state_after_failure(const AdbStatus &status, bool previous)
{
    return status.valid ? status.active || status.enabled : previous;
}
} // namespace setting
