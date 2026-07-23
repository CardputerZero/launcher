#include "cp0_callback_contract.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

int main()
{
    std::vector<int> actions;
    cp0::callback::invoke_direct([&] { actions.push_back(1); });
    assert((actions == std::vector<int>{1}));

    cp0::callback::invoke_direct([&] {
        actions.push_back(2);
        throw std::runtime_error("timer action");
    });
    assert((actions == std::vector<int>{1, 2}));
}
