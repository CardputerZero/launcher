/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "global_hint_policy.hpp"

#include "input_keys.h"

#include <cstring>

#ifndef KEY_RIGHTSHIFT
#define KEY_RIGHTSHIFT 54
#endif

#ifndef KEY_COMPOSE
#define KEY_COMPOSE 127
#endif

#ifndef KEY_FN
#define KEY_FN 0x1d0
#endif

#ifndef KEY_MUTE
#define KEY_MUTE 113
#endif
#ifndef KEY_VOLUMEDOWN
#define KEY_VOLUMEDOWN 114
#endif
#ifndef KEY_VOLUMEUP
#define KEY_VOLUMEUP 115
#endif
#ifndef KEY_BRIGHTNESSDOWN
#define KEY_BRIGHTNESSDOWN 224
#endif
#ifndef KEY_BRIGHTNESSUP
#define KEY_BRIGHTNESSUP 225
#endif

namespace {

bool is_symbol_key_name(const char *name)
{
    if (!name || !name[0])
        return false;
    return std::strcmp(name, "Multi_key") == 0 || std::strcmp(name, "Menu") == 0 ||
           std::strcmp(name, "Sym") == 0 || std::strcmp(name, "SYM") == 0;
}

} // namespace

GlobalHintAction GlobalHintPolicy::action_for(const GlobalHintKeyInput &input) const
{
    if (input.pressed || input.repeated) {
        switch (input.key_code) {
            case KEY_BRIGHTNESSUP:
                return GlobalHintAction::BRIGHTNESS_UP;
            case KEY_BRIGHTNESSDOWN:
                return GlobalHintAction::BRIGHTNESS_DOWN;
            case KEY_VOLUMEUP:
                return GlobalHintAction::VOLUME_UP;
            case KEY_VOLUMEDOWN:
                return GlobalHintAction::VOLUME_DOWN;
            case KEY_MUTE:
                return input.pressed ? GlobalHintAction::TOGGLE_MUTE : GlobalHintAction::NONE;
            default:
                break;
        }
    }

    if (!input.pressed)
        return GlobalHintAction::NONE;

    if (input.key_code == KEY_S && input.control && input.alt)
        return GlobalHintAction::TAKE_SCREENSHOT;

    if (input.key_code == KEY_FN)
        return GlobalHintAction::NONE;

    if (input.key_code == KEY_LEFTSHIFT || input.key_code == KEY_RIGHTSHIFT ||
        input.key_code == KEY_COMPOSE || is_symbol_key_name(input.symbol_name)) {
        return GlobalHintAction::SHOW_LOCK_HINT;
    }

    return GlobalHintAction::NONE;
}
