#pragma once

#include "zclaw_chat_event.h"

#include <string>

namespace zclaw {

struct ChatStreamUpdate {
    bool finished = false;
    bool approval_requested = false;
    ApprovalRequest approval;
};

class ChatStreamState {
public:
    ChatStreamUpdate accept(const ChatEvent &event);

    bool finished() const;
    bool succeeded() const;
    const std::string &response() const;
    const std::string &partial_response() const;

private:
    std::string chunks_;
    std::string response_;
    bool finished_ = false;
    bool succeeded_ = false;
};

}  // namespace zclaw
