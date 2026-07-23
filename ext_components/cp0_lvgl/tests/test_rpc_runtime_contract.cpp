#include "cp0_rpc_runtime_contract.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

namespace {
struct ThrowingFuture {
    bool get() { throw std::runtime_error("broken promise"); }
};
}

int main()
{
    cp0::rpc::RuntimeState state;
    assert(state.can_start());
    state.mark_started();
    assert(state.running());
    state.mark_stopping();
    assert(state.stopping());
    assert(!state.can_start());
    state.mark_stopped();

    ThrowingFuture broken;
    assert(!cp0::rpc::await_readiness(broken));
    assert(state.can_start());
    state.mark_started();
    assert(state.running());
    state.mark_stopped();

    std::vector<int> order;
    const auto successful = cp0::rpc::shutdown_runtime(
        [&] { order.push_back(1); },
        [&] { order.push_back(2); },
        [&] { order.push_back(3); },
        [&] { order.push_back(4); });
    assert(successful.context_shutdown && successful.workers_joined());
    assert((order == std::vector<int>{1, 2, 3, 4}));

    order.clear();
    const auto failed = cp0::rpc::shutdown_runtime(
        [&] { order.push_back(1); throw std::runtime_error("stop"); },
        [&] { order.push_back(2); throw std::runtime_error("context"); },
        [&] { order.push_back(3); throw std::runtime_error("subscriber"); },
        [&] { order.push_back(4); throw std::runtime_error("broker"); });
    assert(!failed.context_shutdown);
    assert(!failed.workers_joined());
    assert((order == std::vector<int>{1, 2, 3, 4}));
}
