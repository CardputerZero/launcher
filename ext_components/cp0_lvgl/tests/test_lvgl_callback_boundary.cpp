#include "cp0_callback_contract.hpp"

#include <cassert>
#include <stdexcept>

namespace {

void throwing_event_handler(int *calls)
{
    ++*calls;
    throw std::runtime_error("event handler");
}

} // namespace

int main()
{
    int calls = 0;
    cp0::callback::invoke_direct(throwing_event_handler, &calls);
    assert(calls == 1);

    cp0::callback::invoke_direct([&] { ++calls; });
    assert(calls == 2);

    void (*empty_handler)(int *) = nullptr;
    cp0::callback::invoke_direct(empty_handler, &calls);
    assert(calls == 2);
}
