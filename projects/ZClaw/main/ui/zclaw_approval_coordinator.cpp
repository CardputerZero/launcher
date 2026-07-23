#include "zclaw_approval_coordinator.h"

#include "zclaw_fonts.hpp"
#include "zclaw_ui_task_queue.h"

#include <hv/hasync.h>

#include <chrono>
#include <utility>

namespace zclaw {

ApprovalCoordinator::ApprovalCoordinator(FontManager &fonts,
                                         std::shared_ptr<UiTaskQueue> tasks)
    : fonts_(fonts), tasks_(std::move(tasks)),
      lifetime_(std::make_shared<CallbackLifetime<ApprovalCoordinator>>(this))
{
}

ApprovalCoordinator::~ApprovalCoordinator()
{
    lifetime_->invalidate();
    shutdown();
}

bool ApprovalCoordinator::pending() const
{
    return controller_->pending();
}

void ApprovalCoordinator::move_selection(int delta)
{
    controller_->move_selection(delta);
    dialog_.update_selection(controller_->selected_index());
}

bool ApprovalCoordinator::answer(const char *decision)
{
    if (!controller_->answer(decision ? decision : "deny"))
        return false;
    dialog_.close();
    return true;
}

bool ApprovalCoordinator::answer_selected()
{
    const std::string decision = controller_->selected_decision();
    return answer(decision.c_str());
}

void ApprovalCoordinator::cancel()
{
    controller_->cancel();
}

void ApprovalCoordinator::shutdown()
{
    controller_->shutdown();
    dialog_.close();
}

ApprovalCoordinator::Handler ApprovalCoordinator::handler()
{
    const std::shared_ptr<ApprovalController> controller = controller_;
    const std::shared_ptr<UiTaskQueue> tasks = tasks_;
    const std::shared_ptr<CallbackLifetime<ApprovalCoordinator>> lifetime = lifetime_;
    return [controller, tasks, lifetime](const ApprovalRequest &request,
                                         Decision decision) {
        wait_for_decision(controller, tasks, lifetime, request,
                          std::move(decision));
    };
}

void ApprovalCoordinator::wait_for_decision(
    const std::shared_ptr<ApprovalController> &controller,
    const std::shared_ptr<UiTaskQueue> &tasks,
    const std::shared_ptr<CallbackLifetime<ApprovalCoordinator>> &lifetime,
    const ApprovalRequest &request, Decision decision)
{
    const int timeout_seconds = request.timeout_secs <= 0 ? 120 : request.timeout_secs;

    if (!decision)
        return;
    if (!controller->begin(request)) {
        decision("deny");
        return;
    }
    if (!tasks->post([lifetime, request] {
            lifetime->invoke([&request](ApprovalCoordinator &view) {
                view.publish(request);
            });
        })) {
        controller->cancel();
        decision("deny");
        return;
    }
    (void)hv::async([controller, tasks, lifetime, request, timeout_seconds,
                     decision = std::move(decision)]() mutable {
        const ApprovalWaitResult result = controller->wait_for(
            request.request_id, std::chrono::seconds(timeout_seconds));
        if (result.timed_out)
            tasks->post([lifetime] {
                lifetime->invoke([](ApprovalCoordinator &view) {
                    view.close_if_finished();
                });
            });
        decision(result.decision);
    });
}

void ApprovalCoordinator::publish(const ApprovalRequest &request)
{
    if (controller_->pending_request(request.request_id))
        dialog_.show(&fonts_, request, controller_->selected_index());
}

void ApprovalCoordinator::close_if_finished()
{
    if (!controller_->pending())
        dialog_.close();
}

}  // namespace zclaw
