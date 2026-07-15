#pragma once

namespace cp0_battery_testable {

inline bool measurement_is_valid(int present, int soc, int current_ma, int temperature_c10)
{
    return present == 1 &&
        soc >= 0 && soc <= 100 &&
        current_ma >= -5000 && current_ma <= 5000 &&
        temperature_c10 >= -400 && temperature_c10 <= 1000;
}

inline bool power_supply_type_is_battery(const char *type)
{
    if (!type) return false;
    const char expected[] = "Battery";
    const char *left = type;
    const char *right = expected;
    while (*left && *left == *right) { ++left; ++right; }
    return *left == '\0' && *right == '\0';
}

inline bool power_supply_status_is_known(const char *status)
{
    if (!status) return false;
    const char *known[] = {"Charging", "Discharging", "Not charging", "Full"};
    for (const char *value : known) {
        const char *left = status;
        const char *right = value;
        while (*left && *left == *right) { ++left; ++right; }
        if (*left == '\0' && *right == '\0') return true;
    }
    return false;
}

} // namespace cp0_battery_testable
