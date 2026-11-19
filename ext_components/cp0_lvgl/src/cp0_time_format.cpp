/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_time_format.hpp"

#include <cstdio>

namespace cp0::time {

namespace {

bool two_digits(std::string_view text, std::size_t offset, int &value) noexcept
{
    if (offset + 1 >= text.size()) return false;
    const char high = text[offset];
    const char low  = text[offset + 1];
    if (high < '0' || high > '9' || low < '0' || low > '9') return false;
    value = (high - '0') * 10 + (low - '0');
    return true;
}

} // namespace

bool format_12_hour(std::string_view hh_mm, char *out, std::size_t out_size) noexcept
{
    // "12:59 PM" is the widest result; requiring room for it up front keeps a
    // failed call from touching the caller's buffer at all.
    constexpr std::size_t kMaxFormattedBytes = sizeof("12:59 PM");
    if (!out || out_size < kMaxFormattedBytes) return false;

    int hour   = 0;
    int minute = 0;
    if (hh_mm.size() != 5 || hh_mm[2] != ':' || !two_digits(hh_mm, 0, hour) ||
        !two_digits(hh_mm, 3, minute) || hour > 23 || minute > 59)
        return false;

    const char *meridiem   = hour < 12 ? "AM" : "PM";
    const int display_hour = hour % 12 == 0 ? 12 : hour % 12;
    const int written =
        std::snprintf(out, out_size, "%d:%02d %s", display_hour, minute, meridiem);
    return written > 0 && static_cast<std::size_t>(written) < out_size;
}

} // namespace cp0::time
