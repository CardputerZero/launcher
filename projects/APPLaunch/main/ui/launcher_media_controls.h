/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "model/setup_value_policy.hpp"

namespace launcher_media_controls {

constexpr int VOLUME_STEP_PERCENT =
    setup_values::volume_metric(setup_values::VolumeMetric::StepPercent);

/* Volume cached for the lifetime of the launcher process. The launcher reads
 * the system mixer once at startup and keeps this value in sync with every
 * volume write it performs, so the shortcut OSD and Settings -> Speaker render
 * the same number instead of querying the mixer at different times.
 * cached_volume() returns -1 until the startup read produced a value. */
bool has_cached_volume();
int cached_volume();
/* Read the mixer once, off the LVGL thread, and publish the settled value.
 * Safe to call once at launcher startup; a later call restarts the read. */
void start_volume_cache();
/* Join the startup reader so teardown cannot race a signal call. */
void stop_volume_cache();
/* Record a value that was just written to the mixer, or read back from it. */
void update_cached_volume(int percent);

int adjust_volume(int delta_percent);
int adjust_brightness(int delta_percent);
bool toggle_mute();

// Apply saved brightness at startup, defaulting to maximum for missing/invalid
// values. Does not persist changes or reuse a screen-off hardware reading.
bool restore_startup_backlight();

/* Drive the panel backlight to zero without persisting anything, for the
 * screen-off gesture.  Returns the raw value to hand back to
 * restore_backlight(), or -1 when the backlight could not be suspended.  The
 * persisted brightness must survive untouched: the normal step-based control
 * can never express zero, so reusing it here would overwrite the user's
 * setting and leave the panel dark after wake-up. */
int suspend_backlight();
void restore_backlight(int raw);

} // namespace launcher_media_controls
