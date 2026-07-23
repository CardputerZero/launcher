#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace zclaw {

struct TaskBatchResult {
    std::size_t executed = 0;
    std::size_t failed = 0;
};

TaskBatchResult execute_task_batch(
    std::vector<std::function<void()>> tasks) noexcept;

}  // namespace zclaw
