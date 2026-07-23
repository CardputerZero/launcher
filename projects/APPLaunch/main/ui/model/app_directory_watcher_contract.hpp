#pragma once

#include "integer_parse_policy.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <mutex>
#include <string>
#include <utility>

class AppDirectoryChangeCallbackSlot
{
public:
    void set(std::function<void()> callback)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        callback_ = std::move(callback);
    }

    void notify()
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!callback_) return;
        auto callback = callback_;
        callback();
    }

private:
    std::recursive_mutex mutex_;
    std::function<void()> callback_;
};

inline bool parse_app_watcher_handle(const std::string &text, uintptr_t &handle)
{
    if (text.size() > 2 && text[0] == '0' &&
        (text[1] == 'x' || text[1] == 'X')) {
        const std::string digits = text.substr(2);
        return launcher_ui::integer_parse::complete(
            digits, uintptr_t{1}, std::numeric_limits<uintptr_t>::max(), handle,
            16);
    }
    return launcher_ui::integer_parse::complete(
        text, uintptr_t{1}, std::numeric_limits<uintptr_t>::max(), handle);
}

inline bool parse_app_watcher_change_count(const std::string &text, int &count)
{
    return launcher_ui::integer_parse::complete(
        text, 0, std::numeric_limits<int>::max(), count);
}

template <typename CallbackTimer, typename ActiveTimer>
constexpr bool app_watcher_timer_is_current(CallbackTimer callback_timer,
                                            ActiveTimer active_timer)
{
    return callback_timer && callback_timer == active_timer;
}
