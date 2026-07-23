#include "cp0_runner_contract.hpp"

#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

int main()
{
    cp0::RunnerLifetime lifetime;
    assert(lifetime.claim());
    assert(!lifetime.claim());

    lifetime.initialization_failed();
    assert(lifetime.claim());
    lifetime.finish();
    assert(!lifetime.claim());

    lifetime.initialization_failed();
    assert(!lifetime.claim());

    cp0::RunnerLifetime concurrent;
    std::atomic<int> successful_claims{0};
    std::vector<std::thread> threads;
    for (int index = 0; index < 32; ++index) {
        threads.emplace_back([&] {
            if (concurrent.claim()) ++successful_claims;
        });
    }
    for (auto &thread : threads) thread.join();
    assert(successful_claims.load() == 1);

    concurrent.initialization_failed();
    assert(concurrent.claim());
    concurrent.finish();
    assert(!concurrent.claim());
}
