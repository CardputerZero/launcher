/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "../main/ui/low_battery_flow.hpp"

#include <cassert>
#include <cstdint>
#include <initializer_list>

int main()
{
    using launcher_battery_ui::LowBatteryFlow;
    using launcher_battery_ui::LowBatteryWarning;

    LowBatteryFlow flow;
    flow.update(true, 5, false, 100);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 4, false, 200);
    assert(flow.warning() == LowBatteryWarning::Low);

    flow.update(true, 3, false, 1000);
    assert(flow.warning() == LowBatteryWarning::ShutdownCountdown);
    assert(flow.seconds_until_shutdown(1000) == 15);
    flow.update(true, 2, false, 4000);
    assert(flow.seconds_until_shutdown(4000) == 12);
    assert(!flow.take_shutdown_due(15999));
    assert(flow.take_shutdown_due(16000));
    assert(!flow.take_shutdown_due(16001));

    flow.update(true, 0, true, 18000);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 4, false, 20000);
    flow.update(false, 0, false, 21000);
    assert(flow.warning() == LowBatteryWarning::Undefined);

    flow.update(true, 0, false, UINT32_MAX - 5000u);
    assert(!flow.take_shutdown_due(4999));
    assert(flow.take_shutdown_due(9999));

    // All reminder boundaries, including starting the launcher on low battery.
    for (int soc : {6, 10, 20}) {
        flow.reset();
        flow.update(true, soc, false, 100);
        assert(flow.warning() == LowBatteryWarning::Reminder);
        assert(flow.reminder_soc() == soc);
        assert(!flow.take_shutdown_due(100));
        flow.update(true, soc, false, 3100);
        flow.update(true, soc, false, 5100);
        assert(flow.warning() == LowBatteryWarning::Reminder);
        flow.update(true, soc, false, 600000);
        assert(flow.warning() == LowBatteryWarning::Reminder);
        flow.dismiss_reminder();
        assert(flow.warning() == LowBatteryWarning::None);
        flow.update(true, 6, false, 603000);
        assert(flow.warning() == LowBatteryWarning::None);
    }

    // Leaving the range and unplugging both arm the next reminder.
    flow.update(true, 21, false, 11000);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 20, false, 12000);
    assert(flow.warning() == LowBatteryWarning::Reminder);
    flow.update(true, 5, false, 12100);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 5, true, 12200);
    flow.update(true, 5, false, 12300);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 6, false, 12400);
    assert(flow.warning() == LowBatteryWarning::Reminder);
    flow.update(true, 15, true, 13000);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 15, true, 16000);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 15, false, 19000);
    assert(flow.warning() == LowBatteryWarning::Reminder);
    flow.dismiss_reminder();
    flow.update(true, 14, false, 25000);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 14, true, 26000);
    flow.update(true, 14, false, 27000);
    assert(flow.warning() == LowBatteryWarning::Reminder);

    // A failed read cancels the popup without repeating it on recovery.
    flow.update(false, 0, false, 28000);
    assert(flow.warning() == LowBatteryWarning::Undefined);
    flow.update(true, 14, false, 29000);
    assert(flow.warning() == LowBatteryWarning::None);
    flow.update(true, 14, true, 30000);
    flow.update(false, 0, false, 31000);
    flow.update(true, 14, false, 32000);
    assert(flow.warning() == LowBatteryWarning::Reminder);

    // Critical warnings replace the reminder immediately.
    flow.update(true, 4, false, 33000);
    assert(flow.warning() == LowBatteryWarning::Low);
    flow.dismiss_reminder();
    assert(flow.warning() == LowBatteryWarning::Low);
    flow.update(true, 0, false, 51000);
    assert(flow.warning() == LowBatteryWarning::ShutdownCountdown);
    flow.dismiss_reminder();
    assert(flow.warning() == LowBatteryWarning::ShutdownCountdown);

    // Rechecking the battery at the deadline must cancel obsolete shutdowns.
    for (int soc : {4, 5, 6, 20, 21}) {
        flow.reset();
        flow.update(true, 3, false, 1000);
        assert(flow.shutdown_due(16000));
        flow.update(true, soc, false, 16000);
        assert(!flow.take_shutdown_due(16000));
        assert(flow.warning() == (soc < 5 ? LowBatteryWarning::Low
                                  : soc > 5 && soc <= 20 ? LowBatteryWarning::Reminder
                                              : LowBatteryWarning::None));
        flow.update(true, 3, false, 17000);
        assert(flow.seconds_until_shutdown(17000) == 15);
        assert(!flow.take_shutdown_due(31999));
        assert(flow.take_shutdown_due(32000));
    }
    // Every value at or below 3% starts the countdown, including skipped readings.
    for (int soc : {0, 1, 2, 3}) {
        flow.reset();
        flow.update(true, soc, false, 1000);
        assert(flow.warning() == LowBatteryWarning::ShutdownCountdown);
        assert(!flow.take_shutdown_due(15999));
        assert(flow.take_shutdown_due(16000));
        assert(!flow.take_shutdown_due(16001));
        flow.update(true, soc, true, 17000);
        assert(flow.warning() == LowBatteryWarning::None);
        assert(!flow.take_shutdown_due(32000));
    }

    // Fluctuations within 0%-3% must not restart the countdown.
    flow.reset();
    flow.update(true, 3, false, 1000);
    flow.update(true, 0, false, 4000);
    flow.update(true, 1, false, 7000);
    flow.update(true, 2, false, 10000);
    flow.update(true, 3, false, 13000);
    assert(flow.seconds_until_shutdown(13000) == 3);
    assert(flow.take_shutdown_due(16000));

    for (bool charging : {false, true}) {
        flow.reset();
        flow.update(true, 3, false, 1000);
        flow.update(charging, 3, charging, 16000);
        assert(!flow.take_shutdown_due(16000));
        assert(flow.warning() == (charging ? LowBatteryWarning::None
                                          : LowBatteryWarning::Undefined));
    }

    flow.reset();
    flow.update(true, 20, false, UINT32_MAX - 2000u);
    flow.update(true, 20, false, 2998);
    assert(flow.warning() == LowBatteryWarning::Reminder);
    flow.update(true, 20, false, 600000);
    assert(flow.warning() == LowBatteryWarning::Reminder);
    flow.dismiss_reminder();
    assert(flow.warning() == LowBatteryWarning::None);
}
