#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui/ui.h"
#include "keyboard_input.h"
#include <linux/input.h>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <algorithm>
// #include "ui/inter_process_comms.h"
#include "ui/components/ui_app_lora.hpp"

static const char *getenv_default(const char *name, const char *dflt)
{
    return getenv(name) ? : dflt;
}

int get_st7789v_fbdev(char *dev_path, size_t buf_size)
{
    if (dev_path == NULL || buf_size == 0) {
        return -1;
    }

    FILE *fp = fopen("/proc/fb", "r");
    if (fp == NULL) {
        perror("Failed to open /proc/fb");
        return -1;
    }

    char line[256];
    int  fb_num = -1;

    /* 逐行读取，查找包含 fb_st7789v 的行，格式如：0 fb_st7789v */
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "fb_st7789v") != NULL) {
            if (sscanf(line, "%d", &fb_num) == 1) {
                break;
            }
        }
    }

    fclose(fp);

    if (fb_num < 0) {
        fprintf(stderr, "fb_st7789v not found in /proc/fb\n");
        return -1;
    }

    snprintf(dev_path, buf_size, "/dev/fb%d", fb_num);
    return 0;
}


static int _evdev_process_key(uint16_t code)
{
    switch(code) {
        case KEY_UP:
            return LV_KEY_UP;
        case KEY_DOWN:
            return LV_KEY_DOWN;
        case KEY_RIGHT:
            return LV_KEY_RIGHT;
        case KEY_LEFT:
            return LV_KEY_LEFT;
        case KEY_ESC:
            return LV_KEY_ESC;
        case KEY_DELETE:
            return LV_KEY_DEL;
        case KEY_BACKSPACE:
            return LV_KEY_BACKSPACE;
        case KEY_ENTER:
            return LV_KEY_ENTER;
        case KEY_NEXT:
            return LV_KEY_NEXT;
        case KEY_TAB:
            return KEY_TAB;
        case KEY_PREVIOUS:
            return LV_KEY_PREV;
        case KEY_HOME:
            return LV_KEY_HOME;
        case KEY_END:
            return LV_KEY_END;
        default:
            return code;
    }
}


/* ----------------- LVGL read callback ----------------- */
static void keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    /* 每次 LVGL 调用，输出队列里一个事件 */
    // kp_evt_t e;
    data->state = LV_INDEV_STATE_RELEASED;
    data->continue_reading = false;
    // 出队列
    {
        pthread_mutex_lock(&keyboard_mutex);
        if (!STAILQ_EMPTY(&keyboard_queue))
        {
            struct key_item *elm = NULL;
            elm = STAILQ_FIRST(&keyboard_queue);
            STAILQ_REMOVE_HEAD(&keyboard_queue, entries);

            printf("Read key event from queue: code=%u state=%u\n", elm->key_code, elm->key_state);
            lv_obj_t *root = lv_screen_active();
            if (root) {
                lv_obj_send_event(root, (lv_event_code_t)LV_EVENT_KEYBOARD, elm);
            }
            // printf("lv_obj_send_event event to root object over\n");

            data->key = _evdev_process_key(elm->key_code);
            if(data->key)
            {
                data->state = (lv_indev_state_t)elm->key_state;
                data->continue_reading = !STAILQ_EMPTY(&keyboard_queue);
            }
            free(elm);
        }
        pthread_mutex_unlock(&keyboard_mutex);
    }
}

#if LV_USE_EVDEV

static void lv_linux_indev_init(void)
{
    const char *mouse_device = getenv_default("LV_LINUX_MOUSE_DEVICE", NULL);
    const char *keyboard_device = getenv_default("LV_LINUX_KEYBOARD_DEVICE", "/dev/input/by-path/platform-3f804000.i2c-event");
    const char *keyboard_map = getenv_default("LV_LINUX_KEYBOARD_MAP", "/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map");
    // /home/nihao/w2T/github/m5stack-linux-dtoverlays/modules/tca8418-1.0/tca8418_keypad_m5stack_keymap.map
 
 
    {
        pthread_t keyboard_read_thread_id;
        pthread_create(&keyboard_read_thread_id,       // 线程ID（输出）
                                    NULL,       // 线程属性（NULL=默认）
                                    keyboard_read_thread,// 线程函数
                                    NULL);      // 传给线程函数的参数
        pthread_detach(keyboard_read_thread_id);
    }
 
 
 
    lv_indev_t * touch = NULL;
    if (mouse_device)
        touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, mouse_device);

    lv_indev_t * keyboard = NULL;
    // if (keyboard_device)
    //     keyboard = tca8418_keypad_init(keyboard_device, keyboard_map);
    // if (keyboard_device)
    //     keyboard = lv_evdev_create(LV_INDEV_TYPE_KEYPAD, keyboard_device);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, keypad_read_cb);
}
#endif


#if LV_USE_LINUX_FBDEV

/* ========== HDMI framebuffer 镜像相关 ========== */
typedef struct {
    int fbfd;
    unsigned char *fbp;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    bool valid;
} mirror_fb_t;

static mirror_fb_t g_lcd_mirror = {0};
static mirror_fb_t g_hdmi_mirror = {0};

/* 相册APP运行时暂停LCD镜像，改用高清原图输出 */
bool g_hdmi_mirror_paused = false;

static bool open_mirror_fb(mirror_fb_t *fb, const char *device, bool read_only)
{
    int flag = read_only ? O_RDONLY : O_RDWR;
    fb->fbfd = open(device, flag);
    if (fb->fbfd < 0) return false;

    if (ioctl(fb->fbfd, FBIOGET_FSCREENINFO, &fb->finfo) == -1 ||
        ioctl(fb->fbfd, FBIOGET_VSCREENINFO, &fb->vinfo) == -1) {
        close(fb->fbfd);
        fb->fbfd = -1;
        return false;
    }

    int prot = read_only ? PROT_READ : (PROT_READ | PROT_WRITE);
    fb->fbp = (unsigned char *)mmap(0, fb->finfo.smem_len, prot, MAP_SHARED, fb->fbfd, 0);
    if (fb->fbp == MAP_FAILED) {
        close(fb->fbfd);
        fb->fbfd = -1;
        return false;
    }

    fb->valid = true;
    return true;
}

static void close_mirror_fb(mirror_fb_t *fb)
{
    if (!fb->valid) return;
    if (fb->fbp && fb->fbp != MAP_FAILED) munmap(fb->fbp, fb->finfo.smem_len);
    if (fb->fbfd >= 0) close(fb->fbfd);
    memset(fb, 0, sizeof(mirror_fb_t));
}

/* 从 /proc/fb 查找非 ST7789V 的 framebuffer（通常是 HDMI） */
static int find_hdmi_fbdev(char *out_path, size_t buf_size)
{
    FILE *fp = fopen("/proc/fb", "r");
    if (!fp) return -1;

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "fb_st7789v") != NULL) continue;

        int fb_num = -1;
        if (sscanf(line, "%d", &fb_num) == 1) {
            snprintf(out_path, buf_size, "/dev/fb%d", fb_num);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return -1;
}

/* 将 LCD framebuffer 内容缩放到 HDMI */
static void mirror_lcd_to_hdmi(void)
{
    if (g_hdmi_mirror_paused) return;
    if (!g_hdmi_mirror.valid || !g_lcd_mirror.valid) return;

    int lcd_w = g_lcd_mirror.vinfo.xres;
    int lcd_h = g_lcd_mirror.vinfo.yres;
    int hdmi_w = g_hdmi_mirror.vinfo.xres;
    int hdmi_h = g_hdmi_mirror.vinfo.yres;
    int lcd_bpp = g_lcd_mirror.vinfo.bits_per_pixel;
    int hdmi_bpp = g_hdmi_mirror.vinfo.bits_per_pixel;

    /* 相同分辨率且色深一致，直接 memcpy（零开销） */
    if (lcd_w == hdmi_w && lcd_h == hdmi_h && lcd_bpp == hdmi_bpp) {
        memcpy(g_hdmi_mirror.fbp, g_lcd_mirror.fbp, g_lcd_mirror.finfo.smem_len);
        return;
    }

    /* 仅处理 RGB565 -> RGB565 的情况 */
    if (lcd_bpp != 16 || hdmi_bpp != 16) return;

    /* 居中保持比例缩放 */
    float scale_x = (float)hdmi_w / lcd_w;
    float scale_y = (float)hdmi_h / lcd_h;
    float scale = scale_x < scale_y ? scale_x : scale_y;

    int out_w = (int)(lcd_w * scale);
    int out_h = (int)(lcd_h * scale);
    int offset_x = (hdmi_w - out_w) / 2;
    int offset_y = (hdmi_h - out_h) / 2;

    uint16_t *src = (uint16_t *)g_lcd_mirror.fbp;
    uint16_t *dst = (uint16_t *)g_hdmi_mirror.fbp;
    int src_stride = g_lcd_mirror.finfo.line_length / 2;
    int dst_stride = g_hdmi_mirror.finfo.line_length / 2;

    /* 清屏为黑色 */
    for (int y = 0; y < hdmi_h; y++) {
        memset(&dst[y * dst_stride], 0, hdmi_w * sizeof(uint16_t));
    }

    /* 最近邻缩放 */
    for (int y = 0; y < out_h; y++) {
        int src_y = (int)(y / scale);
        uint16_t *dst_row = &dst[(offset_y + y) * dst_stride + offset_x];
        const uint16_t *src_row = &src[src_y * src_stride];
        for (int x = 0; x < out_w; x++) {
            int src_x = (int)(x / scale);
            dst_row[x] = src_row[src_x];
        }
    }
}
/* ================================================ */

static void lv_linux_disp_init(void)
{
    /* 1. LCD 显示器（原有逻辑） */
    const char *device = NULL;
    char fbdev[64] = {0};
    device = getenv_default("LV_LINUX_FBDEV_DEVICE", NULL);
    if ((device == NULL) && (get_st7789v_fbdev(fbdev, sizeof(fbdev)) == 0)) {
        device = fbdev;
    }
    printf("Using LCD framebuffer device: %s\n", device);

    lv_display_t * disp = lv_linux_fbdev_create();
    if(disp == NULL) {
        printf("Failed to create fbdev display!\n");
        return;
    }
    lv_linux_fbdev_set_file(disp, device);

    /* 2. 同时以只读方式打开 LCD framebuffer，供镜像读取 */
    if (device && open_mirror_fb(&g_lcd_mirror, device, true)) {
        printf("LCD mirror buffer: %dx%d, %dbpp\n",
               g_lcd_mirror.vinfo.xres,
               g_lcd_mirror.vinfo.yres,
               g_lcd_mirror.vinfo.bits_per_pixel);
    }

    /* 3. 查找并打开 HDMI framebuffer */
    char hdmi_dev[64] = {0};
    if (find_hdmi_fbdev(hdmi_dev, sizeof(hdmi_dev)) == 0) {
        printf("HDMI framebuffer found: %s\n", hdmi_dev);
        if (open_mirror_fb(&g_hdmi_mirror, hdmi_dev, false)) {
            printf("HDMI mirror buffer: %dx%d, %dbpp\n",
                   g_hdmi_mirror.vinfo.xres,
                   g_hdmi_mirror.vinfo.yres,
                   g_hdmi_mirror.vinfo.bits_per_pixel);
        } else {
            printf("Warning: Failed to open HDMI framebuffer\n");
        }
    } else {
        printf("No HDMI framebuffer detected, running in single display mode\n");
    }

    lv_coord_t w = lv_display_get_horizontal_resolution(disp);
    lv_coord_t h = lv_display_get_vertical_resolution(disp);
    printf("LCD resolution: %dx%d\n", w, h);
}
#if ! LV_USE_EVDEV && ! LV_USE_LIBINPUT
static void lv_linux_indev_init(void)
{
}
#endif

#elif LV_USE_LINUX_DRM
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_DRM_CARD", "/dev/dri/card0");
    lv_display_t * disp = lv_linux_drm_create();

    lv_linux_drm_set_file(disp, device, -1);
}
#elif LV_USE_SDL
static void lv_linux_disp_init(void)
{
    const int width = atoi(getenv("LV_SDL_VIDEO_WIDTH") ? : "320");
    const int height = atoi(getenv("LV_SDL_VIDEO_HEIGHT") ? : "170");

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
    /*Create a Demo*/
    // lv_demo_widgets();
    // lv_demo_widgets_start_slideshow();
    // lv_demo_music();

    ui_init();
    // lv_demo_widgets(); // 用LVGL自带demo测试
    /*Handle LVGL tasks*/
    printf("Entering main loop...\n");
    while(1) {
        lv_timer_handler();

        /* 同步 LCD 内容到 HDMI */
        mirror_lcd_to_hdmi();

        usleep(5000);
    }

    return 0;
}

