#pragma once

#include <functional>
#include <mutex>
#include <utility>

namespace cp0::battery {

struct LifecycleResource {
    bool valid = false;
    std::function<void()> release;
};

template <typename TimerHandle, typename Delete>
bool release_timer_if_runtime_active(TimerHandle timer, bool runtime_active,
                                     Delete &&delete_timer) noexcept
{
    if (!timer || !runtime_active) return false;
    try {
        std::forward<Delete>(delete_timer)(timer);
    } catch (...) {
    }
    return true;
}

class Lifecycle {
public:
    using Factory = std::function<LifecycleResource()>;

    ~Lifecycle();

    bool start(const Factory &handler_factory, const Factory &timer_factory);
    void stop();
    bool active() const;

private:
    mutable std::mutex mutex_;
    bool active_ = false;
    std::function<void()> release_handler_;
    std::function<void()> release_timer_;
};

} // namespace cp0::battery
