#pragma once

#include <utility>

namespace cp0_testable {

template <typename Scheduler, typename Task>
bool dispatch_task(Scheduler &&scheduler, Task &&task)
{
    return std::forward<Scheduler>(scheduler)(std::forward<Task>(task));
}

} // namespace cp0_testable
