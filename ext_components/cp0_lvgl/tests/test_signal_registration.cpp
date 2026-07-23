#include "cp0_signal_registration.hpp"

#include "eventpp/callbacklist.h"

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <algorithm>
#include <thread>
#include <vector>

namespace {

class FailingList {
public:
    using Callback = std::function<void()>;

    class Handle {
    public:
        Handle() = default;
        explicit Handle(std::weak_ptr<int> token) : token_(std::move(token)) {}
        explicit operator bool() const { return !token_.expired(); }
        std::weak_ptr<int> token() const { return token_; }

    private:
        std::weak_ptr<int> token_;
    };

    Handle append(const Callback &)
    {
        if (fail_next) {
            fail_next = false;
            return {};
        }
        token = std::make_shared<int>(1);
        tokens.push_back(token);
        return Handle(token);
    }

    bool remove(const Handle &handle)
    {
        const auto target = handle.token().lock();
        const auto found = std::find(tokens.begin(), tokens.end(), target);
        if (!target || found == tokens.end())
            return false;
        tokens.erase(found);
        if (on_remove)
            on_remove();
        return true;
    }

    std::size_t active_count() const { return tokens.size(); }

    bool fail_next = false;
    Callback on_remove;
    std::shared_ptr<int> token;
    std::vector<std::shared_ptr<int>> tokens;
};

} // namespace

int main()
{
    using List = eventpp::CallbackList<void()>;
    using Registration = cp0::SignalRegistration<List>;

    List signal;
    int first_calls = 0;
    int second_calls = 0;
    auto first_instance = std::make_shared<int>(1);
    std::weak_ptr<int> old_instance = first_instance;
    {
        Registration registration;
        assert(!registration.registered());
        assert(registration.replace(signal, [instance = std::move(first_instance), &first_calls] {
            assert(*instance == 1);
            ++first_calls;
        }));
        signal();
        assert(first_calls == 1);
        assert(registration.replace(signal, [&] { ++second_calls; }));
        assert(old_instance.expired());
        signal();
        assert(first_calls == 1 && second_calls == 1);

        Registration moved(std::move(registration));
        assert(!registration.registered() && moved.registered());
        Registration assigned;
        int discarded_calls = 0;
        assert(assigned.replace(signal, [&] { ++discarded_calls; }));
        assigned = std::move(moved);
        assert(!moved.registered() && assigned.registered());
        signal();
        assert(second_calls == 2 && discarded_calls == 0);
        assigned.reset();
        assigned.reset();
        assert(!assigned.registered());
    }
    signal();
    assert(second_calls == 2);

    FailingList failing;
    cp0::SignalRegistration<FailingList> failure_registration;
    assert(failure_registration.replace(failing, [] {}));
    failing.fail_next = true;
    assert(!failure_registration.replace(failing, [] {}));
    assert(failure_registration.registered());
    bool remove_callback_ran = false;
    failing.on_remove = [&] {
        (void)failure_registration.registered();
        remove_callback_ran = true;
    };
    failure_registration.reset();
    assert(remove_callback_ran);
    assert(!failure_registration.registered());

    List concurrent_signal;
    Registration concurrent;
    std::atomic<int> callback_calls{0};
    std::vector<std::thread> threads;
    for (int index = 0; index < 16; ++index) {
        threads.emplace_back([&] {
            assert(concurrent.replace(concurrent_signal, [&] { ++callback_calls; }));
        });
    }
    for (auto &thread : threads)
        thread.join();
    concurrent_signal();
    assert(callback_calls.load() == 1);

    using Bundle = cp0::SignalRegistrationBundle<List, List>;
    List first_signal;
    List second_signal;
    int old_first_calls = 0;
    int old_second_calls = 0;
    int new_first_calls = 0;
    int new_second_calls = 0;
    {
        Bundle bundle;
        assert(bundle.replace(cp0::bind_signal(first_signal, [&] { ++old_first_calls; }),
                              cp0::bind_signal(second_signal, [&] { ++old_second_calls; })));
        assert(bundle.registered());
        first_signal();
        second_signal();
        assert(old_first_calls == 1 && old_second_calls == 1);

        assert(bundle.replace(cp0::bind_signal(first_signal, [&] { ++new_first_calls; }),
                              cp0::bind_signal(second_signal, [&] { ++new_second_calls; })));
        first_signal();
        second_signal();
        assert(old_first_calls == 1 && old_second_calls == 1);
        assert(new_first_calls == 1 && new_second_calls == 1);
    }
    first_signal();
    second_signal();
    assert(new_first_calls == 1 && new_second_calls == 1);

    FailingList first_failing;
    FailingList second_failing;
    cp0::SignalRegistrationBundle<FailingList, FailingList> failing_bundle;
    assert(failing_bundle.replace(cp0::bind_signal(first_failing, [] {}),
                                  cp0::bind_signal(second_failing, [] {})));
    assert(first_failing.active_count() == 1 && second_failing.active_count() == 1);
    second_failing.fail_next = true;
    assert(!failing_bundle.replace(cp0::bind_signal(first_failing, [] {}),
                                   cp0::bind_signal(second_failing, [] {})));
    assert(failing_bundle.registered());
    assert(first_failing.active_count() == 1 && second_failing.active_count() == 1);
    failing_bundle.reset();
    assert(first_failing.active_count() == 0 && second_failing.active_count() == 0);

    FailingList null_binding_first;
    FailingList null_binding_second;
    cp0::SignalRegistrationBundle<FailingList, FailingList> null_binding_bundle;
    int retained_calls = 0;
    assert(null_binding_bundle.replace(
        cp0::bind_signal(null_binding_first, [&] { ++retained_calls; }),
        cp0::bind_signal(null_binding_second, [&] { ++retained_calls; })));
    cp0::SignalBinding<FailingList> missing_owner{
        nullptr, FailingList::Callback([] {})};
    assert(!null_binding_bundle.replace(
        std::move(missing_owner),
        cp0::bind_signal(null_binding_second, [] {})));
    assert(null_binding_bundle.registered());
    assert(null_binding_first.active_count() == 1);
    assert(null_binding_second.active_count() == 1);
    null_binding_bundle.reset();
    assert(null_binding_first.active_count() == 0);
    assert(null_binding_second.active_count() == 0);

    List concurrent_first;
    List concurrent_second;
    Bundle concurrent_bundle;
    std::atomic<int> concurrent_first_calls{0};
    std::atomic<int> concurrent_second_calls{0};
    threads.clear();
    for (int index = 0; index < 16; ++index) {
        threads.emplace_back([&] {
            assert(concurrent_bundle.replace(
                cp0::bind_signal(concurrent_first, [&] { ++concurrent_first_calls; }),
                cp0::bind_signal(concurrent_second, [&] { ++concurrent_second_calls; })));
        });
    }
    for (auto &thread : threads)
        thread.join();
    concurrent_first();
    concurrent_second();
    assert(concurrent_first_calls.load() == 1);
    assert(concurrent_second_calls.load() == 1);
}
