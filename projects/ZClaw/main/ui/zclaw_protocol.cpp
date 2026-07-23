#include "zclaw_protocol.h"

#include "json.hpp"

namespace zclaw::protocol {
namespace {

using Json = nlohmann::json;

std::string url_encode(const std::string &value)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char ch : value) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out += static_cast<char>(ch);
        } else {
            out += '%';
            out += hex[ch >> 4];
            out += hex[ch & 0x0F];
        }
    }
    return out;
}

}  // namespace

bool split_http_url(const std::string &url, HttpUrl *out)
{
    if (!out)
        return false;
    const std::size_t scheme = url.find("://");
    if (scheme == std::string::npos)
        return false;
    const std::size_t authority = scheme + 3;
    const std::size_t path = url.find('/', authority);
    out->base = path == std::string::npos ? url : url.substr(0, path);
    out->path = path == std::string::npos ? "/" : url.substr(path);
    return out->base.rfind("http://", 0) == 0 || out->base.rfind("https://", 0) == 0;
}

std::string gateway_http_base(const UiConfig &config)
{
    std::string url = config.webhook_url.empty() ? "http://127.0.0.1:42617/webhook" :
                                                   config.webhook_url;
    const std::size_t scheme = url.find("://");
    const std::size_t path = scheme == std::string::npos ? url.find('/') :
                                                           url.find('/', scheme + 3);
    if (path != std::string::npos)
        url.resize(path);
    return url;
}

std::string gateway_ws_url(const UiConfig &config)
{
    std::string base = gateway_http_base(config);
    if (base.rfind("https://", 0) == 0)
        base.replace(0, 5, "wss");
    else if (base.rfind("http://", 0) == 0)
        base.replace(0, 4, "ws");
    return base + "/ws/chat?agent=" + url_encode(config.agent_alias) +
           "&token=" + url_encode(config.bearer_token) +
           "&session_id=zclaw-ui&name=ZClaw";
}

std::string webhook_endpoint(const UiConfig &config)
{
    std::string url = config.webhook_url.empty() ? "http://127.0.0.1:42617/webhook" :
                                                   config.webhook_url;
    if (url.find('?') == std::string::npos)
        url += "?agent=" + url_encode(config.agent_alias);
    return url;
}

std::string make_webhook_message(const std::string &message)
{
    return Json{{"message", message}}.dump();
}

std::string make_ws_chat_message(const std::string &message)
{
    return Json{{"type", "message"}, {"content", message}}.dump();
}

std::string make_ws_approval_response(const std::string &request_id,
                                      const std::string &decision)
{
    return Json{{"type", "approval_response"},
                {"request_id", request_id},
                {"decision", decision.empty() ? "deny" : decision}}
        .dump();
}

}  // namespace zclaw::protocol
