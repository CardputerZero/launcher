/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "timezone_setup.h"

#include <cstdlib>

namespace launch_wizard {

std::string apply_timezone(const Timezone &timezone, TimezoneMode mode,
                           const TimezoneCommandRunner &run)
{
    if (!timezone.name || !timezone.label)
        return "Timezone is required";
    std::string name = timezone.name;
    bool supported = false;
    for (const auto &entry : kTimezones)
        supported = supported || (name == entry.name &&
            std::string(timezone.label) == entry.label &&
            timezone.daylight_minutes == entry.daylight_minutes);
    if (!supported)
        return "Unsupported timezone";
    if (mode != TimezoneMode::Standard && mode != TimezoneMode::Daylight)
        return "Unsupported time mode";
    if (mode == TimezoneMode::Daylight && !timezone_supports_daylight(timezone))
        return "This region does not use daylight saving time";

    const int minutes = timezone_offset_minutes(timezone, mode);
    const std::string label = timezone_offset_label(minutes);
    if (minutes % 60 == 0) {
        // Etc/GMT uses the opposite (POSIX) sign from the displayed UTC offset.
        name = minutes == 0 ? "Etc/UTC" : "Etc/GMT" +
            std::string(minutes < 0 ? "+" : "-") + std::to_string(std::abs(minutes) / 60);
    } else {
        name = "LaunchWizard/" + label.substr(0, 6) + label.substr(7, 2);
        // zic STDOFF has the same sign as UTC. No RULES or UNTIL means a
        // constant offset for all dates, including beyond the 2038 boundary.
        const std::string source = "Zone " + name + " " + label.substr(3) +
            " - " + label.substr(3, 3) + label.substr(7, 2) + "\n";
        const CommandResult compiled = run(
            {"zic", "-d", "/usr/share/zoneinfo", "-"}, &source);
        if (compiled.code != 0)
            return "Failed to install fixed timezone" +
                (compiled.output.empty() ? std::string() : ": " + compiled.output);
    }

    const CommandResult result = run({"timedatectl", "set-timezone", name}, nullptr);
    return result.code == 0 ? std::string() :
        (result.output.empty() ? "Failed to set timezone" : result.output);
}

} // namespace launch_wizard
