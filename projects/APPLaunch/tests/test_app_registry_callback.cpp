#include "../main/ui/model/app_registry_callback.hpp"
#include "../main/ui/model/app_directory_watcher_contract.hpp"
#include "../main/ui/model/app_registry_descriptor_contract.hpp"
#include "../main/ui/app_registry.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <limits>
#include <string>
#include <thread>

namespace {

struct BlockingCallback {
    AppRegistryCallbackSlot *slot;
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    int calls = 0;
};

void block(void *user_data)
{
    auto &state = *static_cast<BlockingCallback *>(user_data);
    state.entered = true;
    while (!state.release) std::this_thread::yield();
    ++state.calls;
}

void unregister_self(void *user_data)
{
    auto *slot = static_cast<AppRegistryCallbackSlot *>(user_data);
    slot->set(nullptr, nullptr);
}

struct ReplacementState {
    AppRegistryCallbackSlot *slot;
    int old_calls = 0;
    int new_calls = 0;
};

void replacement_callback(void *user_data)
{
    ++static_cast<ReplacementState *>(user_data)->new_calls;
}

void count_int(void *user_data)
{
    ++*static_cast<int *>(user_data);
}

void replace_self(void *user_data)
{
    auto &state = *static_cast<ReplacementState *>(user_data);
    ++state.old_calls;
    state.slot->set(replacement_callback, &state);
}

} // namespace

int main()
{
    const AppDescriptor unique_descriptors[] = {
        {"One", "one.png", "app_One", true, false},
        {"Two", "two.png", "app_Two", true, false},
    };
    const AppDescriptor duplicate_descriptors[] = {
        {"One", "one.png", "app_Duplicate", true, false},
        {"Two", "two.png", "app_Duplicate", true, false},
    };
    assert(app_registry_descriptor_ids_unique(unique_descriptors, 2));
    assert(!app_registry_descriptor_ids_unique(duplicate_descriptors, 2));
    assert(!app_registry_descriptor_ids_unique<AppDescriptor>(nullptr, 1));

    uintptr_t handle = 0;
    assert(parse_app_watcher_handle("0x1234", handle) && handle == 0x1234);
    assert(parse_app_watcher_handle("0Xffff", handle) && handle == 0xffff);
    assert(parse_app_watcher_handle("1", handle) && handle == 1);
    assert(!parse_app_watcher_handle("", handle));
    assert(!parse_app_watcher_handle("0", handle));
    handle = 77;
    assert(!parse_app_watcher_handle("0x", handle) && handle == 77);
    assert(!parse_app_watcher_handle("0x0", handle) && handle == 77);
    assert(!parse_app_watcher_handle("+1", handle) && handle == 77);
    assert(!parse_app_watcher_handle(" 1", handle) && handle == 77);
    assert(!parse_app_watcher_handle("12garbage", handle));
    assert(handle == 77);
    assert(!parse_app_watcher_handle("184467440737095516160", handle));
    assert(handle == 77);
    int changes = -1;
    assert(parse_app_watcher_change_count("0", changes) && changes == 0);
    assert(parse_app_watcher_change_count("17", changes) && changes == 17);
    assert(parse_app_watcher_change_count(
        std::to_string(std::numeric_limits<int>::max()), changes));
    assert(changes == std::numeric_limits<int>::max());
    changes = 23;
    assert(!parse_app_watcher_change_count("-1", changes));
    assert(changes == 23);
    assert(!parse_app_watcher_change_count("+1", changes));
    assert(changes == 23);
    assert(!parse_app_watcher_change_count("1 ", changes));
    assert(changes == 23);
    assert(!parse_app_watcher_change_count("2147483648", changes));
    assert(changes == 23);
    assert(!parse_app_watcher_change_count("changed", changes));
    assert(changes == 23);
    int timer = 0;
    int stale_timer = 0;
    assert(app_watcher_timer_is_current(&timer, &timer));
    assert(!app_watcher_timer_is_current(&stale_timer, &timer));
    assert(!app_watcher_timer_is_current(nullptr, &timer));

    AppDirectoryChangeCallbackSlot directory_callback;
    int directory_calls = 0;
    directory_callback.set([&]() {
        ++directory_calls;
        directory_callback.set(nullptr);
    });
    directory_callback.notify();
    directory_callback.notify();
    assert(directory_calls == 1);

    std::atomic<bool> directory_entered{false};
    std::atomic<bool> directory_release{false};
    directory_callback.set([&]() {
        directory_entered = true;
        while (!directory_release) std::this_thread::yield();
    });
    std::thread directory_notifier([&]() { directory_callback.notify(); });
    while (!directory_entered) std::this_thread::yield();
    std::atomic<bool> directory_cleared{false};
    std::thread directory_clearer([&]() {
        directory_callback.set(nullptr);
        directory_cleared = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(!directory_cleared);
    directory_release = true;
    directory_notifier.join();
    directory_clearer.join();
    assert(directory_cleared);

    AppRegistryCallbackSlot slot;
    BlockingCallback state{&slot};
    slot.set(block, &state);

    std::thread notifier([&slot]() { slot.notify(); });
    while (!state.entered) std::this_thread::yield();

    std::atomic<bool> unregistered{false};
    std::thread unregister_thread([&]() {
        slot.set(nullptr, nullptr);
        unregistered = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(!unregistered);

    state.release = true;
    notifier.join();
    unregister_thread.join();
    assert(unregistered);
    assert(state.calls == 1);
    slot.notify();
    assert(state.calls == 1);

    slot.set(unregister_self, &slot);
    slot.notify();
    slot.notify();

    ReplacementState replacement{&slot};
    slot.set(replace_self, &replacement);
    slot.notify();
    assert(replacement.old_calls == 1 && replacement.new_calls == 0);
    slot.notify();
    assert(replacement.old_calls == 1 && replacement.new_calls == 1);

    int old_owner = 0;
    int new_owner = 0;
    slot.set(count_int, &old_owner);
    slot.set(count_int, &new_owner);
    assert(!slot.clear_if_matches(count_int, &old_owner));
    slot.notify();
    assert(new_owner == 1);
    assert(slot.clear_if_matches(count_int, &new_owner));
    slot.notify();
    assert(new_owner == 1);
}
