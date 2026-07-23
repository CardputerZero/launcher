#include "cp0_status_component_contract.hpp"

#include <cassert>
#include <stdexcept>

int main()
{
    bool active = true;
    bool attached = true;
    cp0::status::release_before_destroy(
        [&] { active = false; },
        [&] { attached = false; });
    assert(!active && !attached);

    active = true;
    attached = true;
    cp0::status::release_before_destroy(
        [&] {
            active = false;
            throw std::runtime_error("deactivate");
        },
        [&] { attached = false; });
    assert(!active && !attached);

    bool retried_mount = !attached;
    assert(retried_mount);
}
