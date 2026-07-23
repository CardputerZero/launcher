#pragma once

#include <cstddef>

template <typename EventTarget, typename CurrentTarget>
constexpr bool ssh_owned_delete_callback_allowed(EventTarget event_target,
                                                  CurrentTarget current_target) noexcept
{
    return event_target && event_target == current_target;
}

template <typename CurrentTarget, typename RootScreen>
constexpr bool ssh_page_event_callback_allowed(CurrentTarget current_target,
                                                RootScreen root_screen) noexcept
{
    return current_target && current_target == root_screen;
}

template <typename PageHandle>
constexpr bool ssh_restore_completion_allowed(bool token_completed,
                                              PageHandle page) noexcept
{
    return token_completed && page;
}

class SshViewBuildContract
{
public:
    explicit constexpr SshViewBuildContract(std::size_t required_rows)
        : required_rows_(required_rows)
    {
    }

    constexpr void row_completed() { ++completed_rows_; }
    constexpr void hint_completed() { hint_completed_ = true; }

    constexpr bool ready() const
    {
        return completed_rows_ == required_rows_ && hint_completed_;
    }

private:
    std::size_t required_rows_;
    std::size_t completed_rows_ = 0;
    bool hint_completed_ = false;
};
