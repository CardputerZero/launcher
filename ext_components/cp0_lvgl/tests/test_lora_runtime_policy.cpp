#include "cp0_lora_runtime_policy.hpp"

#include <cassert>

int main()
{
    using namespace cp0_lora_runtime_policy;

    assert(!should_timeout_transmit(false, true, true, 100, 4100));
    assert(!should_timeout_transmit(true, false, true, 100, 4100));
    assert(!should_timeout_transmit(true, true, false, 100, 4100));
    assert(!should_timeout_transmit(true, true, true, 0, 5000));
    assert(!should_timeout_transmit(true, true, true, 100, 4099));
    assert(should_timeout_transmit(true, true, true, 100, 4100));
    assert(!should_timeout_transmit(true, true, true, 4100, 100));

    assert(!should_send_demo(false, true, false, 100, 2100));
    assert(!should_send_demo(true, false, false, 100, 2100));
    assert(!should_send_demo(true, true, true, 100, 2100));
    assert(!should_send_demo(true, true, false, 100, 2099));
    assert(should_send_demo(true, true, false, 100, 2100));
    assert(!should_send_demo(true, true, false, 2100, 100));

    return 0;
}
