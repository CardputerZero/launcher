#pragma once

#include "zclaw_approval_controller.h"
#include "zclaw_approval_dialog.h"
#include "zclaw_callback_lifetime.h"

#include <functional>
#include <memory>
#include <string>

namespace zclaw {

class FontManager;
class UiTaskQueue;

class ApprovalCoordinator {
public:
    using Decision = std::function<void(std::string)>;
    using Handler = std::function<void(const ApprovalRequest &, Decision)>;

    ApprovalCoordinator(FontManager &fonts, std::shared_ptr<UiTaskQueue> tasks);
    ~ApprovalCoordinator();

    ApprovalCoordinator(const ApprovalCoordinator &) = delete;
    ApprovalCoordinator &operator=(const ApprovalCoordinator &) = delete;

    bool pending() const;
    void move_selection(int delta);
    bool answer(const char *decision);
    bool answer_selected();
    void cancel();
    void shutdown();

    Handler handler();

private:
    static void wait_for_decision(
        const std::shared_ptr<ApprovalController> &controller,
        const std::shared_ptr<UiTaskQueue> &tasks,
        const std::shared_ptr<CallbackLifetime<ApprovalCoordinator>> &lifetime,
        const ApprovalRequest &request, Decision decision);
    void publish(const ApprovalRequest &request);
    void close_if_finished();

    FontManager &fonts_;
    std::shared_ptr<UiTaskQueue> tasks_;
    std::shared_ptr<ApprovalController> controller_ =
        std::make_shared<ApprovalController>();
    std::shared_ptr<CallbackLifetime<ApprovalCoordinator>> lifetime_;
    ApprovalDialog dialog_;
};

}  // namespace zclaw
