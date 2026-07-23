#include "app_directory_watcher.h"

#include "launcher_platform.hpp"
#include "model/app_directory_watcher_contract.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

namespace {
constexpr uint32_t WATCH_POLL_PERIOD_MS = 3000;
}

AppDirectoryWatcher::~AppDirectoryWatcher()
{
    stop();
}

void AppDirectoryWatcher::start(std::function<void()> on_changed)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    stop();
    on_changed_.set(std::move(on_changed));
    const std::string path = launcher_platform::path("applications");
    cp0_signal_filesystem_api({"WatchStart", path}, [&](int code, std::string data) {
        uintptr_t handle = 0;
        watcher_ = code == 0 && parse_app_watcher_handle(data, handle)
            ? reinterpret_cast<cp0_watcher_t>(handle) : nullptr;
    });
    if (watcher_) {
        timer_ = lv_timer_create(timer_cb, WATCH_POLL_PERIOD_MS, this);
        if (!timer_) {
            cp0_signal_filesystem_api(
                {"WatchStop", std::to_string(reinterpret_cast<uintptr_t>(watcher_))}, nullptr);
            watcher_ = nullptr;
        }
    }
    if (!watcher_)
        on_changed_.set(nullptr);
}

void AppDirectoryWatcher::stop() noexcept
{
    try {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        on_changed_.set(nullptr);
        lv_timer_t *timer = timer_;
        timer_ = nullptr;
        if (timer) lv_timer_delete(timer);
        cp0_watcher_t watcher = watcher_;
        watcher_ = nullptr;
        if (!watcher) return;
        try {
            cp0_signal_filesystem_api(
                {"WatchStop", std::to_string(reinterpret_cast<uintptr_t>(watcher))}, nullptr);
        } catch (...) {
        }
    } catch (...) {
    }
}

void AppDirectoryWatcher::poll(lv_timer_t *source_timer)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!app_watcher_timer_is_current(source_timer, timer_) || !watcher_) return;
    int changed = 0;
    cp0_signal_filesystem_api(
        {"WatchPoll", std::to_string(reinterpret_cast<uintptr_t>(watcher_))},
        [&](int code, std::string data) {
            int parsed = 0;
            if (code == 0 && parse_app_watcher_change_count(data, parsed)) changed = parsed;
        });
    if (changed > 0) on_changed_.notify();
}

void AppDirectoryWatcher::timer_cb(lv_timer_t *timer) noexcept
{
    auto *watcher = static_cast<AppDirectoryWatcher *>(lv_timer_get_user_data(timer));
    if (!watcher) return;
    try {
        watcher->poll(timer);
    } catch (...) {
        watcher->stop();
    }
}
