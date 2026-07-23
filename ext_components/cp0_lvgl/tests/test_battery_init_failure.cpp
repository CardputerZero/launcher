#include "cp0_battery_lifecycle.hpp"

#include <cassert>
#include <vector>

int main()
{
    int invalid_handler_releases = 0;
    cp0::battery::Lifecycle invalid_handler;
    assert(!invalid_handler.start(
        [&] {
            return cp0::battery::LifecycleResource{
                false, [&] { ++invalid_handler_releases; }};
        },
        [] { return cp0::battery::LifecycleResource{true, {}}; }));
    assert(invalid_handler_releases == 1);
    assert(!invalid_handler.active());

    std::vector<int> release_order;
    cp0::battery::Lifecycle invalid_timer;
    assert(!invalid_timer.start(
        [&] {
            return cp0::battery::LifecycleResource{
                true, [&] { release_order.push_back(2); }};
        },
        [&] {
            return cp0::battery::LifecycleResource{
                false, [&] { release_order.push_back(1); }};
        }));
    assert((release_order == std::vector<int>{1, 2}));
    assert(!invalid_timer.active());
}
