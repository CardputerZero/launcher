#pragma once

#include "zclaw_callback_lifetime.h"
#include "zclaw_startup_view.h"

#include <functional>
#include <memory>
#include <string>

namespace zclaw {

class AsyncService;
class FontManager;
class UiConfigManager;
class UiTaskQueue;

class StartupWorkflow {
public:
    using OpenSetup = std::function<void()>;

    StartupWorkflow(UiConfigManager &config, StartupView &view,
                    AsyncService &async_service,
                    std::shared_ptr<UiTaskQueue> tasks, OpenSetup open_setup);
    ~StartupWorkflow();

    void start(lv_obj_t *parent, const FontManager *fonts);
    StartupState state() const;

private:
    void finish_check(bool online, const std::string &error);

    UiConfigManager &config_;
    StartupView &view_;
    AsyncService &async_service_;
    std::shared_ptr<UiTaskQueue> tasks_;
    OpenSetup open_setup_;
    std::shared_ptr<CallbackLifetime<StartupWorkflow>> lifetime_;
};

}  // namespace zclaw
