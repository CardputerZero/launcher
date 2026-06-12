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
    init_lvgl_event();
    init_sdl_disp();
    init_sdl_input();
    init_audio();
    init_battery();
    init_camera();
}
