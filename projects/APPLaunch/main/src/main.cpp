#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include <stdio.h>
#include <errno.h>
#include <chrono>
#include <string>
#include <semaphore.h>
#include "ui/ui.h"
#include "ui/ui_screensaver.h"
#include "keyboard_input.h"
#include "cp0_lvgl_app.h"
#include "cp0_lvgl_file.hpp"
#include "hal_lvgl_bsp.h"
#include "global_config.h"
#include "sample_log.h"
#if CONFIG_BACKWARD_CPP_ENABLED
#define BACKWARD_HAS_DW 1
#include "backward.hpp"
#include "backward.h"
#endif

static sem_t lvgl_sem;

static void lvgl_wait(uint32_t milliseconds)
{
    struct timespec deadline;
#if defined(__linux__)
    clock_gettime(CLOCK_MONOTONIC, &deadline);
#else
    clock_gettime(CLOCK_REALTIME, &deadline);
#endif
    deadline.tv_nsec += (milliseconds % 1000) * 1000000;
    deadline.tv_sec += milliseconds / 1000 + deadline.tv_nsec / 1000000000;
    deadline.tv_nsec %= 1000000000;

    int result;
    do {
#if defined(__linux__)
        result = sem_clockwait(&lvgl_sem, CLOCK_MONOTONIC, &deadline);
#else
        result = sem_timedwait(&lvgl_sem, &deadline);
#endif
    } while (result != 0 && errno == EINTR);
}

static void lvgl_resume_cb(void *data) {
    sem_post(&lvgl_sem);
}

int main(void)
{
    sem_init(&lvgl_sem, 0, 0);
    lv_init();
    SLOGI("[BOOT] lv_init() done");
    lv_timer_handler_set_resume_cb(lvgl_resume_cb, NULL);

    SLOGI("[BOOT] cp0_lvgl_init() starting...");
    cp0_lvgl_init();
    SLOGI("[BOOT] cp0_lvgl_init() done");

    if (LV_EVENT_KEYBOARD == 0)
        LV_EVENT_KEYBOARD = lv_event_register_id();

    launcher_ui::init();
    ui_screensaver_init();

    // Force full-screen refresh immediately after init
    SLOGI("[BOOT] ui_init done, forcing full refresh...");
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    SLOGI("[BOOT] First frame flushed to fb0.");

    /*Handle LVGL tasks*/
    SLOGI("Entering main loop (FULL render mode)...");
    while (1) {
        uint32_t ms = lv_timer_handler();
        if (ms == LV_NO_TIMER_READY) {
            sem_wait(&lvgl_sem);      // 无定时器，阻塞等事件
        } else {
            lvgl_wait(ms);  // 定时唤醒或被事件提前唤醒，不受系统时间调整影响
        }
    }

    return 0;
}
