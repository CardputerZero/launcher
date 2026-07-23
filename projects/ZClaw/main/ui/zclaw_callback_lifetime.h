#pragma once

#include <functional>
#include <mutex>
#include <utility>

namespace zclaw {

template <typename Target>
class CallbackLifetime {
public:
    explicit CallbackLifetime(Target *target) : target_(target) {}

    CallbackLifetime(const CallbackLifetime &) = delete;
    CallbackLifetime &operator=(const CallbackLifetime &) = delete;

    void invalidate()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        target_ = nullptr;
    }

    template <typename Callback>
    bool invoke(Callback &&callback)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!target_)
            return false;
        std::invoke(std::forward<Callback>(callback), *target_);
        return true;
    }

private:
    std::mutex mutex_;
    Target *target_;
};

}  // namespace zclaw
