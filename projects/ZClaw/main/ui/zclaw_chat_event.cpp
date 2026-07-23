#include "zclaw_chat_event.h"

#include "json.hpp"

#include <cstdint>

namespace zclaw {
namespace {

using Json = nlohmann::json;

bool required_string(const Json &object, const char *field, std::string *value,
                     bool allow_empty = true)
{
    const auto found = object.find(field);
    if (found == object.end() || !found->is_string())
        return false;
    *value = found->get<std::string>();
    return allow_empty || !value->empty();
}

bool optional_string(const Json &object, const char *field, std::string *value)
{
    const auto found = object.find(field);
    if (found == object.end())
        return true;
    if (!found->is_string())
        return false;
    *value = found->get<std::string>();
    return true;
}

int approval_timeout(const Json &object)
{
    static constexpr int kDefaultTimeout = 120;
    static constexpr std::uint64_t kMaximumTimeout = 3600;
    const auto found = object.find("timeout_secs");
    if (found == object.end() || !found->is_number_integer())
        return kDefaultTimeout;
    if (found->is_number_unsigned()) {
        const std::uint64_t timeout = found->get<std::uint64_t>();
        return timeout >= 1 && timeout <= kMaximumTimeout
                   ? static_cast<int>(timeout)
                   : kDefaultTimeout;
    }
    const std::int64_t timeout = found->get<std::int64_t>();
    return timeout >= 1 &&
                   static_cast<std::uint64_t>(timeout) <= kMaximumTimeout
               ? static_cast<int>(timeout)
               : kDefaultTimeout;
}

}  // namespace

ChatEvent parse_chat_event(const std::string &json)
{
    ChatEvent event;
    const Json parsed = Json::parse(json, nullptr, false);
    if (!parsed.is_object())
        return event;

    std::string type;
    if (!required_string(parsed, "type", &type, false))
        return event;
    if (type == "chunk") {
        if (!required_string(parsed, "content", &event.content))
            return {};
        event.type = ChatEventType::Chunk;
    } else if (type == "approval_request") {
        if (!required_string(parsed, "request_id", &event.approval.request_id,
                             false) ||
            !optional_string(parsed, "tool", &event.approval.tool) ||
            !optional_string(parsed, "arguments_summary",
                             &event.approval.summary))
            return {};
        event.approval.timeout_secs = approval_timeout(parsed);
        event.type = ChatEventType::ApprovalRequest;
    } else if (type == "done") {
        if (!required_string(parsed, "full_response", &event.content))
            return {};
        event.type = ChatEventType::Done;
    } else if (type == "error") {
        if (!required_string(parsed, "message", &event.content))
            return {};
        event.type = ChatEventType::Error;
    }
    return event;
}

}  // namespace zclaw
