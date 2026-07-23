#include "cp0_battery_lifecycle.hpp"
#include "cp0_battery_publish_gate.hpp"

#include <atomic>
#include <cassert>
#include <mutex>
#include <thread>
#include <stdexcept>
#include <vector>

int main()
{
    int handlers_created = 0;
    int timers_created = 0;
    std::vector<int> release_order;
    cp0::battery::Lifecycle lifecycle;
    const auto handler_factory = [&] {
        ++handlers_created;
        return cp0::battery::LifecycleResource{
            true, [&] { release_order.push_back(2); }};
    };
    const auto timer_factory = [&] {
        ++timers_created;
        return cp0::battery::LifecycleResource{
            true, [&] { release_order.push_back(1); }};
    };

    assert(lifecycle.start(handler_factory, timer_factory));
    assert(lifecycle.start(handler_factory, timer_factory));
    assert(lifecycle.active());
    assert(handlers_created == 1 && timers_created == 1);
    lifecycle.stop();
    lifecycle.stop();
    assert(!lifecycle.active());
    assert((release_order == std::vector<int>{1, 2}));

    int rollback_releases = 0;
    cp0::battery::Lifecycle rollback;
    assert(!rollback.start(
        [&] {
            return cp0::battery::LifecycleResource{
                true, [&] { ++rollback_releases; }};
        },
        [] { return cp0::battery::LifecycleResource{false, {}}; }));
    assert(rollback_releases == 1 && !rollback.active());

    cp0::battery::Lifecycle throwing;
    int exception_rollbacks = 0;
    assert(!throwing.start(
        [&] {
            return cp0::battery::LifecycleResource{
                true, [&] { ++exception_rollbacks; }};
        },
        []() -> cp0::battery::LifecycleResource {
            throw std::runtime_error("timer factory");
        }));
    assert(exception_rollbacks == 1 && !throwing.active());

    cp0::battery::PublishGate gate;
    int publishes = 0;
    assert(gate.run([&] {
        ++publishes;
        assert(!gate.run([&] { ++publishes; }));
    }));
    assert(publishes == 1);
    assert(gate.run([] { throw std::runtime_error("event callback"); }));
    assert(gate.run([&] { ++publishes; }));
    assert(publishes == 2);

    std::atomic<int> concurrent_handlers{0};
    std::atomic<int> concurrent_timers{0};
    cp0::battery::Lifecycle concurrent;
    std::vector<std::thread> threads;
    for (int index = 0; index < 12; ++index) {
        threads.emplace_back([&] {
            assert(concurrent.start(
                [&] {
                    ++concurrent_handlers;
                    return cp0::battery::LifecycleResource{true, [] {}};
                },
                [&] {
                    ++concurrent_timers;
                    return cp0::battery::LifecycleResource{true, [] {}};
                }));
        });
    }
    for (auto &thread : threads)
        thread.join();
    assert(concurrent_handlers.load() == 1);
    assert(concurrent_timers.load() == 1);
}
