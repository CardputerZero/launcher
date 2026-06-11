#include "hal_lvgl_bsp.h"
#include "lvgl/lvgl.h"
#include "commount.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cp0_lvgl.h"

static int find_st7789v_fbdev(char *dev_path, size_t buf_size)
{
    if (dev_path == NULL || buf_size == 0)
        return -1;

    if (access("/dev/fb_lcd", F_OK) == 0) {
        snprintf(dev_path, buf_size, "/dev/fb_lcd");
        return 0;
    }

    FILE *fp = fopen("/proc/fb", "r");
    if (fp != NULL) {
        char line[256];
        int fb_num = -1;
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (strstr(line, "fb_st7789v") != NULL || strstr(line, "st7789") != NULL) {
                if (sscanf(line, "%d", &fb_num) == 1)
                    break;
            }
        }
        fclose(fp);
        if (fb_num >= 0) {
            snprintf(dev_path, buf_size, "/dev/fb%d", fb_num);
            return 0;
        }
    }

    snprintf(dev_path, buf_size, "/dev/fb0");
    return 0;
}

void init_freambuffer_disp()
{
    lv_display_t *disp = lv_linux_fbdev_create();
    if (disp == NULL) {
        printf("Failed to create fbdev display!\n");
        return;
    }

    const char *device = getenv("LV_LINUX_FBDEV_DEVICE");
    char fbdev[64] = {0};
    if (device == NULL || device[0] == '\0') {
        find_st7789v_fbdev(fbdev, sizeof(fbdev));
        device = fbdev;
    }

    printf("Using framebuffer device: %s\n", device);
    lv_linux_fbdev_set_file(disp, device);
    lv_linux_fbdev_set_force_refresh(disp, true);

    lv_coord_t w = lv_display_get_horizontal_resolution(disp);
    lv_coord_t h = lv_display_get_vertical_resolution(disp);
    printf("Framebuffer resolution: %dx%d\n", w, h);
}
