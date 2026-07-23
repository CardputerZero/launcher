#include "cp0_battery_lifecycle.hpp"

#include <utility>

namespace cp0::battery {

namespace {

void release_safely(std::function<void()> &release) noexcept
{
    if (!release) return;
    try {
        release();
    } catch (...) {
    }
    release = {};
}

} // namespace

Lifecycle::~Lifecycle()
{
    stop();
}

bool Lifecycle::start(const Factory &handler_factory, const Factory &timer_factory)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_)
        return true;
    if (!handler_factory || !timer_factory)
        return false;

    LifecycleResource handler;
    LifecycleResource timer;
    try {
        handler = handler_factory();
        if (!handler.valid) {
            release_safely(handler.release);
            return false;
        }
        timer = timer_factory();
    } catch (...) {
        release_safely(handler.release);
        return false;
    }
    if (!timer.valid) {
        release_safely(timer.release);
        release_safely(handler.release);
        return false;
    }

    release_handler_ = std::move(handler.release);
    release_timer_ = std::move(timer.release);
    active_ = true;
    return true;
}

void Lifecycle::stop()
{
    std::function<void()> release_timer;
    std::function<void()> release_handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_)
            return;
        active_ = false;
        release_timer = std::move(release_timer_);
        release_handler = std::move(release_handler_);
    }
    release_safely(release_timer);
    release_safely(release_handler);
}

bool Lifecycle::active() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}

} // namespace cp0::battery
