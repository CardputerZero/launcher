#pragma once

#include <utility>

extern "C" void deinit_battery(void);

namespace cp0::battery {

template <typename StopBattery, typename DeinitializeLvgl>
void shutdown_runtime(StopBattery &&stop_battery,
                      DeinitializeLvgl &&deinitialize_lvgl) noexcept
{
    try {
        std::forward<StopBattery>(stop_battery)();
    } catch (...) {
    }
    try {
        std::forward<DeinitializeLvgl>(deinitialize_lvgl)();
    } catch (...) {
    }
}

} // namespace cp0::battery
