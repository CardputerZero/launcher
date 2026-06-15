#include "lvgl/lvgl.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui/ui.h"
#include "main.h"
#include <cstring>

static const char *getenv_default(const char *name, const char *dflt)
{
    return getenv(name) ?: dflt;
}

int get_st7789v_fbdev(char *dev_path, size_t buf_size)
{
    if (dev_path == NULL || buf_size == 0)
    {
        return -1;
    }

    FILE *fp = fopen("/proc/fb", "r");
    if (fp == NULL)
    {
        perror("Failed to open /proc/fb");
        return -1;
    }

    char line[256];
    int fb_num = -1;

    /* Read /proc/fb line by line and find the fb_st7789v entry, e.g. "0 fb_st7789v". */
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (strstr(line, "fb_st7789v") != NULL)
        {
            if (sscanf(line, "%d", &fb_num) == 1)
            {
                break;
            }
        }
    }

    fclose(fp);

    if (fb_num < 0)
    {
        fprintf(stderr, "fb_st7789v not found in /proc/fb\n");
        return -1;
    }

    snprintf(dev_path, buf_size, "/dev/fb%d", fb_num);
    return 0;
}


#if LV_USE_EVDEV

static void lv_linux_indev_init(void)
{
    const char *mouse_device = getenv_default("LV_LINUX_MOUSE_DEVICE", NULL);

    lv_indev_t *touch = NULL;
    if (mouse_device)
        touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, mouse_device);
}
#endif

#if LV_USE_LINUX_FBDEV
static void lv_linux_disp_init(void)
{
    const char *device = NULL;
    char fbdev[64] = {0};
    device = getenv_default("LV_LINUX_FBDEV_DEVICE", NULL);
    if ((device == NULL) && (get_st7789v_fbdev(fbdev, sizeof(fbdev)) == 0))
    {
        device = fbdev;
    }
    printf("Using framebuffer device: %s\n", device);
    lv_display_t *disp = lv_linux_fbdev_create();
    if (disp == NULL)
    {
        printf("Failed to create fbdev display!\n");
        return;
    }

    lv_linux_fbdev_set_file(disp, device);

    lv_coord_t w = lv_display_get_horizontal_resolution(disp);
    lv_coord_t h = lv_display_get_vertical_resolution(disp);
    printf("Framebuffer resolution: %dx%d\n", w, h);
}
#if !LV_USE_EVDEV && !LV_USE_LIBINPUT
static void lv_linux_indev_init(void)
{
}
#endif

#elif LV_USE_LINUX_DRM
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_DRM_CARD", "/dev/dri/card0");
    lv_display_t *disp = lv_linux_drm_create();

    lv_linux_drm_set_file(disp, device, -1);
}
#elif LV_USE_SDL
static void lv_linux_disp_init(void)
{
    const int width = atoi(getenv("LV_SDL_VIDEO_WIDTH") ?: "320");
    const int height = atoi(getenv("LV_SDL_VIDEO_HEIGHT") ?: "170");

    lv_sdl_window_create(width, height);
}

static void lv_linux_indev_init(void)
{
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();
}

#else
#error Unsupported configuration
#endif

int main(void)
{

    lv_init();

    /*Linux display device init*/
    lv_linux_disp_init();

    lv_linux_indev_init();
    ui_init();
    /*Handle LVGL tasks*/
    printf("Entering main loop...\n");
    while (1)
    {
        lv_timer_handler();
        usleep(1000);
    }

    return 0;
}
