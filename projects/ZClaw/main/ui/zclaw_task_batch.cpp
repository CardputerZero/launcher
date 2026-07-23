#include "zclaw_task_batch.h"

namespace zclaw {

TaskBatchResult execute_task_batch(
    std::vector<std::function<void()>> tasks) noexcept
{
    TaskBatchResult result;
    for (std::function<void()> &task : tasks) {
        ++result.executed;
        try {
            task();
        } catch (...) {
            ++result.failed;
        }
    }
    return result;
}

}  // namespace zclaw
