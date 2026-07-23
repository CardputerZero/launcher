#pragma once

#include "cp0_lvgl_app.h"

#include <string>

namespace cp0::battery {

std::string encode_info(const cp0_battery_info_t &info);
bool decode_info(const std::string &data, cp0_battery_info_t *info);
bool info_is_valid(const cp0_battery_info_t &info);

cp0_battery_info_t from_power_supply(int present,
                                     int capacity_percent,
                                     long voltage_uv,
                                     long current_ua,
                                     int temperature_c10,
                                     const char *status);

} // namespace cp0::battery
