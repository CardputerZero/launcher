#include "cp0_status_component_contract.hpp"

#include <cassert>
#include <stdexcept>

int main()
{
    bool lifecycle_active = true;
    bool top_bar_attached = true;

    cp0::status::release_before_destroy(
        [&] {
            lifecycle_active = false;
            throw std::runtime_error("lifecycle mutex");
        },
        [&] { top_bar_attached = false; });

    assert(!lifecycle_active);
    assert(!top_bar_attached);
}
