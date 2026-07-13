#include "cp0_dispatch_testable.hpp"

#include <cassert>
#include <functional>
#include <vector>

int main()
{
    int calls = 0;
    auto success = [&](auto task) { task(); return true; };
    auto failure = [&](auto) { return false; };
    assert(cp0_testable::dispatch_task(success, [&] { ++calls; }));
    assert(calls == 1);
    assert(!cp0_testable::dispatch_task(failure, [&] { ++calls; }));
    assert(calls == 1);

    std::vector<std::function<void()>> scheduled;
    int attempts = 0;
    auto fake_async = [&](auto task) {
        if (++attempts == 1) return false;
        scheduled.emplace_back(task);
        return true;
    };
    assert(!cp0_testable::dispatch_task(fake_async, [&] { calls += 10; }));
    assert(cp0_testable::dispatch_task(fake_async, [&] { calls += 100; }));
    assert(calls == 1);
    // A failed scheduler must not retain or execute the task.
    scheduled.front()();
    assert(calls == 101);
}
