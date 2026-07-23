#include "cp0_status_component_contract.hpp"

#include <cassert>
#include <stdexcept>

int main()
{
    bool first_attached = true;
    bool second_attached = true;

    cp0::status::release_before_destroy(
        [] { throw std::runtime_error("first deactivate"); },
        [&] { first_attached = false; });
    cp0::status::release_before_destroy(
        [] {}, [&] { second_attached = false; });

    assert(!first_attached);
    assert(!second_attached);
}
