#include "cp0_battery_runtime.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

int main()
{
    std::vector<int> order;
    cp0::battery::shutdown_runtime(
        [&] { order.push_back(1); },
        [&] { order.push_back(2); });
    assert((order == std::vector<int>{1, 2}));

    order.clear();
    cp0::battery::shutdown_runtime(
        [&] {
            order.push_back(1);
            throw std::runtime_error("stop battery");
        },
        [&] { order.push_back(2); });
    assert((order == std::vector<int>{1, 2}));

    order.clear();
    cp0::battery::shutdown_runtime(
        [&] { order.push_back(1); },
        [&] {
            order.push_back(2);
            throw std::runtime_error("deinit LVGL");
        });
    assert((order == std::vector<int>{1, 2}));
}
