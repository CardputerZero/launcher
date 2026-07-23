#include "cp0_init_once.hpp"

#include <atomic>
#include <cassert>
#include <future>
#include <thread>
#include <vector>

int main()
{
    cp0::InitOnce retryable;
    int attempts = 0;
    assert(!retryable.run([&] { ++attempts; return false; }));
    assert(retryable.run([&] { ++attempts; return true; }));
    assert(retryable.run([&] { ++attempts; return true; }));
    assert(attempts == 2);

    cp0::InitOnce asynchronous;
    std::atomic<int> asynchronous_attempts{0};
    auto asynchronous_result = [&](bool success) {
        return asynchronous.run([&] {
            ++asynchronous_attempts;
            std::promise<bool> ready;
            auto readiness = ready.get_future();
            std::thread([ready = std::move(ready), success]() mutable {
                ready.set_value(success);
            }).detach();
            return readiness.get();
        });
    };
    assert(!asynchronous_result(false));
    assert(asynchronous_result(true));
    assert(asynchronous_result(true));
    assert(asynchronous_attempts.load() == 2);

    cp0::InitOnce concurrent;
    std::atomic<int> initializations{0};
    std::vector<std::thread> threads;
    for (int index = 0; index < 16; ++index) {
        threads.emplace_back([&] {
            assert(concurrent.run([&] { ++initializations; return true; }));
        });
    }
    for (auto &thread : threads) thread.join();
    assert(initializations == 1);

    cp0::InitOnce concurrent_retry;
    std::atomic<int> retry_attempts{0};
    std::atomic<int> successful_callers{0};
    threads.clear();
    for (int index = 0; index < 16; ++index) {
        threads.emplace_back([&] {
            if (concurrent_retry.run([&] {
                    const bool success = retry_attempts.fetch_add(1) != 0;
                    std::promise<bool> ready;
                    auto readiness = ready.get_future();
                    std::thread([ready = std::move(ready), success]() mutable {
                        ready.set_value(success);
                    }).detach();
                    return readiness.get();
                })) {
                ++successful_callers;
            }
        });
    }
    for (auto &thread : threads) thread.join();
    assert(retry_attempts.load() == 2);
    assert(successful_callers.load() == 15);
}
