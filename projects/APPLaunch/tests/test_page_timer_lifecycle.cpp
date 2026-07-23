#include "../main/ui/model/page_timer_lifecycle.hpp"

#include <atomic>
#include <cassert>
#include <stdexcept>
#include <thread>
#include <vector>

int main()
{
    using Lifecycle = PageTimerLifecycle<int *>;
    int timer = 1;
    int deleted = 0;
    int creates = 0;
    Lifecycle lifecycle;
    assert(!lifecycle.start([] { return static_cast<int *>(nullptr); },
                            [&](int *) { ++deleted; }));
    assert(!lifecycle.active());
    assert(lifecycle.start([&] { ++creates; return &timer; },
                           [&](int *) { ++deleted; }));
    assert(lifecycle.active());
    lifecycle.stop();

    creates = 0;
    deleted = 0;
    assert(lifecycle.start([&] { ++creates; return &timer; },
                           [&](int *handle) {
                               assert(handle == &timer);
                               assert(!lifecycle.current(handle));
                               assert(!lifecycle.active());
                               lifecycle.stop();
                               ++deleted;
                           }));
    assert(lifecycle.start([&] { ++creates; return &timer; }, [&](int *) { ++deleted; }));
    assert(creates == 1);
    assert(lifecycle.current(&timer));
    lifecycle.stop();
    lifecycle.stop();
    assert(deleted == 1);

    int destructor_deletes = 0;
    {
        Lifecycle scoped;
        assert(scoped.start([&] { return &timer; },
                            [&](int *) { ++destructor_deletes; }));
    }
    assert(destructor_deletes == 1);

    int throwing_deletes = 0;
    assert(lifecycle.start([&] { return &timer; }, [&](int *) {
        ++throwing_deletes;
        throw std::runtime_error("timer delete failed");
    }));
    static_assert(noexcept(lifecycle.stop()));
    lifecycle.stop();
    assert(throwing_deletes == 1);
    assert(!lifecycle.active());
    assert(lifecycle.start([&] { return &timer; }, [](int *) {}));
    lifecycle.stop();

    {
        Lifecycle scoped;
        assert(scoped.start([&] { return &timer; }, [&](int *) {
            ++throwing_deletes;
            throw std::runtime_error("destructor delete failed");
        }));
    }
    assert(throwing_deletes == 2);

    Lifecycle concurrent;
    std::atomic<int> concurrent_creates{0};
    std::vector<std::thread> threads;
    for (int index = 0; index < 16; ++index) {
        threads.emplace_back([&] {
            assert(concurrent.start([&] {
                ++concurrent_creates;
                return &timer;
            }, [](int *) {}));
        });
    }
    for (auto &thread : threads)
        thread.join();
    assert(concurrent_creates.load() == 1);

    Lifecycle poll;
    Lifecycle cursor;
    int poll_timer = 2;
    assert(poll.start([&] { return &poll_timer; }, [](int *) {}));
    assert(!cursor.start([] { return static_cast<int *>(nullptr); }, [](int *) {}));
    assert(poll.active());
    assert(!cursor.active());
    poll.stop();
    cursor.stop();
}
