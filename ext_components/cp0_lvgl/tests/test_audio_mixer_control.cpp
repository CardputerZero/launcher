/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_audio_mixer_control.hpp"

#include <cassert>
#include <chrono>

namespace {

void test_volume_lines()
{
    int value = 77;
    assert(!Cp0AudioMixerControl::parse_volume_line(nullptr, value) && value == 77);
    assert(!Cp0AudioMixerControl::parse_volume_line("", value) && value == 77);
    assert(!Cp0AudioMixerControl::parse_volume_line("Volume: junk%", value) && value == 77);
    assert(!Cp0AudioMixerControl::parse_volume_line("Volume: -1%", value) && value == 77);
    assert(!Cp0AudioMixerControl::parse_volume_line("Volume: junk42%", value) && value == 77);
    assert(!Cp0AudioMixerControl::parse_volume_line("Volume: 101%", value) && value == 77);
    assert(!Cp0AudioMixerControl::parse_volume_line("Volume: 999999999999999999999%", value) && value == 77);
    assert(Cp0AudioMixerControl::parse_volume_line("Volume: 0%", value) && value == 0);
    assert(Cp0AudioMixerControl::parse_volume_line("Volume: 100%", value) && value == 100);
    assert(Cp0AudioMixerControl::parse_volume_line("Volume: 42% / 42%", value) && value == 42);
}

void test_sink_names()
{
    assert(!Cp0AudioMixerControl::is_hardware_sink_name(nullptr));
    assert(!Cp0AudioMixerControl::is_hardware_sink_name(""));
    // PipeWire's placeholder sink; volume written here never reaches the speaker.
    assert(!Cp0AudioMixerControl::is_hardware_sink_name("auto_null"));
    // pipewire-pulse reports this special name while no sink exists.
    assert(!Cp0AudioMixerControl::is_hardware_sink_name("@DEFAULT_SINK@"));
    assert(Cp0AudioMixerControl::is_hardware_sink_name(
        "alsa_output.platform-sound.stereo-fallback"));
}

void test_volume_restore_steps()
{
    using clock = std::chrono::steady_clock;
    const auto settle = std::chrono::seconds(2);
    const auto start = clock::time_point{};
    const char *speaker = "alsa_output.platform-sound.stereo-fallback";

    Cp0VolumeRestoreStep state;
    state.target = 20;

    // A placeholder, a missing sink, and a failed lookup never get a write.
    state = cp0_volume_restore_step(state, "auto_null", 20, 80, start, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Wait && !state.holding);
    state = cp0_volume_restore_step(state, "@DEFAULT_SINK@", 20, 80, start, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Wait);
    state = cp0_volume_restore_step(state, "", 20, -1, start, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Wait && state.target == 20);

    // The hardware sink is still at the factory volume, so rewrite it.
    state = cp0_volume_restore_step(state, speaker, 20, 80, start, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Write);
    assert(state.target == 20 && !state.holding);

    // A change made in the UI while the loop is running replaces the target.
    state = cp0_volume_restore_step(state, speaker, 35, 20, start, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Write && state.target == 35);
    // A failed config read must not throw away the volume the UI just set.
    state = cp0_volume_restore_step(state, speaker, -1, 20, start, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Write && state.target == 35);

    // Matching starts the settle window; it is not done on the first read.
    const auto held = start + std::chrono::seconds(5);
    state = cp0_volume_restore_step(state, speaker, 35, 35, held, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Hold && state.holding);
    assert(state.holding_since == held);

    // WirePlumber landing its own restore after the write resets the window.
    const auto clobbered = held + std::chrono::milliseconds(500);
    state = cp0_volume_restore_step(state, speaker, 35, 80, clobbered, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Write && !state.holding);

    state = cp0_volume_restore_step(state, speaker, 35, 35, clobbered, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Hold);
    state = cp0_volume_restore_step(
        state, speaker, 35, 35, clobbered + std::chrono::milliseconds(1999), settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Hold);

    state = cp0_volume_restore_step(
        state, speaker, 35, 35, clobbered + settle, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Done && state.holding);

    // The sink disappearing invalidates a settle that was already in progress.
    state = cp0_volume_restore_step(state, "auto_null", 35, 35, clobbered + settle, settle);
    assert(state.action == Cp0VolumeRestoreStep::Action::Wait && !state.holding);
}

} // namespace

int main()
{
    test_volume_lines();
    test_sink_names();
    test_volume_restore_steps();
}
