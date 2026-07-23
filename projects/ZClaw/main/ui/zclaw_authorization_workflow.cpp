#include "zclaw_authorization_workflow.h"

#include "zclaw_async_service.h"
#include "zclaw_chat_view.h"
#include "zclaw_fonts.hpp"
#include "zclaw_input_dialog.h"
#include "zclaw_settings_coordinator.h"
#include "zclaw_ui_config_manager.h"

#include <utility>

namespace zclaw {

AuthorizationWorkflow::AuthorizationWorkflow(
    UiConfigManager &config, SettingsCoordinator &settings, InputDialog &input,
    FontManager &fonts, ChatView &chat, AsyncService &async_service)
    : config_(config), settings_(settings), input_(input), fonts_(fonts),
      chat_(chat), async_service_(async_service),
      lifetime_(std::make_shared<CallbackLifetime<AuthorizationWorkflow>>(this))
{
}

AuthorizationWorkflow::~AuthorizationWorkflow()
{
    lifetime_->invalidate();
}

void AuthorizationWorkflow::open_pairing_input()
{
    input_.open_text(&fonts_, "Pairing code", "", InputMode::PairingCode);
}

void AuthorizationWorkflow::start_pairing(const std::string &code)
{
    const AuthorizationSession::Generation generation = session_.begin();
    chat_.append_assistant_message("Pairing with ZeroClaw...");
    const std::shared_ptr<CallbackLifetime<AuthorizationWorkflow>> lifetime = lifetime_;
    if (!async_service_.pair(
            config_.config(), code,
            [lifetime, generation](OperationResult result) {
                lifetime->invoke([generation, &result](AuthorizationWorkflow &workflow) {
                    workflow.finish_pairing(generation, std::move(result));
                });
            })) {
        session_.finish(generation);
        chat_.append_assistant_message("Could not start pairing request.");
    }
}

void AuthorizationWorkflow::clear_token()
{
    session_.invalidate();
    std::string error;
    if (config_.clear_token(&error))
        chat_.append_assistant_message("Authorization token cleared.");
    else
        report_save_error(error);
    settings_.render_authorization();
}

void AuthorizationWorkflow::finish_pairing(
    AuthorizationSession::Generation generation, OperationResult result)
{
    if (!session_.finish(generation))
        return;
    chat_.append_assistant_message(result.text);
    if (result.ok) {
        std::string error;
        if (!config_.update(result.config, &error))
            report_save_error(error);
    }
    if (settings_.is_open() &&
        settings_.state().view() == SettingsView::Authorization)
        settings_.render_authorization();
}

void AuthorizationWorkflow::report_save_error(const std::string &error)
{
    chat_.append_assistant_message(
        error.empty() ? "Could not save settings." : error);
}

}  // namespace zclaw
