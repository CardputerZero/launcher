/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "settings_software_model.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string_view>
#include <utility>

namespace settings_software {
namespace {

constexpr const char *kNotInstalled = "Not installed";
constexpr const char *kNoDescription = "No description available";

std::string trim(std::string_view value)
{
    const auto is_space = [](unsigned char character) {
        return std::isspace(character) != 0;
    };
    const auto begin = std::find_if_not(value.begin(), value.end(), is_space);
    const auto end = std::find_if_not(value.rbegin(), value.rend(), is_space).base();
    return begin < end ? std::string(begin, end) : std::string{};
}

struct StatusEntry {
    std::string package;
    std::string status;
    std::string version;
    std::string description;
    std::string long_description;
};

bool is_installed_status(const std::string &status)
{
    return status == "install ok installed" || status == "hold ok installed";
}

void flush_entry(std::map<std::string, StatusEntry> &entries, StatusEntry &entry)
{
    if (entry.package.empty()) return;
    if (!is_installed_status(entry.status)) return;
    entries[entry.package] = std::move(entry);
}

std::string description_text(const StatusEntry &entry)
{
    if (entry.description.empty() && entry.long_description.empty())
        return kNoDescription;
    if (entry.long_description.empty()) return entry.description;
    if (entry.description.empty()) return entry.long_description;
    return entry.description + "\n" + entry.long_description;
}

bool parse_boolean(const std::string &value, bool &result)
{
    if (value == "0") {
        result = false;
        return true;
    }
    if (value == "1") {
        result = true;
        return true;
    }
    return false;
}

bool parse_nonnegative_int(const std::string &value, int &result)
{
    if (value.empty()) return false;
    errno = 0;
    char *end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (errno == ERANGE || end == value.c_str() || *end != '\0' || parsed < 0 ||
        parsed > 1000000)
        return false;
    result = static_cast<int>(parsed);
    return true;
}

std::vector<std::string> split(const std::string &value, char separator)
{
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const std::size_t end = value.find(separator, begin);
        fields.push_back(value.substr(
            begin, end == std::string::npos ? std::string::npos : end - begin));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return fields;
}

} // namespace

const std::vector<AppDefinition> &app_definitions()
{
    static const std::vector<AppDefinition> definitions = {
        {"recorder", "m5cardputerzero-recorder", "Recorder",
         "m5cardputerzero-recorder_"},
        {"compass", "m5cardputerzero-compass", "Compass",
         "m5cardputerzero-compass_"},
        {"camera", "camera", "CameraApp", "Camera_"},
        {"factorytest", "factorytest", "FactoryTest", "FactoryTest_"},
        {"files", "m5cardputerzero-files", "Files", "m5cardputerzero-files_"},
        {"music", "m5cardputerzero-music", "Music", "m5cardputerzero-music_"},
        {"ir-chat", "m5cardputerzero-ir-chat", "IR-Chat",
         "m5cardputerzero-ir-chat_"},
        {"piano", "m5cardputerzero-piano", "Piano",
         "m5cardputerzero-piano_"},
        {"ir-remote", "m5cardputerzero-ir-remote", "IR-Remote",
         "m5cardputerzero-ir-remote_"},
        {"cap-cc1101-subg-chat", "m5cardputerzero-cap-cc1101-subg-chat",
         "Cap-CC1101-SubG-Chat",
         "m5cardputerzero-cap-cc1101-subg-chat_"},
        {"cap-cc1101-nfc", "m5cardputerzero-cap-cc1101-nfc",
         "Cap-CC1101-NFC", "m5cardputerzero-cap-cc1101-nfc_"},
        {"cap-lora-1262", "m5cardputerzero-cap-lora-1262", "Cap-LoRa-1262",
         "m5cardputerzero-cap-lora-1262_"},
        {"cap-lora-1262-gps", "m5cardputerzero-cap-lora-1262-gps",
         "Cap-LoRa-1262-GPS", "m5cardputerzero-cap-lora-1262-gps_"},
        {"keyboard-guide", "m5cardputerzero-keyboard-guide", "Keyboard-Guide",
         "m5cardputerzero-keyboard-guide_"},
    };
    return definitions;
}

std::string short_name(const std::string &package)
{
    constexpr const char *prefix = "m5cardputerzero-";
    const std::string prefix_text(prefix);
    if (package.rfind(prefix_text, 0) == 0 && package.size() > prefix_text.size())
        return package.substr(prefix_text.size());
    return package;
}

std::vector<InstalledApp> read_installed_apps(const std::string &status_path)
{
    std::map<std::string, StatusEntry> entries;
    std::ifstream file(status_path);
    if (file) {
        StatusEntry entry;
        bool in_description = false;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) {
                flush_entry(entries, entry);
                entry = StatusEntry{};
                in_description = false;
                continue;
            }
            if (!line.empty() && line.front() == ' ') {
                if (!in_description) continue;
                std::string continuation = line.substr(1);
                if (continuation == ".") {
                    if (!entry.long_description.empty())
                        entry.long_description.push_back('\n');
                    continue;
                }
                if (!entry.long_description.empty())
                    entry.long_description.push_back(' ');
                entry.long_description += trim(continuation);
                continue;
            }

            const std::size_t separator = line.find(':');
            if (separator == std::string::npos) {
                in_description = false;
                continue;
            }
            const std::string field = line.substr(0, separator);
            const std::string value = trim(line.substr(separator + 1));
            in_description = field == "Description";
            if (field == "Package") {
                entry.package = value;
            } else if (field == "Status") {
                entry.status = value;
            } else if (field == "Version") {
                entry.version = value;
            } else if (field == "Description") {
                entry.description = value;
            }
        }
        flush_entry(entries, entry);
    }

    std::vector<InstalledApp> apps;
    apps.reserve(app_definitions().size());
    for (const AppDefinition &definition : app_definitions()) {
        InstalledApp app;
        app.package = definition.package;
        app.short_name = definition.short_name;
        const auto found = entries.find(app.package);
        if (found != entries.end()) {
            app.installed = true;
            app.version = found->second.version.empty() ? "--" : found->second.version;
            app.description = description_text(found->second);
        } else {
            app.description = kNotInstalled;
        }
        apps.push_back(std::move(app));
    }
    return apps;
}

bool parse_check_result(const std::string &payload, CheckResult &result)
{
    constexpr const char *marker = "software|";
    const std::size_t marker_position = payload.find(marker);
    if (marker_position == std::string::npos) return false;

    result = CheckResult{};
    const std::string body = payload.substr(marker_position + std::char_traits<char>::length(marker));
    for (const std::string &token : split(body, '|')) {
        const std::size_t separator = token.find('=');
        if (separator == std::string::npos) continue;
        const std::string key = token.substr(0, separator);
        const std::string value = token.substr(separator + 1);

        if (key == "updates") {
            for (const std::string &item : split(value, ';')) {
                const std::size_t equals = item.find('=');
                if (equals == std::string::npos || equals == 0) continue;
                const std::string package = item.substr(0, equals);
                const std::string versions = item.substr(equals + 1);
                const std::size_t arrow = versions.find('>');
                if (arrow == std::string::npos) continue;
                UpdateInfo update;
                update.package = package;
                update.installed = versions.substr(0, arrow);
                update.latest = versions.substr(arrow + 1);
                if (update.package.empty() || update.latest.empty()) continue;
                result.updates.push_back(std::move(update));
            }
        } else if (key == "checked") {
            int parsed = 0;
            if (parse_nonnegative_int(value, parsed)) result.checked = parsed;
        } else if (key == "failed") {
            int parsed = 0;
            if (parse_nonnegative_int(value, parsed)) result.failed = parsed;
        } else if (key == "skipped") {
            int parsed = 0;
            if (parse_nonnegative_int(value, parsed)) result.skipped = parsed;
        } else if (key == "launcher_known") {
            bool parsed = false;
            if (parse_boolean(value, parsed)) result.launcher_known = parsed;
        } else if (key == "launcher_available") {
            bool parsed = false;
            if (parse_boolean(value, parsed)) {
                result.launcher_known = true;
                result.launcher_available = parsed;
            }
        } else if (key == "launcher_failed") {
            bool parsed = false;
            if (parse_boolean(value, parsed)) result.launcher_failed = parsed;
        } else if (key == "launcher_installed") {
            result.launcher_installed = value;
        } else if (key == "launcher_version") {
            result.launcher_version = value;
        } else if (key == "launcher_commit") {
            result.launcher_commit = value;
        }
    }
    return true;
}

bool parse_app_update_status(const std::string &status, AppUpdateState &result)
{
    result = AppUpdateState{};
    if (status.empty()) return false;

    const auto starts_with = [&status](const char *prefix) {
        return status.rfind(prefix, 0) == 0;
    };
    const auto parse_entries = [](const std::string &value,
                                  std::vector<AppUpdateEntry> &entries) {
        for (const std::string &item : split(value, ',')) {
            if (item.empty()) continue;
            const std::size_t separator = item.find(':');
            if (separator == std::string::npos || separator == 0) continue;
            AppUpdateEntry entry;
            entry.package = item.substr(0, separator);
            entry.version = item.substr(separator + 1);
            entries.push_back(std::move(entry));
        }
    };
    const auto parse_failures = [](const std::string &value,
                                   std::vector<AppUpdateFailure> &failures) {
        for (const std::string &item : split(value, ',')) {
            if (item.empty()) continue;
            const std::size_t separator = item.find(':');
            if (separator == std::string::npos || separator == 0) continue;
            AppUpdateFailure failure;
            failure.package = item.substr(0, separator);
            failure.reason = item.substr(separator + 1);
            failures.push_back(std::move(failure));
        }
    };

    if (status == "starting") {
        result.phase = AppUpdateState::Phase::Starting;
        return true;
    }

    if (starts_with("checking:") || starts_with("downloading:") ||
        starts_with("installing:")) {
        const auto fields = split(status, ':');
        if (fields.size() < 4) return false;
        result.phase = fields[0] == "checking" ? AppUpdateState::Phase::Checking
                       : fields[0] == "downloading" ? AppUpdateState::Phase::Downloading
                                                    : AppUpdateState::Phase::Installing;
        int index = 0;
        int total = 0;
        if (!parse_nonnegative_int(fields[1], index) ||
            !parse_nonnegative_int(fields[2], total))
            return false;
        result.index = index;
        result.total = total;
        result.package = fields[3];
        int updated = 0;
        int failed = 0;
        // Older firmware only writes the three leading fields; the counters are
        // optional so both formats stay readable.
        if (fields.size() >= 5 && parse_nonnegative_int(fields[4], updated))
            result.updated_count = updated;
        if (fields.size() >= 6 && parse_nonnegative_int(fields[5], failed))
            result.failed_count = failed;
        return true;
    }

    if (starts_with("succeeded:")) {
        result.phase = AppUpdateState::Phase::Succeeded;
        const std::string body = status.substr(std::char_traits<char>::length("succeeded:"));
        for (const std::string &token : split(body, '|')) {
            const std::size_t separator = token.find('=');
            if (separator == std::string::npos) continue;
            const std::string key = token.substr(0, separator);
            const std::string value = token.substr(separator + 1);
            if (key == "updated") {
                parse_entries(value, result.updated);
                result.updated_count = static_cast<int>(result.updated.size());
            } else if (key == "failed") {
                parse_failures(value, result.failures);
                result.failed_count = static_cast<int>(result.failures.size());
            }
        }
        return true;
    }

    if (starts_with("failed:")) {
        result.phase = AppUpdateState::Phase::Failed;
        result.detail = status.substr(std::char_traits<char>::length("failed:"));
        return true;
    }

    if (status == "cancelled" || status == "canceled") {
        result.phase = AppUpdateState::Phase::Cancelled;
        return true;
    }

    if (starts_with("cancelling:")) {
        result.phase = AppUpdateState::Phase::Cancelling;
        result.detail = status.substr(std::char_traits<char>::length("cancelling:"));
        return true;
    }

    return false;
}

bool parse_app_update_warning(const std::string &line, AppUpdateFailure &failure)
{
    failure = AppUpdateFailure{};
    const std::string text = trim(line);
    if (text.empty()) return false;
    const std::size_t separator = text.find(':');
    if (separator == std::string::npos || separator == 0) return false;
    failure.package = text.substr(0, separator);
    failure.reason = text.substr(separator + 1);
    if (failure.package.empty() || failure.reason.empty()) return false;
    return true;
}

std::string app_update_failure_label(const std::string &reason)
{
    if (reason == "download") return "download failed";
    if (reason == "install") return "install failed";
    if (reason == "verify") return "verification failed";
    if (reason == "invalid-package") return "invalid package";
    if (reason == "metadata" || reason == "release" || reason == "version")
        return "check failed";
    if (reason == "busy") return "updater busy";
    if (reason.empty()) return "failed";
    return reason;
}

std::vector<AppUpdateFailure> read_app_update_warnings(const std::string &path)
{
    std::vector<AppUpdateFailure> failures;
    if (path.empty()) return failures;
    std::ifstream input(path);
    if (!input) return failures;
    std::string line;
    while (std::getline(input, line)) {
        AppUpdateFailure failure;
        if (parse_app_update_warning(line, failure))
            failures.push_back(std::move(failure));
    }
    return failures;
}

AppUpdateResolution resolve_app_update(
    const std::vector<UpdateInfo> &expected,
    const std::vector<InstalledPackage> &installed,
    const AppUpdateState &backend,
    const std::vector<AppUpdateFailure> &warnings)
{
    AppUpdateResolution resolution;
    const auto find_installed = [&installed](const std::string &package)
        -> const InstalledPackage * {
        for (const auto &entry : installed)
            if (entry.package == package) return &entry;
        return nullptr;
    };
    const auto in_updated = [&resolution](const std::string &package) {
        for (const auto &entry : resolution.updated)
            if (entry.package == package) return true;
        return false;
    };
    const auto in_failed = [&resolution](const std::string &package) {
        for (const auto &entry : resolution.failed)
            if (entry.package == package) return true;
        return false;
    };
    const auto add_updated = [&](const std::string &package,
                                 const std::string &version) {
        if (in_updated(package)) return;
        resolution.updated.push_back({package, version});
    };
    const auto add_failed = [&](const std::string &package,
                                const std::string &reason) {
        if (in_updated(package) || in_failed(package)) return;
        resolution.failed.push_back({package, reason});
    };
    const auto recorded_reason = [&](const std::string &package,
                                     const std::string &fallback) {
        for (const auto &failure : backend.failures)
            if (failure.package == package && !failure.reason.empty())
                return failure.reason;
        for (const auto &failure : warnings)
            if (failure.package == package && !failure.reason.empty())
                return failure.reason;
        return fallback;
    };

    // The installed state after the run is the ground truth: a package the
    // check offered but that still reports the old version (or disappeared) is
    // a failure, never an update.  This is what used to turn a failed run into
    // "Apps are up to date".
    for (const auto &update : expected) {
        const InstalledPackage *current = find_installed(update.package);
        if (current && current->installed) {
            const bool changed = !current->version.empty() &&
                                 current->version != update.installed;
            if (current->version == update.latest || changed) {
                add_updated(update.package,
                            current->version.empty() ? update.latest
                                                     : current->version);
                continue;
            }
            add_failed(update.package,
                       recorded_reason(update.package, "not updated"));
        } else {
            add_failed(update.package,
                       recorded_reason(update.package, "not installed"));
        }
    }

    for (const auto &entry : backend.updated)
        add_updated(entry.package, entry.version);
    for (const auto &failure : backend.failures)
        add_failed(failure.package, failure.reason);
    for (const auto &failure : warnings)
        add_failed(failure.package, failure.reason);

    // A package that now reports the new version is a success even if the
    // backend recorded an earlier failure for it.
    resolution.failed.erase(
        std::remove_if(resolution.failed.begin(), resolution.failed.end(),
                       [&in_updated](const AppUpdateFailure &failure) {
                           return in_updated(failure.package);
                       }),
        resolution.failed.end());
    return resolution;
}


bool update_changes_version(const UpdateInfo &update)
{
    return !update.installed.empty() && !update.latest.empty() &&
           update.installed != update.latest;
}

std::string update_line(const UpdateInfo &update)
{
    return short_name(update.package) + " " + update.installed + " -> " +
           update.latest;
}

std::string launcher_update_line(const std::string &installed,
                                 const std::string &latest)
{
    return "APPLaunch " + installed + " -> " + latest;
}

} // namespace settings_software
