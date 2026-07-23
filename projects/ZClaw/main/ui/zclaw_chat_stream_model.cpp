#include "zclaw_chat_stream_model.h"

namespace zclaw {

ChatStreamUpdate ChatStreamState::accept(const ChatEvent &event)
{
    if (finished_)
        return {true, false, {}};

    switch (event.type) {
    case ChatEventType::Chunk:
        chunks_ += event.content;
        break;
    case ChatEventType::ApprovalRequest:
        return {false, true, event.approval};
    case ChatEventType::Done:
        response_ = event.content.empty() ? chunks_ : event.content;
        finished_ = true;
        succeeded_ = true;
        break;
    case ChatEventType::Error:
        response_ = "ZeroClaw error: " + event.content;
        finished_ = true;
        break;
    case ChatEventType::Unknown:
        break;
    }
    return {finished_, false, {}};
}

bool ChatStreamState::finished() const
{
    return finished_;
}

bool ChatStreamState::succeeded() const
{
    return succeeded_;
}

const std::string &ChatStreamState::response() const
{
    return response_;
}

const std::string &ChatStreamState::partial_response() const
{
    return chunks_;
}

}  // namespace zclaw
