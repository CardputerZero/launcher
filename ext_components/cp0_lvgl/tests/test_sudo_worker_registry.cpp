#include "cp0_sudo_worker_registry.hpp"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <thread>

int main()
{
    using cp0_sudo::JoinResult;
    using cp0_sudo::WorkerRegistry;

    WorkerRegistry registry;
    std::mutex mutex;
    std::condition_variable wake;
    std::atomic<int> cancellations{0};
    std::atomic<int> completions{0};
    bool cancelled = false;

    for (int i = 0; i < 2; ++i) {
        assert(registry.start(
            [&] {
                ++cancellations;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    cancelled = true;
                }
                wake.notify_all();
            },
            [&] {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [&] { return cancelled; });
                ++completions;
            }));
    }
    assert(registry.active() > 0);
    registry.request_shutdown();
    registry.request_shutdown();
    assert(!registry.accepting());
    assert(cancellations.load() == 2);
    assert(!registry.start([] {}, [] {}));
    assert(registry.join() == JoinResult::Joined);
    assert(registry.active() == 0);
    assert(completions.load() == 2);

    WorkerRegistry self_join_registry;
    std::mutex self_mutex;
    std::condition_variable self_wake;
    bool allow_exit = false;
    std::atomic<JoinResult> self_result{JoinResult::Joined};
    std::atomic<bool> recognized_worker{false};
    assert(self_join_registry.start(
        [&] {
            {
                std::lock_guard<std::mutex> lock(self_mutex);
                allow_exit = true;
            }
            self_wake.notify_all();
        },
        [&] {
            recognized_worker.store(self_join_registry.is_current_worker());
            self_result.store(self_join_registry.join());
            std::unique_lock<std::mutex> lock(self_mutex);
            self_wake.wait(lock, [&] { return allow_exit; });
        }));
    while (self_result.load() == JoinResult::Joined) std::this_thread::yield();
    assert(self_result.load() == JoinResult::SelfJoin);
    assert(recognized_worker.load());
    self_join_registry.request_shutdown();
    assert(self_join_registry.join() == JoinResult::Joined);
}
