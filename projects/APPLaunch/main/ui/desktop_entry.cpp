/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "desktop_entry.h"

#include <sstream>

namespace {
void trim(std::string &value)
{
    const size_t first = value.find_first_not_of(" \t");
    if (first == std::string::npos) {
        value.clear();
        return;
    }
    const size_t last = value.find_last_not_of(" \t");
    value = value.substr(first, last - first + 1);
}

bool parse_bool(const std::string &value)
{
    return value == "true" || value == "True" || value == "1";
}

bool contains_nul(const std::string &value)
{
    return value.find('\0') != std::string::npos;
}

bool contains_control(const std::string &value)
{
    for (const unsigned char character : value) {
        if (character < 0x20 || character == 0x7F) return true;
    }
    return false;
}
} // namespace

bool desktop_entry_filename_valid(std::string_view name)
{
    constexpr std::string_view suffix = ".desktop";
    if (name.size() <= suffix.size() ||
        name.substr(name.size() - suffix.size()) != suffix)
        return false;
    for (const unsigned char character : name) {
        if (character < 0x20 || character == 0x7F || character == '/' || character == '\\')
            return false;
    }
    return true;
}

std::optional<DesktopEntry> parse_desktop_entry(std::string_view contents)
{
    DesktopEntry entry;
    bool in_desktop_entry = false;
    bool hidden = false;
    bool valid_type = true;
    std::istringstream stream{std::string(contents)};
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line[0] == '[') {
            in_desktop_entry = line == "[Desktop Entry]";
            continue;
        }
        if (!in_desktop_entry) continue;

        const size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        trim(key);
        trim(value);

        if (key == "Name") entry.name = std::move(value);
        else if (key == "Icon") entry.icon = std::move(value);
        else if (key == "Exec") entry.exec = std::move(value);
        else if (key == "Terminal") entry.terminal = parse_bool(value);
        else if (key == "Sysplause") entry.sysplause = parse_bool(value);
        else if (key == "Hidden" || key == "NoDisplay") hidden = hidden || parse_bool(value);
        else if (key == "Type") valid_type = value == "Application";
    }
    if (hidden || !valid_type || entry.name.empty() || entry.exec.empty() ||
        entry.name.size() > DESKTOP_ENTRY_MAX_NAME_BYTES ||
        entry.icon.size() > DESKTOP_ENTRY_MAX_ICON_BYTES ||
        entry.exec.size() > DESKTOP_ENTRY_MAX_EXEC_BYTES ||
        contains_control(entry.name) || contains_control(entry.icon) || contains_nul(entry.exec))
        return std::nullopt;
    return entry;
}
