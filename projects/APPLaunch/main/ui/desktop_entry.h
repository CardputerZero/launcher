/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>

struct DesktopEntry
{
    std::string name;
    std::string icon;
    std::string exec;
    bool terminal = false;
    bool sysplause = true;
};

inline constexpr std::size_t DESKTOP_ENTRY_MAX_NAME_BYTES = 128;
inline constexpr std::size_t DESKTOP_ENTRY_MAX_ICON_BYTES = 512;
inline constexpr std::size_t DESKTOP_ENTRY_MAX_EXEC_BYTES = 512;

std::optional<DesktopEntry> parse_desktop_entry(std::string_view contents);
bool desktop_entry_filename_valid(std::string_view name);
