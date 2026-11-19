/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "cp0_lvgl_app.h"

#include <string>

namespace cp0::battery {

std::string encode_info(const cp0_battery_info_t &info);
bool decode_info(const std::string &data, cp0_battery_info_t *info);
bool info_is_valid(const cp0_battery_info_t &info);

/* Marks time_to_empty_min, time_to_full_min and cycle_count as unknown (-1). */
void clear_extras(cp0_battery_info_t &info);

/* Raw optional fuel-gauge readings in power_supply sysfs units; -1 = missing. */
struct GaugeExtras {
    long charge_now_uah = -1;
    long charge_full_uah = -1;
    long time_to_empty_s = -1;
    long time_to_full_s = -1;
    long cycle_count = -1;
};

/* Fills remain_mah/full_mah, the times and cycle_count of a valid info.
 * Out-of-range or missing values stay unknown, 0 times count as unknown,
 * time to empty is only kept while discharging and time to full only while
 * charging, and charge_now is clamped to charge_full. */
void apply_gauge_extras(cp0_battery_info_t &info, const GaugeExtras &raw);

cp0_battery_info_t from_power_supply(int present,
                                     int capacity_percent,
                                     long voltage_uv,
                                     long current_ua,
                                     int temperature_c10,
                                     const char *status);

} // namespace cp0::battery
