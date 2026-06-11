#include "hal_lvgl_bsp.h"
#include "lvgl/lvgl.h"
#include "commount.h"
#include "cp0_lvgl.h"
void cp0_lvgl_init(void)
{
    init_lvgl_env();
    init_lvgl_event();
    init_freambuffer_disp();
    init_input();
    init_audio();
    init_lvgl_saved_settings();
    init_battery();
    init_camera();
}
