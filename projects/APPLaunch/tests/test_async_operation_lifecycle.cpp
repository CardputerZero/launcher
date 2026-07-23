#include "../main/ui/model/async_operation_lifecycle.hpp"

#include <atomic>
#include <cassert>
#include <memory>
#include <thread>
#include <vector>

using setting::AsyncOperationLifecycle;

int main()
{
    AsyncOperationLifecycle lifecycle;
    const auto first = lifecycle.begin();
    assert(first);
    assert(!lifecycle.begin());
    assert(lifecycle.activate(first, 42));
    assert(first.current());
    assert(lifecycle.active());
    assert(first.complete());
    assert(first.current());
    assert(!first.complete());
    assert(!lifecycle.active());

    const auto stale = lifecycle.begin();
    assert(stale);
    assert(lifecycle.abort(stale));
    assert(!stale.current());
    const auto current = lifecycle.begin();
    assert(current);
    assert(!first.current());
    assert(lifecycle.activate(current, 77));
    assert(!stale.complete());
    assert(lifecycle.active());
    assert(current.complete());

    const auto reentrant = lifecycle.begin();
    assert(reentrant.complete());
    assert(reentrant.current());
    assert(!lifecycle.activate(reentrant, 88));
    assert(!lifecycle.active());

    const auto next_generation = lifecycle.begin();
    assert(next_generation);
    assert(!reentrant.current());
    assert(!reentrant.complete());
    assert(next_generation.current());
    assert(next_generation.complete());

    AsyncOperationLifecycle concurrent;
    std::atomic<int> winners{0};
    std::vector<std::thread> threads;
    for (int index = 0; index < 16; ++index) {
        threads.emplace_back([&] {
            const auto token = concurrent.begin();
            if (token)
                ++winners;
        });
    }
    for (auto &thread : threads)
        thread.join();
    assert(winners.load() == 1);
    assert(concurrent.active());

    AsyncOperationLifecycle cancellable;
    const auto cancel_token = cancellable.begin();
    assert(cancellable.activate(cancel_token, 1234));
    assert(cancellable.shutdown() == 1234);
    assert(cancellable.shutdown() == 0);
    assert(!cancel_token.complete());
    assert(!cancel_token.current());
    assert(!cancellable.begin());

    AsyncOperationLifecycle destroyed_before_completion;
    const auto queued_completion = destroyed_before_completion.begin();
    assert(queued_completion);
    destroyed_before_completion.shutdown();
    assert(!queued_completion.current());
    assert(!queued_completion.complete());

    AsyncOperationLifecycle::Token late;
    {
        auto owner = std::make_unique<AsyncOperationLifecycle>();
        late = owner->begin();
        assert(owner->activate(late, 9));
    }
    assert(!late.complete());
    assert(!late.current());
}
