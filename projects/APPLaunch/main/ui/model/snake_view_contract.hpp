#pragma once

#include <cstddef>

template <typename Target, typename CurrentTarget>
constexpr bool snake_owned_delete_callback_allowed(
    Target target, CurrentTarget current_target) noexcept
{
    return target && target == current_target;
}

template <typename GameArea, typename RenderLayer>
constexpr bool snake_tick_callback_allowed(bool callback_enabled,
                                           bool timer_current,
                                           bool game_playing,
                                           GameArea game_area,
                                           RenderLayer render_layer) noexcept
{
    return callback_enabled && timer_current && game_playing &&
           game_area && render_layer;
}

template <typename CurrentTarget, typename RootScreen>
constexpr bool snake_page_event_callback_allowed(
    CurrentTarget current_target, RootScreen root_screen) noexcept
{
    return current_target && current_target == root_screen;
}

class SnakeFrameBuildContract
{
public:
    explicit constexpr SnakeFrameBuildContract(std::size_t required_cells)
        : required_cells_(required_cells)
    {
    }

    constexpr void cell_created() { ++created_cells_; }
    constexpr bool ready() const { return created_cells_ == required_cells_; }

private:
    std::size_t required_cells_;
    std::size_t created_cells_ = 0;
};
