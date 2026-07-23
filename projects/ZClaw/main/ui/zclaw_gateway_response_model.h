#pragma once

#include "zclaw_types.h"

#include <string>

namespace zclaw {

OperationResult interpret_pairing_response(UiConfig config, int http_status,
                                           const std::string &body);
OperationResult interpret_webhook_response(const UiConfig &config, int http_status,
                                           const std::string &body);

}  // namespace zclaw
