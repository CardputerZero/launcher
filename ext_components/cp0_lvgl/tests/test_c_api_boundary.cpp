#include "cp0_c_api_boundary.hpp"

#include <cassert>
#include <new>

namespace {
struct Operation {
    int operator()() const { return 1; }
};
} // namespace

int main()
{
    static_assert(noexcept(cp0::invoke_c_api_or(0, Operation{})));
    assert(cp0::invoke_c_api_or(-1, [] { return 7; }) == 7);
    assert(cp0::invoke_c_api_or(-1, []() -> int { throw std::bad_alloc(); }) == -1);
    assert(cp0::invoke_c_api_or(-1, []() -> int { throw 1; }) == -1);

    const char *fallback = "";
    assert(cp0::invoke_c_api_or(fallback, []() -> const char * {
               throw std::bad_alloc();
           }) == fallback);
    return 0;
}
