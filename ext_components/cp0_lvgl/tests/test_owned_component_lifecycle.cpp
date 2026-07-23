#include "cp0_status_component_contract.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

int main()
{
    std::vector<int> order;
    cp0::status::release_before_destroy(
        [&] { order.push_back(1); },
        [&] { order.push_back(2); });
    assert((order == std::vector<int>{1, 2}));

    order.clear();
    cp0::status::release_before_destroy(
        [&] {
            order.push_back(1);
            throw std::runtime_error("deactivate");
        },
        [&] { order.push_back(2); });
    assert((order == std::vector<int>{1, 2}));

    order.clear();
    cp0::status::release_before_destroy(
        [&] { order.push_back(1); },
        [&] {
            order.push_back(2);
            throw std::runtime_error("unmount");
        });
    assert((order == std::vector<int>{1, 2}));
}
