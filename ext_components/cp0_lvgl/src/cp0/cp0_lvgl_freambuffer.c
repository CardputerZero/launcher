#include "hal_lvgl_bsp.h"
#include "lvgl/lvgl.h"
#include "commount.h"
#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <libudev.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include "cp0_lvgl.h"


void init_freambuffer_disp()
{
    lv_display_t *disp = lv_linux_fbdev_create();
    if (disp == NULL)
    {
        printf("Failed to create fbdev display!\n");
        return;
    }
    const char *device = getenv("LV_LINUX_FBDEV_DEVICE");
    char fbdev[32] = {0};
    if (device == NULL)
        while (0)
        {
            FILE *fp = popen("grep st7789 /proc/fb | awk '{print $1}'", "r");
            if (fp == NULL)
            {
                perror("popen failed");
                break;
            }

            char fb_num[32] = {0};
            if (fgets(fb_num, sizeof(fb_num), fp) == NULL)
            {
                fprintf(stderr, "st7789 framebuffer not found in /proc/fb\n");
                pclose(fp);
                break;
            }
            pclose(fp);

            fb_num[strcspn(fb_num, "\r\n")] = '\0';
            snprintf(fbdev, sizeof(fbdev), "/dev/fb%s", fb_num);
            device = fbdev;
        }
    if (device == NULL)
    {
        snprintf(fbdev, sizeof(fbdev), "/dev/fb%d", 0);
        device = fbdev;
    }

    lv_linux_fbdev_set_file(disp, device);
}