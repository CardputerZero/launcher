#pragma once

#include "lvgl/lvgl.h"
#include "zclaw_callback_lifetime.h"
#include "zclaw_types.h"

#include <memory>
#include <string>

namespace zclaw {

class AsyncService;
class ChatView;
class FontManager;
class InputDialog;
class ProviderManager;
class SettingsCoordinator;
class UiConfigManager;

class SetupWorkflow {
public:
    SetupWorkflow(ProviderManager &providers, UiConfigManager &config,
                  SettingsCoordinator &settings, InputDialog &input,
                  FontManager &fonts, ChatView &chat,
                  AsyncService &async_service);
    ~SetupWorkflow();

    bool in_flight() const;
    bool retry_pending() const;
    void open(lv_obj_t *parent, bool first_run_needed);
    void render();
    void update_progress(const SetupProgress &progress);
    void select_provider(int preset_index);
    void edit_selected_field();
    void apply_edit(const std::string &value);
    void start();
    void dismiss_error();
    void move_retry_selection(int delta);
    bool activate_retry_selection();

private:
    void finish(OperationResult result);
    void report_save_error(const std::string &error);

    ProviderManager &providers_;
    UiConfigManager &config_;
    SettingsCoordinator &settings_;
    InputDialog &input_;
    FontManager &fonts_;
    ChatView &chat_;
    AsyncService &async_service_;
    bool in_flight_ = false;
    bool retry_pending_ = false;
    int retry_selection_ = 0;
    std::shared_ptr<CallbackLifetime<SetupWorkflow>> lifetime_;
};

}  // namespace zclaw
