#include "zclaw_startup_workflow.h"

#include "zclaw_async_service.h"
#include "zclaw_runtime_state.h"
#include "zclaw_ui_config_manager.h"
#include "zclaw_ui_task_queue.h"

#include <utility>

namespace zclaw {

StartupWorkflow::StartupWorkflow(UiConfigManager &config, StartupView &view,
                                 AsyncService &async_service,
                                 std::shared_ptr<UiTaskQueue> tasks,
                                 OpenSetup open_setup)
    : config_(config), view_(view), async_service_(async_service),
      tasks_(std::move(tasks)), open_setup_(std::move(open_setup)),
      lifetime_(std::make_shared<CallbackLifetime<StartupWorkflow>>(this))
{
}

StartupWorkflow::~StartupWorkflow()
{
    lifetime_->invalidate();
}

void StartupWorkflow::start(lv_obj_t *parent, const FontManager *fonts)
{
    view_.show_network_check(parent, fonts);
    if (!tasks_ || !tasks_->start()) {
        view_.show_offline("Could not start the UI task queue.");
        return;
    }
    const std::shared_ptr<CallbackLifetime<StartupWorkflow>> lifetime = lifetime_;
    if (!async_service_.check_network([lifetime](NetworkCheckResult result) {
            lifetime->invoke([&result](StartupWorkflow &workflow) {
                workflow.finish_check(result.online, result.error);
            });
        })) {
        finish_check(false, "Could not start network check.");
    }
}

StartupState StartupWorkflow::state() const
{
    return view_.state();
}

void StartupWorkflow::finish_check(bool online, const std::string &error)
{
    if (!online) {
        view_.show_offline(error);
        return;
    }

    view_.show_ready();
    if (RuntimeState::first_run_needed(config_.config()) && open_setup_)
        open_setup_();
}

}  // namespace zclaw
