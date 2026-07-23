#include "cp0_dispatch_testable.hpp"

#include <cassert>
#include <stdexcept>

int main()
{
    int fallbacks = 0;
    assert(!cp0_testable::run_on_dispatch_failure(
        true, [&] { ++fallbacks; }));
    assert(fallbacks == 0);

    assert(cp0_testable::run_on_dispatch_failure(
        false, [&] { ++fallbacks; }));
    assert(fallbacks == 1);

    assert(cp0_testable::run_on_dispatch_failure(false, [&] {
        ++fallbacks;
        throw std::runtime_error("fallback");
    }));
    assert(fallbacks == 2);
}
