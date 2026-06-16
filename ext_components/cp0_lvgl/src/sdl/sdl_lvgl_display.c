#include "hal_lvgl_bsp.h"
#include "lvgl/lvgl.h"
#include "sdl_lvgl.h"


#include "lvgl/src/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/src/drivers/sdl/lv_sdl_private.h"
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"

#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *getenv_default(const char *name, const char *dflt)
{
    const char *value = getenv(name);
    return (value && value[0] != '\0') ? value : dflt;
}

void init_sdl_disp(void)
{
    static int display_initialized = 0;
    if (display_initialized)
        return;

    int width = atoi(getenv_default("LV_SDL_VIDEO_WIDTH", "320"));
    int height = atoi(getenv_default("LV_SDL_VIDEO_HEIGHT", "170"));
    lv_display_t *disp = lv_sdl_window_create(width, height);
    if (disp == NULL) {
        fprintf(stderr, "cp0_lvgl: failed to create SDL display\n");
        return;
    }

    lv_sdl_window_set_title(disp, getenv_default("LV_SDL_WINDOW_TITLE", "M5CardputerZero"));
    display_initialized = 1;
}

void init_freambuffer_disp(void)
{
    init_sdl_disp();
}
