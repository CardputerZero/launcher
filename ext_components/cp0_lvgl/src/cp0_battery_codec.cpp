#include "cp0_battery_codec.hpp"

#include "cp0_battery_testable.hpp"

#include <cmath>
#include <cstring>
#include <sstream>

namespace cp0::battery {

std::string encode_info(const cp0_battery_info_t &info)
{
    std::ostringstream output;
    output << info.voltage_mv << ','
           << info.current_ma << ','
           << info.temperature_c10 << ','
           << info.soc << ','
           << info.remain_mah << ','
           << info.full_mah << ','
           << info.flags << ','
           << info.avg_current_ma << ','
           << info.valid;
    return output.str();
}

bool decode_info(const std::string &data, cp0_battery_info_t *info)
{
    if (!info) return false;

    cp0_battery_info_t parsed{};
    char separator = '\0';
    std::istringstream input(data);
    const auto comma = [&]() {
        return static_cast<bool>(input >> separator) && separator == ',';
    };
    if (input >> parsed.voltage_mv && comma() &&
        input >> parsed.current_ma && comma() &&
        input >> parsed.temperature_c10 && comma() &&
        input >> parsed.soc && comma() &&
        input >> parsed.remain_mah && comma() &&
        input >> parsed.full_mah && comma() &&
        input >> parsed.flags && comma() &&
        input >> parsed.avg_current_ma && comma() &&
        input >> parsed.valid && (input >> std::ws).eof()) {
        if (!info_is_valid(parsed)) return false;
        *info = parsed;
        return true;
    }
    return false;
}

bool info_is_valid(const cp0_battery_info_t &info)
{
    if (info.valid != 0 && info.valid != 1) return false;
    if (info.voltage_mv < 0 || info.voltage_mv > 20000 ||
        info.current_ma < -5000 || info.current_ma > 5000 ||
        info.avg_current_ma < -5000 || info.avg_current_ma > 5000 ||
        info.temperature_c10 < -400 || info.temperature_c10 > 1000 ||
        info.soc < 0 || info.soc > 100 || info.remain_mah < 0 || info.full_mah < 0 ||
        info.flags < 0)
        return false;
    if (info.full_mah > 0 && info.remain_mah > info.full_mah) return false;
    return true;
}

cp0_battery_info_t from_power_supply(int present,
                                     int capacity_percent,
                                     long voltage_uv,
                                     long current_ua,
                                     int temperature_c10,
                                     const char *status)
{
    cp0_battery_info_t info{};
    constexpr long kMaximumVoltageUv = 20000000;
    constexpr long kMinimumCurrentUa = -5000499;
    constexpr long kMaximumCurrentUa = 5000499;
    if (voltage_uv < 0 || voltage_uv > kMaximumVoltageUv ||
        current_ua < kMinimumCurrentUa || current_ua > kMaximumCurrentUa)
        return info;
    const int current_ma = static_cast<int>(std::lround(current_ua / 1000.0));
    if (!cp0_battery_testable::measurement_is_valid(
            present, capacity_percent, current_ma, temperature_c10) ||
        !cp0_battery_testable::power_supply_status_is_known(status)) {
        return info;
    }

    info.voltage_mv = static_cast<int>(voltage_uv / 1000);
    info.current_ma = current_ma;
    info.temperature_c10 = temperature_c10;
    info.soc = capacity_percent;
    info.flags = std::strcmp(status, "Charging") == 0 ? 1 : 0;
    info.avg_current_ma = current_ma;
    info.valid = 1;
    return info;
}

} // namespace cp0::battery
