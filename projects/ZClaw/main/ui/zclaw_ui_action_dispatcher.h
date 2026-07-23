#pragma once

#include "zclaw_key_router.h"

#include <functional>

namespace zclaw {

class ApprovalCoordinator;
class ChatView;
class FontManager;
class InputDialog;
class InputWorkflow;
class SettingsCoordinator;
class SettingsWorkflow;
class ShellView;
class UiConfigManager;

class UiActionDispatcher {
public:
    using RequestQuit = std::function<void()>;

    UiActionDispatcher(UiConfigManager &config, ShellView &shell,
                       FontManager &fonts, InputDialog &input,
                       InputWorkflow &input_workflow,
                       ApprovalCoordinator &approvals,
                       SettingsCoordinator &settings,
                       SettingsWorkflow &settings_workflow, ChatView &chat,
                       RequestQuit request_quit);

    void execute(const KeyAction &action);

private:
    void request_quit();

    UiConfigManager &config_;
    ShellView &shell_;
    FontManager &fonts_;
    InputDialog &input_;
    InputWorkflow &input_workflow_;
    ApprovalCoordinator &approvals_;
    SettingsCoordinator &settings_;
    SettingsWorkflow &settings_workflow_;
    ChatView &chat_;
    RequestQuit request_quit_;
};

}  // namespace zclaw
