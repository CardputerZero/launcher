#pragma once

#include <functional>
#include <mutex>
#include <utility>

template <typename Handle>
class PageTimerLifecycle
{
public:
    using Deleter = std::function<void(Handle)>;

    PageTimerLifecycle() = default;
    ~PageTimerLifecycle() noexcept { stop(); }

    PageTimerLifecycle(const PageTimerLifecycle &) = delete;
    PageTimerLifecycle &operator=(const PageTimerLifecycle &) = delete;

    template <typename Factory>
    bool start(Factory &&factory, Deleter deleter)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle_)
            return true;
        Handle handle = std::forward<Factory>(factory)();
        if (!handle)
            return false;
        handle_ = handle;
        deleter_ = std::move(deleter);
        return true;
    }

    void stop() noexcept
    {
        Handle handle{};
        Deleter deleter;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            handle = handle_;
            handle_ = Handle{};
            deleter = std::move(deleter_);
        }
        if (handle && deleter) {
            try {
                deleter(handle);
            } catch (...) {
            }
        }
    }

    bool current(Handle handle) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return handle && handle_ == handle;
    }

    bool active() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<bool>(handle_);
    }

private:
    mutable std::mutex mutex_;
    Handle handle_{};
    Deleter deleter_;
};
