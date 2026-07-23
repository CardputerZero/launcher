#pragma once

template <typename BackgroundHandle, typename ArenaHandle, typename PlayerHandle>
constexpr bool tank_battle_ui_ready(BackgroundHandle background,
                                    ArenaHandle arena,
                                    PlayerHandle player)
{
    return background && arena && player;
}

template <typename BackgroundHandle, typename ArenaHandle, typename PlayerHandle>
constexpr bool tank_battle_tick_callback_allowed(bool callback_enabled,
                                                 bool timer_current,
                                                 BackgroundHandle background,
                                                 ArenaHandle arena,
                                                 PlayerHandle player) noexcept
{
    return callback_enabled && timer_current &&
           tank_battle_ui_ready(background, arena, player);
}

template <typename CurrentTarget, typename RootScreen>
constexpr bool tank_battle_root_event_callback_allowed(
    CurrentTarget current_target, RootScreen root_screen) noexcept
{
    return current_target && current_target == root_screen;
}

template <typename ObjectHandle>
constexpr bool tank_battle_owned_delete_callback_allowed(
    ObjectHandle deleted, ObjectHandle current_target) noexcept
{
    return deleted && deleted == current_target;
}
