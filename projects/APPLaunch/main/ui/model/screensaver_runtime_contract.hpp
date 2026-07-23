#pragma once

#include <charconv>
#include <string>

template <typename CallbackTimer, typename ActiveTimer>
constexpr bool screensaver_timer_is_current(CallbackTimer callback_timer,
                                            ActiveTimer active_timer)
{
    return callback_timer && callback_timer == active_timer;
}

template <typename Target, typename CurrentTarget, typename Owned>
constexpr bool screensaver_delete_is_tracked(Target target,
                                              CurrentTarget current_target,
                                              Owned owned)
{
    return target && target == current_target && target == owned;
}

inline int screensaver_timeout_from_config(bool request_succeeded,
                                           const std::string &text,
                                           int default_seconds = 30)
{
    if (!request_succeeded || text.empty()) return default_seconds;
    int value = 0;
    const char *begin = text.data();
    const char *end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) return default_seconds;
    switch (value) {
    case 0:
    case 10:
    case 30:
    case 60:
    case 300:
        return value;
    default:
        return default_seconds;
    }
}
