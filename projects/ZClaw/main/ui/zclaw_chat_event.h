#pragma once

#include "zclaw_types.h"

#include <string>

namespace zclaw {

enum class ChatEventType {
    Unknown,
    Chunk,
    ApprovalRequest,
    Done,
    Error,
};

struct ChatEvent {
    ChatEventType type = ChatEventType::Unknown;
    std::string content;
    ApprovalRequest approval;
};

ChatEvent parse_chat_event(const std::string &json);

}  // namespace zclaw
