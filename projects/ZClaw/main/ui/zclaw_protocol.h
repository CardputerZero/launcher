#pragma once

#include "zclaw_types.h"

#include <string>

namespace zclaw::protocol {

struct HttpUrl {
    std::string base;
    std::string path;
};

bool split_http_url(const std::string &url, HttpUrl *out);
std::string gateway_http_base(const UiConfig &config);
std::string gateway_ws_url(const UiConfig &config);
std::string webhook_endpoint(const UiConfig &config);

std::string make_webhook_message(const std::string &message);
std::string make_ws_chat_message(const std::string &message);
std::string make_ws_approval_response(const std::string &request_id,
                                      const std::string &decision);

}  // namespace zclaw::protocol
