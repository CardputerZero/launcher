#pragma once

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace cp0::callback {

inline void invoke(const std::function<void(int, std::string)> &callback,
                   int code, const std::string &payload) noexcept
{
    if (!callback) return;
    try {
        callback(code, payload);
    } catch (...) {
    }
}

template <typename Callback, typename... Arguments>
inline void invoke_direct(Callback &&callback, Arguments &&...arguments) noexcept
{
    if constexpr (std::is_pointer_v<std::remove_reference_t<Callback>>) {
        if (!callback) return;
    }
    try {
        callback(std::forward<Arguments>(arguments)...);
    } catch (...) {
    }
}

template <typename Owner, typename Callback>
inline void invoke_if_present(Owner *owner, Callback &&callback) noexcept
{
    if (!owner) return;
    try {
        std::forward<Callback>(callback)(*owner);
    } catch (...) {
    }
}

} // namespace cp0::callback
