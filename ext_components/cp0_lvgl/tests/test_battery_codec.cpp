#include "../src/cp0_battery_codec.hpp"

#include <cassert>

int main()
{
    cp0_battery_info_t source{4100, -125, 273, 81, 2430, 3000, 4, -120, 1};
    const std::string encoded = cp0::battery::encode_info(source);
    assert(encoded == "4100,-125,273,81,2430,3000,4,-120,1");

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
    assert(charging.flags == 1);

    const cp0_battery_info_t discharging = cp0::battery::from_power_supply(
        1, 20, 3600000, -125500, 250, "Discharging");
    assert(discharging.valid == 1);
    assert(discharging.current_ma == -126);
    assert(discharging.flags == 0);

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
