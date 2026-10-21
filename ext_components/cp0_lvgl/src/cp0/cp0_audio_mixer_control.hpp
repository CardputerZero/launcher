/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstring>
#include <string>

class Cp0AudioMixerControl
{
public:
    static bool parse_volume_line(const char *line, int &percent);
    // "auto_null" is PipeWire's placeholder sink. A volume written to it is
    // discarded when the real hardware sink appears, so callers that must
    // reach the speaker have to tell the two apart.
    // Defined inline because shared callers such as commount.cpp are also built
    // by the emulator and AppStore targets, which do not compile the pactl
    // implementation in cp0_audio_mixer_control.cpp.
    static bool is_hardware_sink_name(const char *name)
    {
        if (!name || !name[0])
            return false;
        return std::strcmp(name, "auto_null") != 0;
    }
    int read_volume() const;
    // Empty when the default sink cannot be resolved.
    std::string default_sink_name() const;
    int write_volume(int percent) const;
    int read_mute() const;
    int toggle_mute() const;

private:
    static int clamp_percent(int percent);
};
