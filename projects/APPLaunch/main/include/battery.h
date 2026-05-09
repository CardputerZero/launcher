#ifndef __BATTERY__H__
#define __BATTERY__H__

#include "lvgl/lvgl.h"
#include "ui/ui.h"
#ifdef __cplusplus
extern "C" {
#endif
void battery_timer_cb(lv_timer_t *timer);
#ifdef __cplusplus
}
#endif
#endif