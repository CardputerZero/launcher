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

int adjust_volume(int delta_percent);
int adjust_brightness(int delta_percent);
bool toggle_mute();

/* Drive the panel backlight to zero without persisting anything, for the
 * screen-off gesture.  Returns the raw value to hand back to
 * restore_backlight(), or -1 when the backlight could not be suspended.  The
 * persisted brightness must survive untouched: the normal step-based control
 * can never express zero, so reusing it here would overwrite the user's
 * setting and leave the panel dark after wake-up. */
int suspend_backlight();
void restore_backlight(int raw);

} // namespace launcher_media_controls
