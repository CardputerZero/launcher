#include "hal_lvgl_bsp.h"
#include "commount.h"
#include "lvgl/lvgl.h"

uint32_t lv_c_event[(2*CP0_C_EVENT_END)];
uint32_t cp0_event[(2*CP0_C_EVENT_END)];

void init_lvgl_event()
{
    for (int i = 0; i < CP0_C_EVENT_END; i++)
        lv_c_event[i] = lv_event_register_id();
}
