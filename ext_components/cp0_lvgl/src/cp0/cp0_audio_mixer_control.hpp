/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <chrono>
#include <cstring>
#include <string>

class Cp0AudioMixerControl
{
public:
    static bool parse_volume_line(const char *line, int &percent);
    // "auto_null" is PipeWire's placeholder sink. A volume written to it is
    // discarded when the real hardware sink appears, so callers that must
    // reach the speaker have to tell the two apart. While no sink exists at
    // all, pipewire-pulse reports the special name "@DEFAULT_SINK@" instead.
    // Defined inline because shared callers such as commount.cpp are also built
    // by the emulator and AppStore targets, which do not compile the pactl
    // implementation in cp0_audio_mixer_control.cpp.
    static bool is_hardware_sink_name(const char *name)
    {
        if (!name || !name[0] || name[0] == '@')
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

// Decides one pass of the startup volume restore. The loop itself lives in
// the launcher, but the transitions are the part that usually breaks, so they
// stay pure: no clock, no pactl, no signals.
// Defined inline for the same reason as is_hardware_sink_name: commount.cpp is
// built by the emulator and AppStore targets, which do not compile the pactl
// implementation in cp0_audio_mixer_control.cpp.
struct Cp0VolumeRestoreStep
{
    enum class Action
    {
        Wait,  // placeholder or missing sink; do not write
        Write, // hardware sink is not at the saved volume
        Hold,  // volume matches, but the settle window has not elapsed
        Done,  // volume has held on a hardware sink for the settle window
    };

    Action action = Action::Wait;
    int target = -1;
    bool holding = false;
    std::chrono::steady_clock::time_point holding_since{};
};

inline Cp0VolumeRestoreStep cp0_volume_restore_step(
    const Cp0VolumeRestoreStep &previous, const char *sink_name, int saved_volume,
    int current_volume, std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::duration settle)
{
    Cp0VolumeRestoreStep next = previous;
    if (!Cp0AudioMixerControl::is_hardware_sink_name(sink_name)) {
        next.action = Cp0VolumeRestoreStep::Action::Wait;
        next.holding = false;
        return next;
    }

    // A volume change made in the UI while the loop is running wins. A failed
    // config read leaves the last known target in place.
    if (saved_volume >= 0)
        next.target = saved_volume;

    if (current_volume != next.target) {
        next.action = Cp0VolumeRestoreStep::Action::Write;
        next.holding = false;
        return next;
    }

    if (!next.holding) {
        next.action = Cp0VolumeRestoreStep::Action::Hold;
        next.holding = true;
        next.holding_since = now;
        return next;
    }

    next.action = now - next.holding_since >= settle
                      ? Cp0VolumeRestoreStep::Action::Done
                      : Cp0VolumeRestoreStep::Action::Hold;
    return next;
}
