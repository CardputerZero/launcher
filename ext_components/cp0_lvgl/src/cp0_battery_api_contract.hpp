#pragma once

#include "cp0_lvgl_app.h"

#include <functional>
#include <list>
#include <string>

namespace cp0::battery {

enum class ApiCommand {
    read,
    calibrate,
};

struct ApiRequest {
    ApiCommand command = ApiCommand::read;
    int calibration_index = -1;
};

struct ApiParseResult {
    bool ok = false;
    ApiRequest request;
    std::string error;
};

struct ApiReply {
    int code = -1;
    std::string data;
};

using ReadOperation = std::function<cp0_battery_info_t()>;
using CalibrateOperation = std::function<int(int)>;

ApiParseResult parse_api_request(const std::list<std::string> &arguments);
ApiReply dispatch_api_request(const std::list<std::string> &arguments,
                              const ReadOperation &read,
                              const CalibrateOperation &calibrate);

} // namespace cp0::battery
