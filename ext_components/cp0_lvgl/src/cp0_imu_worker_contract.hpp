#pragma once

#include <thread>
#include <utility>

namespace cp0::imu {

enum class StartOutcome {
    Started,
    AlreadyRunning,
    Stopping,
    ThreadCreationFailed,
};

template <typename Prepare, typename Report>
StartOutcome prepare_then_report(Prepare &&prepare, Report &&report) noexcept
{
    StartOutcome outcome = StartOutcome::ThreadCreationFailed;
    try {
        outcome = std::forward<Prepare>(prepare)();
    } catch (...) {
        outcome = StartOutcome::ThreadCreationFailed;
    }
    try {
        std::forward<Report>(report)(outcome);
    } catch (...) {
    }
    return outcome;
}

inline bool reap_completed_worker(std::thread &worker, bool active)
{
    if (!worker.joinable() || active) return false;
    worker.join();
    return true;
}

template <typename Cancel, typename Wake>
bool cancel_and_join_worker(std::thread &worker, Cancel &&cancel,
                            Wake &&wake) noexcept
{
    try {
        std::forward<Cancel>(cancel)();
        std::forward<Wake>(wake)();
        if (!worker.joinable()) return false;
        worker.join();
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace cp0::imu
