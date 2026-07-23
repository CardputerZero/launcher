#pragma once

#include "cp0_lvgl_app.h"
#include "lvgl/lvgl.h"
#include "model/app_directory_watcher_contract.hpp"

#include <functional>
#include <mutex>

class AppDirectoryWatcher
{
public:
    ~AppDirectoryWatcher();

    void start(std::function<void()> on_changed);
    void stop() noexcept;

private:
    void poll(lv_timer_t *source_timer);
    static void timer_cb(lv_timer_t *timer) noexcept;

    cp0_watcher_t watcher_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    AppDirectoryChangeCallbackSlot on_changed_;
    std::recursive_mutex mutex_;
};
