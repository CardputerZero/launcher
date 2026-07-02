#include "ui/ui.h"
#include "lvgl/lvgl.h"
#include "hal_lvgl_bsp.h"
#include <semaphore.h>
#include <time.h>

static sem_t lvgl_sem;

static void lvgl_resume_cb(void *data)
{
    (void)data;
    sem_post(&lvgl_sem);
}

int main(void)
{
    sem_init(&lvgl_sem, 0, 0);
    lv_init();
    lv_timer_handler_set_resume_cb(lvgl_resume_cb, NULL);
    cp0_lvgl_init();
    ui_init();

    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);

    while (1)
    {
        uint32_t ms = lv_timer_handler();
        if (ms == LV_NO_TIMER_READY)
        {
            sem_wait(&lvgl_sem);
        }
        else
        {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += (ms % 1000) * 1000000;
            ts.tv_sec += ms / 1000 + ts.tv_nsec / 1000000000;
            ts.tv_nsec %= 1000000000;
            sem_timedwait(&lvgl_sem, &ts);
        }
    }
}
