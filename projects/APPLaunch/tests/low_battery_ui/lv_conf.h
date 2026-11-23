/* SPDX-License-Identifier: MIT */
#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_MEM_SIZE (8 * 1024 * 1024)
#define LV_COLOR_DEPTH 16
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_USE_SNAPSHOT 1
#define LV_USE_SDL 1
#define LV_USE_LODEPNG 1
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'A'
/* Mirror the device configuration (CONFIG_V9_5_LV_FS_DEFAULT_DRIVER_LETTER=65):
 * unlettered paths, including a per-user wallpaper loaded from /home, use the
 * same POSIX/STDIO driver as the packaged assets. */
#define LV_FS_DEFAULT_DRIVER_LETTER 'A'
#endif
