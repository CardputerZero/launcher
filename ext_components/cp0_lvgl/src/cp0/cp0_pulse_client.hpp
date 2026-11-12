/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

namespace cp0::pulse {

// The launcher talks to the PulseAudio-compatible server (pipewire-pulse) in
// process through libpulse.so.0. The library is loaded with dlopen so the
// build does not need the PulseAudio development package; when the library or
// the server is unavailable every call fails and the caller falls back to the
// persisted configuration. No call spawns a process.

// Loads libpulse.so.0 and resolves the client symbols. Call this during
// startup (for example while the splash animation is on screen) so the first
// volume action does not pay the loader cost. Returns false when libpulse is
// missing. The call is idempotent and thread-safe; later calls only return the
// cached result.
bool preload();

bool default_sink_name(std::string &name);
int read_default_sink_volume();
int write_default_sink_volume(int percent);
int read_default_sink_mute();
int toggle_default_sink_mute();

// PulseAudio stores volumes on a linear scale where PA_VOLUME_NORM (0x10000)
// is 100%. The percentage is rounded like pa_cvolume_snprint(), so a value
// written by volume_scale_from_percent() reads back unchanged.
int percent_from_volume_scale(std::uint32_t volume);
std::uint32_t volume_scale_from_percent(int percent);

} // namespace cp0::pulse
