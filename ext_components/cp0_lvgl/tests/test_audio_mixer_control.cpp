/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_audio_mixer_control.hpp"

#include <cassert>

int main()
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

    assert(!Cp0AudioMixerControl::is_hardware_sink_name(nullptr));
    assert(!Cp0AudioMixerControl::is_hardware_sink_name(""));
    // PipeWire's placeholder sink; volume written here never reaches the speaker.
    assert(!Cp0AudioMixerControl::is_hardware_sink_name("auto_null"));
    assert(Cp0AudioMixerControl::is_hardware_sink_name(
        "alsa_output.platform-sound.stereo-fallback"));
}
