#include "cp0_battery_lifecycle.hpp"

#include <cassert>
#include <stdexcept>

int main()
{
    int timer = 0;
    int deletes = 0;
    assert(cp0::battery::release_timer_if_runtime_active(
        &timer, true, [&](int *handle) {
            assert(handle == &timer);
            ++deletes;
        }));
    assert(deletes == 1);

    assert(!cp0::battery::release_timer_if_runtime_active(
        &timer, false, [&](int *) { ++deletes; }));
    assert(!cp0::battery::release_timer_if_runtime_active(
        static_cast<int *>(nullptr), true, [&](int *) { ++deletes; }));
    assert(deletes == 1);

    assert(cp0::battery::release_timer_if_runtime_active(
        &timer, true, [](int *) { throw std::runtime_error("delete"); }));
}
