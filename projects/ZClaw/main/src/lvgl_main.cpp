#include "lvgl/lvgl.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "hal_lvgl_bsp.h"
#include "main.h"

int lvgl_main(void)
{
    lv_init();
    cp0_lvgl_init();
    lv_display_t *disp = lv_display_get_default();
    if (disp == nullptr) {
        fprintf(stderr, "HelloWorld: failed to create LVGL display\n");
        return 1;
    }

    printf("HelloWorld: display %dx%d\n",
           (int)lv_display_get_horizontal_resolution(disp),
           (int)lv_display_get_vertical_resolution(disp));

    ui_init();
    lv_obj_invalidate(lv_screen_active());

    while (1) {
        lv_timer_handler();
        usleep(10000);
    }
}
