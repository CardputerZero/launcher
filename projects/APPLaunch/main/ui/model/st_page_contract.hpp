#pragma once

#include <charconv>
#include <string>
#include <system_error>

inline bool st_parse_child_status(const std::string &text, int &status)
{
    if (text.empty()) return false;
    int parsed = 0;
    const char *begin = text.data();
    const char *end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed, 10);
    if (result.ec != std::errc() || result.ptr != end) return false;
    status = parsed;
    return true;
}

template <typename... Handles>
constexpr bool st_page_renderer_ready(Handles... handles)
{
    return (... && static_cast<bool>(handles));
}

struct STCallbackFailureState
{
    bool terminal_active;
    bool waiting_key_to_exit;
    int home_hold_status;
};

constexpr STCallbackFailureState st_callback_failure_state() noexcept
{
    return {false, true, 0};
}
