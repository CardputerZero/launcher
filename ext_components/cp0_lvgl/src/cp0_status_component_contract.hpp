#pragma once

#include <cstddef>
#include <utility>

namespace cp0::status {

template <typename Handle>
bool resources_ready(const Handle *resources, std::size_t count)
{
    if (!resources && count != 0) return false;
    for (std::size_t index = 0; index < count; ++index)
        if (!resources[index]) return false;
    return true;
}

template <typename Deactivate, typename Unmount>
void release_before_destroy(Deactivate &&deactivate, Unmount &&unmount) noexcept
{
    try {
        std::forward<Deactivate>(deactivate)();
    } catch (...) {
    }
    try {
        std::forward<Unmount>(unmount)();
    } catch (...) {
    }
}

template <typename Initialize, typename Ready, typename Rollback>
bool initialize_with_rollback(Initialize &&initialize, Ready &&ready,
                              Rollback &&rollback) noexcept
{
    bool initialized = false;
    try {
        std::forward<Initialize>(initialize)();
        initialized = std::forward<Ready>(ready)();
    } catch (...) {
    }
    if (initialized) return true;
    try {
        std::forward<Rollback>(rollback)();
    } catch (...) {
    }
    return false;
}

} // namespace cp0::status
