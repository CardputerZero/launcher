/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lockscreen_wallpaper.hpp"

#include "cp0_config_json.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <utility>
#include <vector>

namespace lockscreen_wallpaper {
namespace {

std::string trim(const std::string &value)
{
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin])))
        ++begin;
    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1])))
        --end;
    return value.substr(begin, end - begin);
}

bool usable_entry(const std::string &entry)
{
    return !entry.empty() && entry.find('\0') == std::string::npos;
}

std::string config_directory(const std::string &home)
{
    if (home.empty()) return {};
    std::string directory = home;
    if (directory.back() != '/') directory.push_back('/');
    directory += ".config/APPLaunch";
    return directory;
}

} // namespace

std::string home_directory()
{
    const char *home = std::getenv("HOME");
    return home && home[0] ? home : "/root";
}

std::string config_file_path(const std::string &home)
{
    const std::string directory = config_directory(home);
    if (directory.empty()) return {};
    return directory + "/config.json";
}

ConfigStatus parse_entry(const std::string &json_text, std::string &out_entry)
{
    out_entry.clear();
    if (json_text.empty()) return ConfigStatus::Missing;

    std::vector<std::pair<std::string, std::string>> entries;
    if (!cp0cfg::from_json(json_text, entries)) return ConfigStatus::Invalid;

    for (const auto &entry : entries) {
        if (entry.first != kConfigKey) continue;
        const std::string value = trim(entry.second);
        if (!usable_entry(value)) return ConfigStatus::Missing;
        out_entry = value;
        return ConfigStatus::Found;
    }
    return ConfigStatus::Missing;
}

std::string resolve_path(const std::string &entry, const std::string &home)
{
    std::string path = trim(entry);
    if (!usable_entry(path)) return {};

    if (path == "~") {
        path = home;
    } else if (path.rfind("~/", 0) == 0) {
        path = home + path.substr(1);
    } else if (path.front() != '/') {
        const std::string directory = config_directory(home);
        if (directory.empty()) return {};
        path = directory + "/" + path;
    }
    if (path.empty()) return {};

    /* Collapse ./ and ../ segments so a relative entry behaves the way a shell
     * user expects; the file itself is opened with the normalized path. */
    return std::filesystem::path(path).lexically_normal().string();
}

Choice choose(const std::string &json_text,
              const std::string &home,
              const std::function<bool(const std::string &)> &is_readable,
              const std::string &fallback_path)
{
    Choice choice;
    choice.path = fallback_path;

    std::string entry;
    choice.status = parse_entry(json_text, entry);
    if (choice.status != ConfigStatus::Found) return choice;

    const std::string candidate = resolve_path(entry, home);
    if (candidate.empty()) {
        choice.status = ConfigStatus::Missing;
        return choice;
    }

    choice.candidate = candidate;
    if (!is_readable || is_readable(candidate)) {
        choice.path = candidate;
        choice.overridden = true;
    }
    return choice;
}

} // namespace lockscreen_wallpaper
