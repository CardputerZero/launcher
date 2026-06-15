#include "hal_lvgl_bsp.h"
#include "lvgl/lvgl.h"

#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdl_lvgl.h"
#include "commount.h"


void cp0_lvgl_init(void)
{
    init_lvgl_env();
    init_lvgl_event();
    init_filesystem();
    init_config();
    init_pty();
    init_freambuffer_disp();
    init_input();
    init_audio();
    init_process();
    init_osinfo();
    init_screenshot();
    init_lora();
    init_wifi();
    init_settings();
    init_bq27220();
    init_imu();
    init_lvgl_saved_settings();
    init_battery();
    init_camera();
}
