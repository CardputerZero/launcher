#include "cp0_dispatch_shutdown_gate.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

int main()
{
    cp0::dispatch::ShutdownGate gate;
    const uint64_t first_generation = gate.generation();

    auto failed_enqueue = gate.reserve();
    assert(failed_enqueue && gate.pending() == 1);
    failed_enqueue->release();
    assert(gate.pending() == 0);

    auto old_callback = gate.reserve();
    assert(old_callback);
    assert(gate.begin_shutdown() == first_generation + 1);
    assert(!gate.accepting());
    assert(!gate.reserve());
    {
        auto lease = gate.acquire(std::move(*old_callback));
        assert(!lease);
        assert(gate.pending() == 1);
    }
    gate.wait_for_drain();
    assert(gate.pending() == 0);

    gate.resume();
    auto current_callback = gate.reserve();
    assert(current_callback);
    {
        auto lease = gate.acquire(std::move(*current_callback));
        assert(lease);
    }
    assert(gate.pending() == 0);

    constexpr int kCallbacks = 32;
    std::vector<cp0::dispatch::ShutdownGate::Reservation> reservations;
    reservations.reserve(kCallbacks);
    for (int index = 0; index < kCallbacks; ++index) {
        auto reservation = gate.reserve();
        assert(reservation);
        reservations.push_back(std::move(*reservation));
    }

    std::atomic<bool> release_callbacks{false};
    std::atomic<int> acquired{0};
    std::vector<std::thread> workers;
    for (auto &reservation : reservations) {
        workers.emplace_back([&gate, &release_callbacks, &acquired,
                              reservation = std::move(reservation)]() mutable {
            auto lease = gate.acquire(std::move(reservation));
            if (lease) ++acquired;
            while (!release_callbacks.load()) std::this_thread::yield();
        });
    }
    while (acquired.load() != kCallbacks) std::this_thread::yield();

    std::atomic<bool> drained{false};
    std::thread shutdown([&] {
        gate.begin_shutdown();
        gate.wait_for_drain();
        drained.store(true);
    });
    while (gate.accepting()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(!drained.load());
    assert(!gate.reserve());

    release_callbacks.store(true);
    for (auto &worker : workers) worker.join();
    shutdown.join();
    assert(drained.load());
    assert(gate.pending() == 0);
}
