#include "../main/ui/low_battery_flow.hpp"

#include <cassert>
#include <cstdint>

int main()
{
    using launcher_battery_ui::LowBatteryFlow;
    using launcher_battery_ui::LowBatteryWarning;

    LowBatteryFlow flow;
    flow.update(true, 5, false, 100);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 4, false, 200);
    assert(flow.warning() == LowBatteryWarning::Low);

    flow.update(true, 0, false, 1000);
    assert(flow.warning() == LowBatteryWarning::ShutdownCountdown);
    assert(flow.seconds_until_shutdown(1000) == 15);
    assert(!flow.confirm_shutdown(true, false, 15999));
    assert(flow.confirm_shutdown(true, false, 16000));

    flow.update(true, 0, true, 18000);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 4, false, 20000);
    flow.update(false, 0, false, 21000);
    assert(flow.warning() == LowBatteryWarning::None);

    flow.update(true, 0, false, UINT32_MAX - 5000u);
    assert(!flow.confirm_shutdown(true, false, 4999));
    assert(flow.confirm_shutdown(true, false, 9999));
}
