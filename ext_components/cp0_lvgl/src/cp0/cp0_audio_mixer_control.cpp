/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_audio_mixer_control.hpp"

#include "cp0_pulse_client.hpp"

std::string Cp0AudioMixerControl::default_sink_name() const
{
    std::string name;
    return cp0::pulse::default_sink_name(name) ? name : std::string();
}

int Cp0AudioMixerControl::read_volume() const
{
    return cp0::pulse::read_default_sink_volume();
}

int Cp0AudioMixerControl::write_volume(int percent) const
{
    return cp0::pulse::write_default_sink_volume(percent);
}

int Cp0AudioMixerControl::read_mute() const
{
    return cp0::pulse::read_default_sink_mute();
}

int Cp0AudioMixerControl::toggle_mute() const
{
    return cp0::pulse::toggle_default_sink_mute();
}

bool Cp0AudioMixerControl::preload() const
{
    return cp0::pulse::preload();
}
