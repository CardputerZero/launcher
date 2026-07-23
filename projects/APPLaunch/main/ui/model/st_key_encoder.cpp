/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "st_key_encoder.hpp"

#include "input_keys.h"

#include <cstring>

std::string STKeyEncoder::encode(uint32_t evdev_key, const char *utf8,
                                 bool application_cursor_mode)
{
    switch (evdev_key) {
    case KEY_ENTER: return "\r";
    case KEY_BACKSPACE: return std::string(1, static_cast<char>(0x7f));
    case KEY_ESC: return "\x1b";
    case KEY_UP: return application_cursor_mode ? "\x1bOA" : "\x1b[A";
    case KEY_DOWN: return application_cursor_mode ? "\x1bOB" : "\x1b[B";
    case KEY_RIGHT: return application_cursor_mode ? "\x1bOC" : "\x1b[C";
    case KEY_LEFT: return application_cursor_mode ? "\x1bOD" : "\x1b[D";
    default: break;
    }

    if (!utf8) return {};
    const size_t length = std::strlen(utf8);
    if (length == 0 || length > MAX_TEXT_BYTES) return {};
    return std::string(utf8, length);
}
