#pragma once

#include <cstdint>

class LauncherStartupGeneration
{
public:
    using Token = std::uint64_t;

    Token begin()
    {
        if (++generation_ == 0) ++generation_;
        active_ = true;
        return generation_;
    }

    bool current(Token token) const
    {
        return active_ && token != 0 && token == generation_;
    }

    bool stop(Token token)
    {
        if (!current(token)) return false;
        active_ = false;
        return true;
    }

private:
    Token generation_ = 0;
    bool active_ = false;
};

template <typename CallbackTimerHandle, typename ActiveTimerHandle>
constexpr bool launcher_startup_timer_is_current(CallbackTimerHandle callback_timer,
                                                 ActiveTimerHandle active_timer)
{
    return callback_timer && callback_timer == active_timer;
}

struct LauncherStartupOneShotDecision
{
    bool retire_timer = false;
    bool release_context = false;
    bool complete = false;
};

constexpr LauncherStartupOneShotDecision launcher_startup_one_shot_decision(
    bool timer_is_current, bool context_is_owned, bool generation_is_current)
{
    if (!timer_is_current) return {};
    return {
        true,
        context_is_owned,
        context_is_owned && generation_is_current,
    };
}

constexpr bool launcher_startup_should_recover_deleted_gif(bool is_current_gif,
                                                           bool gif_already_done)
{
    return is_current_gif && !gif_already_done;
}
