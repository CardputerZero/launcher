#include "zclaw_task_inbox.h"

#include <utility>

namespace zclaw {

void TaskInbox::activate()
{
    std::lock_guard<std::mutex> lock(mutex_);
    active_ = true;
}

bool TaskInbox::post(Task task)
{
    if (!task)
        return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_)
        return false;
    pending_.push_back(std::move(task));
    return true;
}

std::vector<TaskInbox::Task> TaskInbox::take_pending()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Task> tasks;
    if (active_)
        tasks.swap(pending_);
    return tasks;
}

void TaskInbox::shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    active_ = false;
    pending_.clear();
}

bool TaskInbox::active() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}

}  // namespace zclaw
