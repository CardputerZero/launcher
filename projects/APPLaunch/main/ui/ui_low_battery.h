/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "cp0_lvgl_app.h"

namespace launcher_battery_ui {

void init_warning();
void update_warning(const cp0_battery_info_t &info);
void shutdown_warning();

} // namespace launcher_battery_ui
