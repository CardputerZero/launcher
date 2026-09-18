/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "timezone_setup.h"

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>

namespace {

bool verify_offset(const launch_wizard::Timezone &zone,
                   launch_wizard::TimezoneMode mode, const std::string &zone_path)
{
    if (!std::filesystem::is_regular_file(zone_path)) {
        std::cerr << "Missing zoneinfo: " << zone_path << '\n';
        return false;
    }
    const std::string label = zone.label;
    const int standard_seconds = (std::stoi(label.substr(4, 2)) * 60 +
                                  std::stoi(label.substr(7, 2))) * 60 *
                                 (label[3] == '-' ? -1 : 1);
    setenv("TZ", (":" + zone_path).c_str(), 1);
    tzset();
    // Cover both hemispheres' DST seasons and dates beyond the 2038 boundary.
    for (int year : {2026, 2027, 2038, 2050}) {
        for (int month = 0; month < 12; ++month) {
            for (int day : {1, 15, 18, 28}) {
                std::tm utc{};
                utc.tm_year = year - 1900;
                utc.tm_mon = month;
                utc.tm_mday = day;
                utc.tm_hour = 12;
                const std::time_t instant = timegm(&utc);
                std::tm local{};
                const bool converted = localtime_r(&instant, &local) != nullptr;
                const bool daylight = mode == launch_wizard::TimezoneMode::Daylight;
                const int seconds = standard_seconds + (daylight ? zone.daylight_minutes * 60 : 0);
                if (!converted || local.tm_gmtoff != seconds ||
                    local.tm_isdst != 0) {
                    std::cerr << label << " (" << zone.name << ") at "
                              << year << '-' << month + 1 << '-' << day
                              << ": expected " << seconds << " seconds, got "
                              << local.tm_gmtoff << ", DST=" << local.tm_isdst << '\n';
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace

int main()
{
    using namespace launch_wizard;
    bool passed = true;
    const auto expect = [&passed](bool condition, const std::string &message) {
        if (!condition) {
            std::cerr << message << '\n';
            passed = false;
        }
    };
    char directory_template[] = "/tmp/launch-wizard-timezones-XXXXXX";
    const char *directory = mkdtemp(directory_template);
    if (!directory) return 1;
    const std::string zoneinfo = directory;
    for (const auto &zone : kTimezones) {
        for (TimezoneMode mode : {TimezoneMode::Standard, TimezoneMode::Daylight}) {
            if (mode == TimezoneMode::Daylight && !timezone_supports_daylight(zone))
                continue;
            std::string installed_name;
            const std::string error = apply_timezone(zone, mode,
                [&](const std::vector<std::string> &args, const std::string *input) {
                    if (args.front() == "zic") {
                        expect(args == std::vector<std::string>{
                            "zic", "-d", "/usr/share/zoneinfo", "-"}, "incorrect zic command");
                        expect(input != nullptr, "missing zone definition");
                        // Only the temporary directory is written; never change the host zone.
                        return run_command_process({LAUNCH_WIZARD_ZIC, "-d", zoneinfo, "-"}, input);
                    }
                    expect(args.size() == 3 && args[0] == "timedatectl" &&
                           args[1] == "set-timezone" && input == nullptr,
                           "incorrect timezone apply command");
                    installed_name = args.back();
                    return CommandResult{};
                });
            expect(error.empty(), error);
            if (!error.empty()) continue;
            const std::string path = (installed_name.find("LaunchWizard/") == 0
                ? zoneinfo : "/usr/share/zoneinfo") + "/" + installed_name;
            passed = verify_offset(zone, mode, path) && passed;
        }
    }

    int calls = 0;
    const TimezoneCommandRunner fail_compile = [&](const auto &args, const auto *) {
        ++calls;
        expect(args.front() == "zic", "applied timezone after compilation failed");
        return CommandResult{1, "compiler unavailable"};
    };
    expect(apply_timezone(kTimezones[8], TimezoneMode::Standard, fail_compile).find(
               "compiler unavailable") != std::string::npos && calls == 1,
           "zone compilation failure was swallowed");
    calls = 0;
    const TimezoneCommandRunner fail_apply = [&](const auto &args, const auto *) {
        ++calls;
        expect(args.front() == "timedatectl", "unexpected timezone command");
        return CommandResult{1, "permission denied"};
    };
    expect(apply_timezone(kTimezones[6], TimezoneMode::Standard, fail_apply) == "permission denied",
           "timezone application failure was swallowed");
    expect(calls == 1, "unexpected number of timezone commands");
    calls = 0;
    expect(!apply_timezone(kTimezones[25], TimezoneMode::Daylight, fail_apply).empty(),
           "Shanghai incorrectly offered daylight saving time");
    expect(!apply_timezone({nullptr, nullptr}, TimezoneMode::Standard, fail_apply).empty(),
           "empty timezone accepted");
    expect(!apply_timezone({"Unknown/City", "UTC+00:00"}, TimezoneMode::Standard, fail_apply).empty(),
           "unknown timezone accepted");
    expect(calls == 0, "invalid timezone selection ran a command");
    std::filesystem::remove_all(zoneinfo);
    if (passed) std::cout << "Timezone modes, seasonal offsets and failure handling passed\n";
    return passed ? 0 : 1;
}
