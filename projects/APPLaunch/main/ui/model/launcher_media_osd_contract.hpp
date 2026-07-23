#pragma once

template <typename Container, typename Icon, typename Title, typename Value, typename Bar>
constexpr bool launcher_media_osd_ui_ready(Container container,
                                           Icon icon,
                                           Title title,
                                           Value value,
                                           Bar bar)
{
    return container && icon && title && value && bar;
}

template <typename CallbackTimer, typename ActiveTimer>
constexpr bool launcher_media_osd_timer_is_current(CallbackTimer callback_timer,
                                                   ActiveTimer active_timer)
{
    return callback_timer && callback_timer == active_timer;
}
