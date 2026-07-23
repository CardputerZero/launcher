#include "zclaw_ui_task_queue.h"

#include "zclaw_task_batch.h"

#include <utility>

namespace zclaw {

UiTaskQueue::~UiTaskQueue()
{
    shutdown();
}

bool UiTaskQueue::start(uint32_t poll_period_ms)
{
    if (inbox_.active())
        return true;
    timer_ = lv_timer_create(timer_callback, poll_period_ms == 0 ? 1 : poll_period_ms,
                             this);
    if (!timer_)
        return false;
    inbox_.activate();
    return true;
}

bool UiTaskQueue::post(Task task)
{
    return inbox_.post(std::move(task));
}

void UiTaskQueue::shutdown()
{
    inbox_.shutdown();
    lv_timer_t *timer = timer_;
    timer_ = nullptr;
    if (timer)
        lv_timer_delete(timer);
}

bool UiTaskQueue::active() const
{
    return inbox_.active();
}

void UiTaskQueue::timer_callback(lv_timer_t *timer)
{
    UiTaskQueue *queue = static_cast<UiTaskQueue *>(lv_timer_get_user_data(timer));
    if (queue)
        queue->drain();
}

void UiTaskQueue::drain()
{
    execute_task_batch(inbox_.take_pending());
}

}  // namespace zclaw
