#include <string.h>
#include "battery.h"
#include "thpool.h"

extern threadpool g_launch_thread_pool;

static void _battery_timer_cb(int *workingp)
{
    lv_battery_event_data_t data;
    memset(&data, 0, sizeof(data));
    data.info = hal_battery_read();
    lv_lock();
    lv_obj_t *root = lv_screen_active();
    if(root)
    {
        lv_obj_send_event(root, (lv_event_code_t)LV_EVENT_BATTERY, &data);
    }
    lv_unlock();
    *workingp = 0;
}


void battery_timer_cb(lv_timer_t *timer)
{
    static int working = 0;
    if(working)
        return;
    working = 1;
    thpool_add_work(g_launch_thread_pool, (void (*)(void *))_battery_timer_cb, &working);
}