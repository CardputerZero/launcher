#include "cp0_status_lifecycle.hpp"

#include <atomic>
#include <cassert>
#include <stdexcept>
#include <thread>
#include <vector>

int main()
{
    cp0::status::Lifecycle lifecycle;
    int creates = 0;
    std::vector<int> releases;
    std::vector<cp0::status::Lifecycle::Factory> factories;
    for (int id = 1; id <= 3; ++id) {
        factories.push_back([&, id] {
            ++creates;
            return cp0::status::Resource{
                true, [&, id] { releases.push_back(id); }};
        });
    }
    assert(lifecycle.start(factories));
    assert(lifecycle.start(factories));
    assert(lifecycle.active() && creates == 3);
    lifecycle.stop();
    lifecycle.stop();
    assert(!lifecycle.active());
    assert((releases == std::vector<int>{3, 2, 1}));

    cp0::status::Lifecycle rollback;
    releases.clear();
    assert(!rollback.start({
        [&] { return cp0::status::Resource{true, [&] { releases.push_back(1); }}; },
        [&] { return cp0::status::Resource{true, [&] { releases.push_back(2); }}; },
        [] { return cp0::status::Resource{}; },
    }));
    assert(!rollback.active());
    assert((releases == std::vector<int>{2, 1}));

    cp0::status::Lifecycle throwing_factory;
    releases.clear();
    assert(!throwing_factory.start({
        [&] { return cp0::status::Resource{true, [&] {
            assert(!throwing_factory.active());
            releases.push_back(1);
        }}; },
        []() -> cp0::status::Resource { throw std::runtime_error("factory"); },
    }));
    assert(!throwing_factory.active());
    assert((releases == std::vector<int>{1}));

    cp0::status::Lifecycle throwing_release;
    releases.clear();
    assert(throwing_release.start({
        [&] { return cp0::status::Resource{true, [&] { releases.push_back(1); }}; },
        [] { return cp0::status::Resource{true, [] { throw std::runtime_error("release"); }}; },
        [&] { return cp0::status::Resource{true, [&] { releases.push_back(3); }}; },
    }));
    throwing_release.stop();
    assert(!throwing_release.active());
    assert((releases == std::vector<int>{3, 1}));

    std::atomic<int> concurrent_creates{0};
    cp0::status::Lifecycle concurrent;
    std::vector<std::thread> threads;
    for (int index = 0; index < 12; ++index) {
        threads.emplace_back([&] {
            assert(concurrent.start({[&] {
                ++concurrent_creates;
                return cp0::status::Resource{true, [] {}};
            }}));
        });
    }
    for (auto &thread : threads)
        thread.join();
    assert(concurrent_creates.load() == 1);

    cp0::status::Lifecycle reentrant_factory;
    int reentrant_releases = 0;
    assert(!reentrant_factory.start({[&] {
        assert(!reentrant_factory.active());
        reentrant_factory.stop();
        return cp0::status::Resource{
            true, [&] { ++reentrant_releases; }};
    }}));
    assert(!reentrant_factory.active());
    assert(reentrant_releases == 1);

    cp0::status::Lifecycle externally_deleted;
    int timer_creates = 0;
    int timer_releases = 0;
    auto enable = [&] {
        return externally_deleted.start({[&] {
            ++timer_creates;
            return cp0::status::Resource{
                true, [&] { ++timer_releases; }};
        }});
    };
    assert(enable());
    externally_deleted.stop();
    assert(!externally_deleted.active());
    assert(timer_creates == 1 && timer_releases == 1);
    assert(enable());
    externally_deleted.stop();
    assert(timer_creates == 2 && timer_releases == 2);
}
