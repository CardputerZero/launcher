#include "hal_lvgl_bsp.h"
#include "commount.h"
#include "lvgl/lvgl.h"
uint32_t LV_C_EVENT_KEYBOARD;
uint32_t LV_C_EVENT_BATTERY;
uint32_t LV_C_EVENT_NETWORK;
uint32_t LV_C_EVENT_DATATIME;
void init_lvgl_event()
{
    LV_C_EVENT_KEYBOARD = lv_event_register_id();
    LV_C_EVENT_BATTERY = lv_event_register_id();
    LV_C_EVENT_NETWORK = lv_event_register_id();
    LV_C_EVENT_DATATIME = lv_event_register_id();
}
