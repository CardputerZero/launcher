#pragma once

#include <string>

namespace cp0_camera_api {

constexpr int DEFAULT_WIDTH = 320;
constexpr int DEFAULT_HEIGHT = 150;

int parse_integer_argument(const std::string &value, int fallback);

} // namespace cp0_camera_api
