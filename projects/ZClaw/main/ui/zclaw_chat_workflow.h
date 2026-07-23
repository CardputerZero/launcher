#pragma once

#include "zclaw_callback_lifetime.h"

#include <functional>
#include <memory>
#include <string>

namespace zclaw {

class ApprovalCoordinator;
class AsyncService;
class ChatView;
class UiConfigManager;

class ChatWorkflow {
public:
    using OpenSetup = std::function<void()>;

    ChatWorkflow(UiConfigManager &config, ChatView &chat,
                 AsyncService &async_service, ApprovalCoordinator &approvals,
                 OpenSetup open_setup);
    ~ChatWorkflow();

    void submit(const std::string &message);

private:
    void request_reply(const std::string &message);
    void finish_request(std::string text);

    UiConfigManager &config_;
    ChatView &chat_;
    AsyncService &async_service_;
    ApprovalCoordinator &approvals_;
    OpenSetup open_setup_;
    bool request_in_flight_ = false;
    std::shared_ptr<CallbackLifetime<ChatWorkflow>> lifetime_;
};

}  // namespace zclaw
