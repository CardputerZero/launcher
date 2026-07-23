#pragma once

#include "zclaw_authorization_session_model.h"
#include "zclaw_callback_lifetime.h"
#include "zclaw_types.h"

#include <memory>
#include <string>

namespace zclaw {

class AsyncService;
class ChatView;
class FontManager;
class InputDialog;
class SettingsCoordinator;
class UiConfigManager;

class AuthorizationWorkflow {
public:
    AuthorizationWorkflow(UiConfigManager &config,
                          SettingsCoordinator &settings, InputDialog &input,
                          FontManager &fonts, ChatView &chat,
                          AsyncService &async_service);
    ~AuthorizationWorkflow();

    void open_pairing_input();
    void start_pairing(const std::string &code);
    void clear_token();

private:
    void finish_pairing(AuthorizationSession::Generation generation,
                        OperationResult result);
    void report_save_error(const std::string &error);

    UiConfigManager &config_;
    SettingsCoordinator &settings_;
    InputDialog &input_;
    FontManager &fonts_;
    ChatView &chat_;
    AsyncService &async_service_;
    AuthorizationSession session_;
    std::shared_ptr<CallbackLifetime<AuthorizationWorkflow>> lifetime_;
};

}  // namespace zclaw
