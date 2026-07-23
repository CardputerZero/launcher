#include "cp0_sync_signal.hpp"

#include <cassert>
#include <stdexcept>

int main()
{
    int calls = 0;
    assert(cp0::signal::invoke_noexcept([&] { ++calls; }));
    assert(calls == 1);
    assert(!cp0::signal::invoke_noexcept([&] {
        ++calls;
        throw std::runtime_error("signal");
    }));
    assert(calls == 2);
}
