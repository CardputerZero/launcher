#include "../src/cp0_battery_testable.hpp"

#include <cassert>

int main()
{
    using cp0_battery_testable::measurement_is_valid;
    using cp0_battery_testable::power_supply_status_is_known;

    assert(measurement_is_valid(4, -250));
    assert(!measurement_is_valid(101, -250));
    assert(power_supply_status_is_known("Charging"));
    assert(power_supply_status_is_known("Not charging"));
    assert(!power_supply_status_is_known("Unknown"));
    assert(!power_supply_status_is_known(nullptr));
}
