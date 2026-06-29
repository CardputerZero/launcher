/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include <unistd.h>
#include <stdio.h>
#include <chrono>
#include <string>
#include <semaphore.h>
#include "ui/ui.h"
#include "ui/ui_darkscreen.h"
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

static const char* lock_file = NULL;
static sem_t lvgl_sem;

static void lvgl_resume_cb(void *data) {
    sem_post(&lvgl_sem);
}

/*
void APPLaunch_lock()
{
    static int home_back_status = 0;
    static std::chrono::time_point<std::chrono::steady_clock> start_time;

    int holder_pid = 0;
    cp0_process_check_lock(lock_file, &holder_pid);

    static int lvgl_lock = 0;
    if (holder_pid == 0) {
        if (lvgl_lock == 1) {
            LVGL_RUN_FLAGE = 1;
            lvgl_lock = 0;
            lv_obj_invalidate(lv_scr_act());
        }
    } else {
        if (LVGL_HOME_KEY_FLAG) {
            if (home_back_status == 0) {
                home_back_status = 1;
                start_time = std::chrono::steady_clock::now();
            }
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            if (secs >= 5) {
                cp0_process_kill(holder_pid, 3000);
                home_back_status = 0;
            }
        } else {
            home_back_status = 0;
        }
        lvgl_lock = 1;
        LVGL_RUN_FLAGE = 0;
    }
}
*/

int main(void)
{
    sem_init(&lvgl_sem, 0, 0);
    static const std::string default_lock_file = cp0_file_path("lock_file");
    lock_file = default_lock_file.c_str();
    lv_init();
    SLOGI("[BOOT] lv_init() done");
    lv_timer_handler_set_resume_cb(lvgl_resume_cb, NULL);

    SLOGI("[BOOT] cp0_lvgl_init() starting...");
    cp0_lvgl_init();
    SLOGI("[BOOT] cp0_lvgl_init() done");

    if (LV_EVENT_KEYBOARD == 0)
        LV_EVENT_KEYBOARD = lv_event_register_id();

    launcher_ui::init();

    // Force full-screen refresh immediately after init
    SLOGI("[BOOT] ui_init done, forcing full refresh...");
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    SLOGI("[BOOT] First frame flushed to fb0.");

    /*Handle LVGL tasks*/
    SLOGI("Entering main loop (FULL render mode)...");
    // while(1) {
    //     // APPLaunch_lock();
    //     // ui_darkscreen_tick();
    //     lv_timer_handler();
    //     usleep(5000);
    // }
    while (1) {
        uint32_t ms = lv_timer_handler();
        if (ms == LV_NO_TIMER_READY) {
            sem_wait(&lvgl_sem);      // 无定时器，阻塞等事件
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += (ms % 1000) * 1000000;
            ts.tv_sec += ms / 1000 + ts.tv_nsec / 1000000000;
            ts.tv_nsec %= 1000000000;
            sem_timedwait(&lvgl_sem, &ts);  // 定时唤醒 或 被事件提前唤醒
        }
    }

    return 0;
}
