/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace launcher_media_controls {

int adjust_volume(int delta_percent);
int adjust_brightness(int delta_percent);
bool toggle_mute();

} // namespace launcher_media_controls
