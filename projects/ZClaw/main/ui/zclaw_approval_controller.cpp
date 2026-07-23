#include "zclaw_approval_controller.h"

namespace zclaw {

bool ApprovalController::begin(const ApprovalRequest &request)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_ || waiting_ || !waiter_request_id_.empty() ||
        request.request_id.empty())
        return false;
    request_id_ = request.request_id;
    decision_.clear();
    waiting_ = true;
    selected_ = 0;
    return true;
}

bool ApprovalController::pending() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return waiting_ && !request_id_.empty();
}

bool ApprovalController::pending_request(const std::string &request_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return waiting_ && !request_id.empty() && request_id_ == request_id;
}

bool ApprovalController::answer(const std::string &decision)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!waiting_ || request_id_.empty())
            return false;
        decision_ = decision.empty() ? "deny" : decision;
        request_id_.clear();
        waiting_ = false;
    }
    condition_.notify_all();
    return true;
}

void ApprovalController::cancel()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        decision_ = "deny";
        request_id_.clear();
        waiting_ = false;
        selected_ = 0;
    }
    condition_.notify_all();
}

void ApprovalController::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        decision_ = "deny";
        request_id_.clear();
        waiting_ = false;
        selected_ = 0;
    }
    condition_.notify_all();
}

int ApprovalController::selected_index() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return selected_;
}

void ApprovalController::move_selection(int delta)
{
    std::lock_guard<std::mutex> lock(mutex_);
    selected_ = (selected_ + delta) % 3;
    if (selected_ < 0)
        selected_ += 3;
}

std::string ApprovalController::selected_decision() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (selected_ == 1)
        return "always";
    if (selected_ == 2)
        return "deny";
    return "approve";
}

ApprovalWaitResult ApprovalController::wait_for(
    const std::string &request_id, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mutex_);
    ApprovalWaitResult result;
    if (!waiting_ || request_id.empty() || request_id_ != request_id ||
        !waiter_request_id_.empty())
        return result;

    waiter_request_id_ = request_id;
    const bool answered = condition_.wait_for(lock, timeout, [&] {
        return !waiting_ || request_id_ != request_id;
    });
    result.timed_out = !answered;
    result.decision = answered && !decision_.empty() ? decision_ : "deny";
    if (request_id_ == request_id) {
        request_id_.clear();
        decision_.clear();
        waiting_ = false;
        selected_ = 0;
    }
    waiter_request_id_.clear();
    return result;
}

}  // namespace zclaw
