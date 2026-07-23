#pragma once

#include "lvgl/lvgl.h"
#include "zclaw_task_inbox.h"
#include "zclaw_ui_task_sink.h"

#include <functional>

namespace zclaw {

class UiTaskQueue : public UiTaskSink {
public:
    UiTaskQueue() = default;
    ~UiTaskQueue();

    UiTaskQueue(const UiTaskQueue &) = delete;
    UiTaskQueue &operator=(const UiTaskQueue &) = delete;

    bool start(uint32_t poll_period_ms = 10);
    bool post(Task task) override;
    void shutdown();
    bool active() const;

private:
    static void timer_callback(lv_timer_t *timer);
    void drain();

    TaskInbox inbox_;
    lv_timer_t *timer_ = nullptr;
};

}  // namespace zclaw
