#pragma once

#include "cp0_lvgl_app.h"

#include <string>

namespace cp0::imu {

std::string encode_info(const cp0_compass_info_t &info);
bool decode_info(const std::string &payload, cp0_compass_info_t &info);

} // namespace cp0::imu
