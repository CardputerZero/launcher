#include "zclaw_chat_workflow.h"

#include "zclaw_approval_coordinator.h"
#include "zclaw_async_service.h"
#include "zclaw_chat_view.h"
#include "zclaw_runtime_state.h"
#include "zclaw_ui_config_manager.h"

#include <utility>

namespace zclaw {

ChatWorkflow::ChatWorkflow(UiConfigManager &config, ChatView &chat,
                           AsyncService &async_service,
                           ApprovalCoordinator &approvals, OpenSetup open_setup)
    : config_(config), chat_(chat), async_service_(async_service),
      approvals_(approvals), open_setup_(std::move(open_setup)),
      lifetime_(std::make_shared<CallbackLifetime<ChatWorkflow>>(this))
{
}

ChatWorkflow::~ChatWorkflow()
{
    lifetime_->invalidate();
}

void ChatWorkflow::submit(const std::string &message)
{
    if (message.empty())
        return;
    chat_.append_user_message(message);
    request_reply(message);
}

void ChatWorkflow::request_reply(const std::string &message)
{
    if (request_in_flight_) {
        chat_.append_assistant_message("I am still waiting for ZeroClaw.");
        return;
    }
    if (RuntimeState::first_run_needed(config_.config())) {
        chat_.append_assistant_message("Run quickstart before chatting.");
        if (open_setup_)
            open_setup_();
        return;
    }

    request_in_flight_ = true;
    chat_.append_assistant_message("Thinking...");
    const std::shared_ptr<CallbackLifetime<ChatWorkflow>> lifetime = lifetime_;
    if (!async_service_.send_chat(
            config_.config(), message, approvals_.handler(),
            [lifetime](OperationResult result) {
                lifetime->invoke([&result](ChatWorkflow &workflow) {
                    workflow.finish_request(std::move(result.text));
                });
            })) {
        request_in_flight_ = false;
        chat_.append_assistant_message("Could not start webhook request.");
    }
}

void ChatWorkflow::finish_request(std::string text)
{
    request_in_flight_ = false;
    chat_.append_assistant_message(text);
}

}  // namespace zclaw
