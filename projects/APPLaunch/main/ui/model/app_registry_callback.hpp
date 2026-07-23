#pragma once

#include <mutex>

class AppRegistryCallbackSlot
{
public:
    using Callback = void (*)(void *);

    void set(Callback callback, void *user_data)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        callback_ = callback;
        user_data_ = user_data;
    }

    void notify()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        Callback callback = callback_;
        void *user_data = user_data_;
        if (callback) callback(user_data);
    }

    bool clear_if_matches(Callback callback, void *user_data)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (callback_ != callback || user_data_ != user_data) return false;
        callback_ = nullptr;
        user_data_ = nullptr;
        return true;
    }

private:
    std::recursive_mutex mutex_;
    Callback callback_ = nullptr;
    void *user_data_ = nullptr;
};
