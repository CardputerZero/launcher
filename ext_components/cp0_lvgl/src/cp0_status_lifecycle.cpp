#include "cp0_status_lifecycle.hpp"

#include <utility>

namespace cp0::status {

namespace {

void release_reverse(std::vector<std::function<void()>> &releases) noexcept
{
    for (auto iterator = releases.rbegin(); iterator != releases.rend(); ++iterator) {
        if (!*iterator) continue;
        try {
            (*iterator)();
        } catch (...) {
            // Cleanup is best-effort, but one faulty resource must not retain the rest.
        }
    }
    releases.clear();
}

} // namespace

Lifecycle::~Lifecycle()
{
    stop();
}

bool Lifecycle::start(const std::vector<Factory> &factories)
{
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return !starting_; });
    if (active_)
        return true;
    starting_ = true;
    cancel_start_ = false;
    lock.unlock();

    std::vector<std::function<void()>> acquired;
    acquired.reserve(factories.size());
    try {
        for (const Factory &factory : factories) {
            Resource resource = factory ? factory() : Resource{};
            if (!resource.valid) {
                release_reverse(acquired);
                lock.lock();
                starting_ = false;
                cancel_start_ = false;
                lock.unlock();
                condition_.notify_all();
                return false;
            }
            acquired.push_back(std::move(resource.release));
        }
    } catch (...) {
        release_reverse(acquired);
        lock.lock();
        starting_ = false;
        cancel_start_ = false;
        lock.unlock();
        condition_.notify_all();
        return false;
    }

    lock.lock();
    if (cancel_start_) {
        starting_ = false;
        cancel_start_ = false;
        lock.unlock();
        release_reverse(acquired);
        condition_.notify_all();
        return false;
    }
    releases_ = std::move(acquired);
    active_ = true;
    starting_ = false;
    lock.unlock();
    condition_.notify_all();
    return true;
}

void Lifecycle::stop()
{
    std::vector<std::function<void()>> releases;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (starting_) {
            cancel_start_ = true;
            return;
        }
        if (!active_)
            return;
        active_ = false;
        releases = std::move(releases_);
    }
    release_reverse(releases);
}

bool Lifecycle::active() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}

} // namespace cp0::status
