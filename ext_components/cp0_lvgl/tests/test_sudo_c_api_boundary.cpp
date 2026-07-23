#include "cp0_sudo_c_api_boundary.hpp"

#include <cassert>
#include <cerrno>
#include <new>

struct Operation {
    int operator()() const { return 0; }
};

int main()
{
    static_assert(noexcept(cp0_sudo::invoke_c_api(Operation{})));
    assert(cp0_sudo::invoke_c_api([] { return 17; }) == 17);
    assert(cp0_sudo::invoke_c_api([]() -> int { throw std::bad_alloc(); }) == -ENOMEM);
    assert(cp0_sudo::invoke_c_api([]() -> int { throw 1; }) == -EIO);
}
