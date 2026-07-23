#include "zclaw_task_batch.h"

#include <cassert>
#include <stdexcept>
#include <utility>
#include <vector>

int main()
{
    std::vector<int> execution_order;
    std::vector<std::function<void()>> tasks;
    tasks.push_back([&execution_order] { execution_order.push_back(1); });
    tasks.push_back([&execution_order] {
        execution_order.push_back(2);
        throw std::runtime_error("callback failed");
    });
    tasks.push_back([&execution_order] {
        execution_order.push_back(3);
        throw 42;
    });
    tasks.push_back([&execution_order] { execution_order.push_back(4); });
    tasks.emplace_back();
    tasks.push_back([&execution_order] { execution_order.push_back(5); });

    const zclaw::TaskBatchResult result =
        zclaw::execute_task_batch(std::move(tasks));
    assert(result.executed == 6);
    assert(result.failed == 3);
    assert((execution_order == std::vector<int>{1, 2, 3, 4, 5}));

    const zclaw::TaskBatchResult empty = zclaw::execute_task_batch({});
    assert(empty.executed == 0);
    assert(empty.failed == 0);
    return 0;
}
