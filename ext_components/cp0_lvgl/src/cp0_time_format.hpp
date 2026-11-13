/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <string_view>

namespace cp0::time {

/**
 * @brief Presents 24-hour clock text as US 12-hour time.
 *
 * cp0_time_str() keeps its documented "HH:MM" contract because callers such as
 * the Date & Time page parse that value. This helper only changes how the same
 * value is presented, e.g. on the application top bar.
 *
 * @param hh_mm    Exactly five characters: a two-digit hour 00..23, ':' and a
 *                 two-digit minute 00..59.
 * @param out      Destination buffer. Untouched when the call fails.
 * @param out_size Destination capacity in bytes, including the terminator.
 * @return true when hh_mm was a valid 24-hour clock and out holds the 12-hour
 *         text ("h:MM AM" / "h:MM PM"); false otherwise.
 */
bool format_12_hour(std::string_view hh_mm, char *out, std::size_t out_size) noexcept;

} // namespace cp0::time
