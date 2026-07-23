#include "cp0_imu_worker_contract.hpp"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <thread>

int main()
{
    std::mutex lifecycle_mutex;
    bool report_reentered = false;
    const auto outcome = cp0::imu::prepare_then_report(
        [&] {
            std::lock_guard<std::mutex> lock(lifecycle_mutex);
            return cp0::imu::StartOutcome::AlreadyRunning;
        },
        [&](cp0::imu::StartOutcome reported) {
            assert(reported == cp0::imu::StartOutcome::AlreadyRunning);
            assert(lifecycle_mutex.try_lock());
            report_reentered = true;
            lifecycle_mutex.unlock();
        });
    assert(outcome == cp0::imu::StartOutcome::AlreadyRunning);
    assert(report_reentered);

    bool failure_reported = false;
    assert(cp0::imu::prepare_then_report(
        []() -> cp0::imu::StartOutcome { throw 1; },
        [&](cp0::imu::StartOutcome reported) {
            failure_reported =
                reported == cp0::imu::StartOutcome::ThreadCreationFailed;
            throw 2;
        }) == cp0::imu::StartOutcome::ThreadCreationFailed);
    assert(failure_reported);

    std::atomic<bool> completed{false};
    std::thread natural([&] { completed.store(true); });
    while (!completed.load()) std::this_thread::yield();
    assert(cp0::imu::reap_completed_worker(natural, false));
    assert(!natural.joinable());

    std::atomic<bool> cancelled{false};
    std::mutex mutex;
    std::condition_variable wake;
    std::thread blocked([&] {
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait(lock, [&] { return cancelled.load(); });
    });
    assert(cp0::imu::cancel_and_join_worker(
        blocked,
        [&] { cancelled.store(true); },
        [&] { wake.notify_all(); }));
    assert(!blocked.joinable());

    std::thread none;
    assert(!cp0::imu::cancel_and_join_worker(none, [] {}, [] {}));
}
