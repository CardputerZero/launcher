#include "cp0_status_component_contract.hpp"

#include <cassert>
#include <stdexcept>

int main()
{
    int rollbacks = 0;
    assert(cp0::status::initialize_with_rollback(
        [] {}, [] { return true; }, [&] { ++rollbacks; }));
    assert(rollbacks == 0);

    assert(!cp0::status::initialize_with_rollback(
        [] {}, [] { return false; }, [&] { ++rollbacks; }));
    assert(rollbacks == 1);

    assert(!cp0::status::initialize_with_rollback(
        [] { throw std::runtime_error("on_create"); },
        [] { return true; }, [&] { ++rollbacks; }));
    assert(rollbacks == 2);

    assert(!cp0::status::initialize_with_rollback(
        [] {}, []() -> bool { throw std::runtime_error("ready"); },
        [&] { ++rollbacks; }));
    assert(rollbacks == 3);

    assert(!cp0::status::initialize_with_rollback(
        [] {}, [] { return false; },
        [] { throw std::runtime_error("rollback"); }));
}
