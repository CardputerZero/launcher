/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_audio_mixer_control.hpp"
#include "cp0_pulse_client.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>

namespace {

void test_volume_scale_conversions()
{
    // PulseAudio rounds the percentage like pa_cvolume_snprint(); a value
    // written by the launcher must read back unchanged.
    assert(cp0::pulse::percent_from_volume_scale(0) == 0);
    assert(cp0::pulse::percent_from_volume_scale(1) == 0);
    assert(cp0::pulse::percent_from_volume_scale(32767) == 50);
    assert(cp0::pulse::percent_from_volume_scale(32768) == 50);
    assert(cp0::pulse::percent_from_volume_scale(26214) == 40);
    assert(cp0::pulse::percent_from_volume_scale(65535) == 100);
    assert(cp0::pulse::percent_from_volume_scale(65536) == 100);
    assert(cp0::pulse::percent_from_volume_scale(131072) == 200);

    for (int percent = 0; percent <= 100; ++percent) {
        const std::uint32_t scale = cp0::pulse::volume_scale_from_percent(percent);
        assert(cp0::pulse::percent_from_volume_scale(scale) == percent);
    }
    assert(cp0::pulse::volume_scale_from_percent(-20) == 0);
    assert(cp0::pulse::volume_scale_from_percent(140) == cp0::pulse::volume_scale_from_percent(100));
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
    test_volume_scale_conversions();
    test_sink_names();
    test_volume_restore_steps();
}
