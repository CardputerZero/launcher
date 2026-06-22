#include "lvgl/lvgl.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "hal_lvgl_bsp.h"
#include "keyboard_input.h"
#include "main.h"

int lvgl_main(void)
{
    lv_init();
    cp0_lvgl_init();

    if (LV_EVENT_KEYBOARD == 0)
        LV_EVENT_KEYBOARD = lv_event_register_id();

    lv_display_t *disp = lv_display_get_default();
    if (disp == nullptr) {
        fprintf(stderr, "LaunchWizard: failed to create LVGL display\n");
        return 1;
    }

    printf("LaunchWizard: display %dx%d\n",
           (int)lv_display_get_horizontal_resolution(disp),
           (int)lv_display_get_vertical_resolution(disp));

    ui_init();
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(nullptr);

    while (1) {
        lv_timer_handler();
        usleep(10000);
    }
}
