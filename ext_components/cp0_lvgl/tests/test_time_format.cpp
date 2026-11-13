/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_time_format.hpp"

#include <cassert>
#include <cstring>
#include <string>

using cp0::time::format_12_hour;

namespace {

std::string formatted(const char *text)
{
    char buffer[16] = {};
    if (!format_12_hour(text, buffer, sizeof(buffer))) return "<invalid>";
    return buffer;
}

void expect(const char *input, const char *expected)
{
    char buffer[16] = {};
    assert(format_12_hour(input, buffer, sizeof(buffer)));
    assert(std::strcmp(buffer, expected) == 0);
}

} // namespace

int main()
{
    // Midnight and noon are the two cases that are easy to get wrong: neither
    // maps to hour 0.
    expect("00:00", "12:00 AM");
    expect("00:05", "12:05 AM");
    expect("00:59", "12:59 AM");
    expect("12:00", "12:00 PM");
    expect("12:34", "12:34 PM");

    // Morning keeps the AM suffix; the leading hour zero is dropped.
    expect("01:05", "1:05 AM");
    expect("09:09", "9:09 AM");
    expect("11:59", "11:59 AM");

    // Afternoon/evening wrap back to a 1..12 hour.
    expect("13:05", "1:05 PM");
    expect("19:44", "7:44 PM");
    expect("23:59", "11:59 PM");
    expect("10:44", "10:44 AM");

    // Minutes are always two digits, even when they contain zeros.
    expect("07:00", "7:00 AM");
    expect("21:07", "9:07 PM");

    // Anything that is not a valid 24-hour clock is rejected, and the caller's
    // buffer is left untouched so the previous text cannot be half-overwritten.
    const char *invalid[] = {
        "",
        "1:05",
        "01:5",
        "0105",
        "24:00",
        "23:60",
        "aa:bb",
        " 1:05",
        "-1:05",
        "01-05",
        "01:005",
    };
    for (const char *input : invalid) {
        char buffer[16] = "keep";
        assert(!format_12_hour(input, buffer, sizeof(buffer)));
        assert(std::strcmp(buffer, "keep") == 0);
    }

    // A too-small buffer fails instead of truncating the result.
    char small[8] = "keep";
    assert(!format_12_hour("10:44", small, sizeof(small)));
    assert(std::strcmp(small, "keep") == 0);

    // A one-byte buffer cannot hold even an empty string; make sure the helper
    // does not write a truncated result.
    char tiny[4] = "abc";
    assert(!format_12_hour("13:05", tiny, 1));
    assert(tiny[0] == 'a');

    // Null/zero-size output is rejected rather than crashing.
    char buffer[16] = {};
    assert(!format_12_hour("10:44", nullptr, sizeof(buffer)));
    assert(!format_12_hour("10:44", buffer, 0));

    assert(formatted("10:44") == "10:44 AM");
    assert(formatted("nope!") == "<invalid>");

    return 0;
}
