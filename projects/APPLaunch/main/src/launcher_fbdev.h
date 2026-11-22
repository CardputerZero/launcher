/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

void launcher_enable_fbdev_stripes();

// The display is eligible for the stripe-specific lock and carousel timing.
bool launcher_fbdev_stripes_active();
