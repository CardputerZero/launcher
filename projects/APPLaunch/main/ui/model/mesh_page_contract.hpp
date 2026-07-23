#pragma once

#include "integer_parse_policy.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <string>

inline bool mesh_parse_random_u32(const std::string &text, uint32_t &value)
{
    return launcher_ui::integer_parse::complete<uint32_t>(
        text, 0, std::numeric_limits<uint32_t>::max(), value);
}

inline bool mesh_parse_local_time(const std::string &text, int &hour, int &minute, int &second)
{
    std::array<int, 6> values{};
    size_t begin = 0;
    for (size_t index = 0; index < values.size(); ++index) {
        const size_t end = text.find(',', begin);
        if ((index + 1 < values.size() && end == std::string::npos) ||
            (index + 1 == values.size() && end != std::string::npos))
            return false;
        const std::string field = text.substr(begin, end - begin);
        if (!launcher_ui::integer_parse::complete<int>(
                field, 0, index == 0 ? 9999 : 59, values[index]))
            return false;
        begin = end == std::string::npos ? text.size() : end + 1;
    }
    if (values[1] < 1 || values[1] > 12 || values[2] < 1 || values[2] > 31 ||
        values[3] > 23)
        return false;
    hour = values[3];
    minute = values[4];
    second = values[5];
    return true;
}

template <typename... Handles>
constexpr bool mesh_page_ui_ready(Handles... handles)
{
    return (... && static_cast<bool>(handles));
}

constexpr bool mesh_heartbeat_callback_allowed(bool timer_is_current,
                                               bool heartbeat_enabled) noexcept
{
    return timer_is_current && heartbeat_enabled;
}

template <typename Target, typename CurrentTarget>
constexpr bool mesh_owned_delete_callback_allowed(
    Target target, CurrentTarget current_target) noexcept
{
    return target && target == current_target;
}

template <typename CurrentTarget, typename RootScreen>
constexpr bool mesh_root_event_callback_allowed(CurrentTarget current_target,
                                                RootScreen root_screen) noexcept
{
    return current_target && current_target == root_screen;
}
