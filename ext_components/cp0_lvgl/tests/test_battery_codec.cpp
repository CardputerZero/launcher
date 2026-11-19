/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "../src/cp0_battery_codec.hpp"

#include <cassert>

namespace {

cp0_battery_info_t with_flags(int flags)
{
    cp0_battery_info_t info = cp0::battery::from_power_supply(
        1, 50, 3800000, 0, 250, "Discharging");
    info.flags = flags;
    return info;
}

void test_gauge_extras()
{
    using cp0::battery::GaugeExtras;
    using cp0::battery::apply_gauge_extras;

    // Everything missing: capacity stays 0, extras stay unknown.
    cp0_battery_info_t info = with_flags(CP0_BATTERY_FLAG_DISCHARGING);
    apply_gauge_extras(info, GaugeExtras{});
    assert(info.remain_mah == 0 && info.full_mah == 0);
    assert(info.time_to_empty_min == -1 && info.time_to_full_min == -1 &&
           info.cycle_count == -1);
    assert(cp0::battery::info_is_valid(info));

    // Discharging: capacity, time to empty (rounded up), cycles; no time to full.
    GaugeExtras raw;
    raw.charge_now_uah = 860000;
    raw.charge_full_uah = 1750000;
    raw.time_to_empty_s = 5430;
    raw.time_to_full_s = 1200;
    raw.cycle_count = 0;
    info = with_flags(CP0_BATTERY_FLAG_DISCHARGING);
    apply_gauge_extras(info, raw);
    assert(info.remain_mah == 860 && info.full_mah == 1750);
    assert(info.time_to_empty_min == 91 && info.time_to_full_min == -1);
    assert(info.cycle_count == 0);
    assert(cp0::battery::info_is_valid(info));

    // Charging: only time to full; sub-minute values round up to 1.
    raw.time_to_full_s = 30;
    info = with_flags(CP0_BATTERY_FLAG_CHARGING);
    apply_gauge_extras(info, raw);
    assert(info.time_to_empty_min == -1 && info.time_to_full_min == 1);

    // Full / Not charging / unknown state: neither time is shown.
    for (int flags : {CP0_BATTERY_FLAG_FULL, CP0_BATTERY_FLAG_NOT_CHARGING, 0}) {
        info = with_flags(flags);
        apply_gauge_extras(info, raw);
        assert(info.time_to_empty_min == -1 && info.time_to_full_min == -1);
        assert(info.remain_mah == 860 && info.cycle_count == 0);
    }

    // A 0 time from the gauge is unknown, not "0 min".
    raw.time_to_empty_s = 0;
    raw.time_to_full_s = 0;
    info = with_flags(CP0_BATTERY_FLAG_DISCHARGING);
    apply_gauge_extras(info, raw);
    assert(info.time_to_empty_min == -1);
    info = with_flags(CP0_BATTERY_FLAG_CHARGING);
    apply_gauge_extras(info, raw);
    assert(info.time_to_full_min == -1);

    // charge_now > charge_full is clamped so the info stays valid.
    raw.charge_now_uah = 1800000;
    info = with_flags(CP0_BATTERY_FLAG_FULL);
    apply_gauge_extras(info, raw);
    assert(info.remain_mah == 1750 && info.full_mah == 1750);
    assert(cp0::battery::info_is_valid(info));

    // Out-of-range values are ignored individually.
    GaugeExtras bad;
    bad.charge_now_uah = 500000;
    bad.charge_full_uah = 0;              // zero full capacity
    bad.time_to_empty_s = 60L * 60 * 24 * 31; // more than 30 days
    bad.cycle_count = 100000;
    info = with_flags(CP0_BATTERY_FLAG_DISCHARGING);
    apply_gauge_extras(info, bad);
    assert(info.remain_mah == 0 && info.full_mah == 0);
    assert(info.time_to_empty_min == -1 && info.cycle_count == -1);

    bad.charge_now_uah = -1;              // missing charge_now
    bad.charge_full_uah = 1750000;
    bad.time_to_empty_s = -5;
    bad.cycle_count = -3;
    info = with_flags(CP0_BATTERY_FLAG_DISCHARGING);
    apply_gauge_extras(info, bad);
    assert(info.remain_mah == 0 && info.full_mah == 0);
    assert(info.time_to_empty_min == -1 && info.cycle_count == -1);

    bad.charge_now_uah = 1000;
    bad.charge_full_uah = 100000001L;     // more than 100 Ah
    info = with_flags(CP0_BATTERY_FLAG_DISCHARGING);
    apply_gauge_extras(info, bad);
    assert(info.full_mah == 0);

    // Invalid infos are left alone apart from clearing the extras.
    cp0_battery_info_t invalid{};
    apply_gauge_extras(invalid, raw);
    assert(invalid.valid == 0 && invalid.remain_mah == 0 && invalid.full_mah == 0);
    assert(invalid.time_to_empty_min == -1 && invalid.cycle_count == -1);
}

} // namespace

int main()
{
    cp0_battery_info_t source{4100, -125, 273, 81, 2430, 3000, 4, -120, 1, 95, -1, 3};
    const std::string encoded = cp0::battery::encode_info(source);
    assert(encoded == "4100,-125,273,81,2430,3000,4,-120,1,95,-1,3");

    cp0_battery_info_t decoded{};
    assert(cp0::battery::decode_info(encoded, &decoded));
    assert(decoded.voltage_mv == source.voltage_mv);
    assert(decoded.current_ma == source.current_ma);
    assert(decoded.temperature_c10 == source.temperature_c10);
    assert(decoded.soc == source.soc);
    assert(decoded.remain_mah == source.remain_mah);
    assert(decoded.full_mah == source.full_mah);
    assert(decoded.flags == source.flags);
    assert(decoded.avg_current_ma == source.avg_current_ma);
    assert(decoded.valid == source.valid);
    assert(decoded.time_to_empty_min == 95);
    assert(decoded.time_to_full_min == -1);
    assert(decoded.cycle_count == 3);

    // Legacy 9-field payloads still decode; the appended fields are unknown.
    cp0_battery_info_t legacy{};
    assert(cp0::battery::decode_info("4100,-125,273,81,2430,3000,4,-120,1", &legacy));
    assert(legacy.soc == 81 && legacy.valid == 1);
    assert(legacy.time_to_empty_min == -1 && legacy.time_to_full_min == -1 &&
           legacy.cycle_count == -1);
    assert(!cp0::battery::decode_info("4100,-125,273,81,2430,3000,4,-120,1,", &decoded));
    assert(!cp0::battery::decode_info("4100,-125,273,81,2430,3000,4,-120,1,5", &decoded));
    assert(!cp0::battery::decode_info("4100,-125,273,81,2430,3000,4,-120,1,5,6", &decoded));
    assert(!cp0::battery::decode_info("4100,-125,273,81,2430,3000,4,-120,1,-2,-1,3", &decoded));
    assert(!cp0::battery::decode_info("4100,-125,273,81,2430,3000,4,-120,1,-1,-2,3", &decoded));
    assert(!cp0::battery::decode_info("4100,-125,273,81,2430,3000,4,-120,1,-1,-1,-2", &decoded));
    assert(!cp0::battery::decode_info("4100,-125,273", &decoded));
    assert(!cp0::battery::decode_info("4100;-125,273,81,2430,3000,4,-120,1", &decoded));
    assert(!cp0::battery::decode_info(encoded + ",extra", &decoded));
    assert(cp0::battery::decode_info(encoded + "  \n", &decoded));
    assert(!cp0::battery::decode_info(encoded, nullptr));
    assert(!cp0::battery::decode_info("4100,-125,273,101,2430,3000,4,-120,1", &decoded));
    assert(!cp0::battery::decode_info("4100,-125,273,81,3001,3000,4,-120,1", &decoded));
    assert(!cp0::battery::decode_info("4100,-125,273,81,2430,3000,4,-120,2", &decoded));
    assert(!cp0::battery::decode_info("-1,-125,273,81,2430,3000,4,-120,1", &decoded));

    const cp0_battery_info_t unavailable{};
    assert(cp0::battery::info_is_valid(unavailable));

    const cp0_battery_info_t charging = cp0::battery::from_power_supply(
        1, 81, 4100123, 125500, 273, "Charging");
    assert(charging.valid == 1);
    assert(charging.voltage_mv == 4100);
    assert(charging.current_ma == 126);
    assert(charging.avg_current_ma == 126);
    assert(charging.temperature_c10 == 273);
    assert(charging.soc == 81);
    assert(charging.flags == CP0_BATTERY_FLAG_CHARGING);
    assert(charging.time_to_empty_min == -1 && charging.time_to_full_min == -1 &&
           charging.cycle_count == -1);

    const cp0_battery_info_t discharging = cp0::battery::from_power_supply(
        1, 20, 3600000, -125500, 250, "Discharging");
    assert(discharging.valid == 1);
    assert(discharging.current_ma == -126);
    assert(discharging.flags == CP0_BATTERY_FLAG_DISCHARGING);
    assert((discharging.flags & CP0_BATTERY_FLAG_CHARGING) == 0);

    const cp0_battery_info_t full = cp0::battery::from_power_supply(
        1, 100, 4200000, 0, 250, "Full");
    assert(full.valid == 1 && full.flags == CP0_BATTERY_FLAG_FULL);
    const cp0_battery_info_t idle = cp0::battery::from_power_supply(
        1, 98, 4180000, 0, 250, "Not charging");
    assert(idle.valid == 1 && idle.flags == CP0_BATTERY_FLAG_NOT_CHARGING);
    assert((idle.flags & CP0_BATTERY_FLAG_CHARGING) == 0);

    test_gauge_extras();

    assert(!cp0::battery::from_power_supply(
                0, 81, 4100000, 0, 273, "Charging").valid);
    assert(!cp0::battery::from_power_supply(
                1, 101, 4100000, 0, 273, "Charging").valid);
    assert(!cp0::battery::from_power_supply(
                1, 81, 4100000, 5000500, 273, "Charging").valid);
    assert(!cp0::battery::from_power_supply(
                1, 81, -1, 0, 273, "Charging").valid);
    assert(!cp0::battery::from_power_supply(
                1, 81, 20000001, 0, 273, "Charging").valid);
    assert(!cp0::battery::from_power_supply(
                1, 81, 4100000, -5000500, 273, "Charging").valid);
    assert(!cp0::battery::from_power_supply(
                1, 81, 4100000, 0, 273, "Unknown").valid);
}
