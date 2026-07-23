#include "zclaw_http_cancellation.h"

#include <atomic>
#include <cassert>
#include <thread>
#include <utility>

int main()
{
    zclaw::HttpCancellationRegistry registry;
    std::atomic<int> stopped{0};

    {
        zclaw::HttpCancellationRegistration completed =
            registry.register_request([&stopped] { ++stopped; });
        assert(completed.active());
    }
    registry.shutdown();
    assert(stopped == 0);
    registry.shutdown();

    zclaw::HttpCancellationRegistration rejected =
        registry.register_request([&stopped] { ++stopped; });
    assert(!rejected.active());
    assert(registry.shutdown_requested());
    assert(stopped == 0);

    zclaw::HttpCancellationRegistry active_registry;
    auto first = active_registry.register_request([&stopped] { ++stopped; });
    auto second = active_registry.register_request([&stopped] { ++stopped; });
    assert(first.active());
    assert(second.active());
    zclaw::HttpCancellationRegistration moved = std::move(first);
    assert(!first.active());
    assert(moved.active());
    second.reset();
    active_registry.shutdown();
    assert(stopped == 1);
    moved.reset();

    for (int iteration = 0; iteration < 100; ++iteration) {
        zclaw::HttpCancellationRegistry racing_registry;
        std::atomic<int> racing_stops{0};
        auto registration = racing_registry.register_request(
            [&racing_stops] { ++racing_stops; });
        std::thread unregister([registration = std::move(registration)]() mutable {
            registration.reset();
        });
        std::thread shutdown([&racing_registry] { racing_registry.shutdown(); });
        unregister.join();
        shutdown.join();
        assert(racing_stops >= 0);
        assert(racing_stops <= 1);
    }

    return 0;
}
