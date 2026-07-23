#pragma once

#include <utility>

namespace cp0_testable {

template <typename Scheduler, typename Task>
bool dispatch_task(Scheduler &&scheduler, Task &&task)
{
    return std::forward<Scheduler>(scheduler)(std::forward<Task>(task));
}

template <typename Fallback>
bool run_on_dispatch_failure(bool dispatched, Fallback &&fallback) noexcept
{
    if (dispatched) return false;
    try {
        std::forward<Fallback>(fallback)();
    } catch (...) {
    }
    return true;
}

} // namespace cp0_testable
