#include "keyboard_input.h"
#include "compat/input_keys.h"
#include "global_config.h"
#include "lvgl/lvgl.h"
#include "recorder_app.h"
#include "ui_recorder.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>

#if LV_USE_SDL
#include "lvgl/src/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"
#include "lvgl/src/drivers/sdl/lv_sdl_keyboard.h"
#endif

#if LV_USE_EVDEV
#include <pthread.h>
#endif

namespace {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 170;

volatile sig_atomic_t g_quit_requested = 0;
lv_obj_t *g_root = nullptr;
lv_indev_t *g_keyboard_indev = nullptr;
lv_group_t *g_group = nullptr;
UiRecorder *g_ui = nullptr;
RecorderApp *g_app = nullptr;

void request_quit()
{
    g_quit_requested = 1;
}

void handle_signal(int)
{
    request_quit();
}

int get_st7789v_fbdev(char *dev_path, size_t buf_size)
{
    if (!dev_path || buf_size == 0) return -1;
    FILE *fp = std::fopen("/proc/fb", "r");
    if (!fp) return -1;
    char line[256];
    int fb_num = -1;
    while (std::fgets(line, sizeof(line), fp)) {
        if (std::strstr(line, "fb_st7789v") && std::sscanf(line, "%d", &fb_num) == 1) break;
    }
    std::fclose(fp);
    if (fb_num < 0) return -1;
    std::snprintf(dev_path, buf_size, "/dev/fb%d", fb_num);
    return 0;
}

void handle_keyboard_event(lv_event_t *event)
{
    uint32_t key_code = 0;
    bool pressed = false;

    if (lv_event_get_code(event) == static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD)) {
        auto *key = static_cast<key_item *>(lv_event_get_param(event));
        if (!key || key->key_state == KBD_KEY_RELEASED) return; // Ignore release, allow repeat
        key_code = key->key_code;
        bool isRepeat = (key->key_state == KBD_KEY_REPEATED);
        if (g_ui && key->codepoint >= 32 && key->codepoint < 127 && !isRepeat) {
            g_ui->onCharTyped(key->codepoint);
        }
        if (g_ui) g_ui->onKeyPressed(key_code, isRepeat);
        return;
    }
#if LV_USE_SDL
    else if (lv_event_get_code(event) == LV_EVENT_KEY) {
        lv_indev_t *indev = lv_indev_active();
        if (lv_indev_get_state(indev) != LV_INDEV_STATE_PRESSED) return;
        uint32_t lv_key = lv_indev_get_key(indev);
        switch (lv_key) {
            case LV_KEY_ENTER: key_code = KEY_ENTER; break;
            case LV_KEY_ESC:   key_code = KEY_ESC; break;
            case LV_KEY_LEFT:  key_code = KEY_LEFT; break;
            case LV_KEY_RIGHT: key_code = KEY_RIGHT; break;
            case 'p': case 'P': key_code = KEY_P; break;
            case 's': case 'S': key_code = KEY_S; break;
            case ' ':           key_code = KEY_SPACE; break;
            // Map digits to F4-F8 for SDL testing convenience
            case '1':           key_code = KEY_F4; break;
            case '2':           key_code = KEY_F5; break;
            case '3':           key_code = KEY_F6; break;
            case '4':           key_code = KEY_F7; break;
            case '5':           key_code = KEY_F8; break;
            default:            key_code = lv_key; break;
        }
        if (g_ui && lv_key >= 32 && lv_key < 127) {
            g_ui->onCharTyped(lv_key);
        }
        pressed = true;
    }
#endif

    if (!pressed) return;
    if (key_code == KEY_ESC) {
        request_quit();
        return;
    }
    if (g_ui) g_ui->onKeyPressed(key_code);
}

#if LV_USE_EVDEV
int evdev_to_lv_key(uint16_t code)
{
    switch (code) {
        case KEY_UP: return LV_KEY_UP;
        case KEY_DOWN: return LV_KEY_DOWN;
        case KEY_RIGHT: return LV_KEY_RIGHT;
        case KEY_LEFT: return LV_KEY_LEFT;
        case KEY_ESC: return LV_KEY_ESC;
        case KEY_DELETE: return LV_KEY_DEL;
        case KEY_BACKSPACE: return LV_KEY_BACKSPACE;
        case KEY_ENTER: return LV_KEY_ENTER;
        case KEY_TAB: return KEY_TAB;
        case KEY_HOME: return LV_KEY_HOME;
        case KEY_END: return LV_KEY_END;
        default: return code;
    }
}

void keypad_read_cb(lv_indev_t *, lv_indev_data_t *data)
{
    data->state = LV_INDEV_STATE_RELEASED;
    data->continue_reading = false;
    pthread_mutex_lock(&keyboard_mutex);
    if (!STAILQ_EMPTY(&keyboard_queue)) {
        key_item *elm = STAILQ_FIRST(&keyboard_queue);
        STAILQ_REMOVE_HEAD(&keyboard_queue, entries);
        if (g_root) {
            lv_obj_send_event(g_root, static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), elm);
        }
        data->key = evdev_to_lv_key(elm->key_code);
        data->state = static_cast<lv_indev_state_t>(elm->key_state);
        data->continue_reading = !STAILQ_EMPTY(&keyboard_queue);
        std::free(elm);
    }
    pthread_mutex_unlock(&keyboard_mutex);
}

void lv_linux_indev_init()
{
    const char *keyboard_device = getenv("LV_LINUX_KEYBOARD_DEVICE");
    if (!keyboard_device) keyboard_device = "/dev/input/by-path/platform-3f804000.i2c-event";
    pthread_t thread_id;
    pthread_create(&thread_id, nullptr, keyboard_read_thread, const_cast<char *>(keyboard_device));
    pthread_detach(thread_id);
    g_keyboard_indev = lv_indev_create();
    lv_indev_set_type(g_keyboard_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(g_keyboard_indev, keypad_read_cb);
}
#endif

#if LV_USE_LINUX_FBDEV
void lv_linux_disp_init()
{
    char fbdev[64] = {};
    const char *device = getenv("LV_LINUX_FBDEV_DEVICE");
    if (!device && get_st7789v_fbdev(fbdev, sizeof(fbdev)) == 0) device = fbdev;
    if (!device) device = "/dev/fb0";
    lv_display_t *disp = lv_linux_fbdev_create();
    if (disp) lv_linux_fbdev_set_file(disp, device);
}

#if !LV_USE_EVDEV && !LV_USE_LIBINPUT
void lv_linux_indev_init() {}
#endif

#elif LV_USE_SDL
void lv_linux_disp_init()
{
    lv_display_t *disp = lv_sdl_window_create(kScreenWidth, kScreenHeight);
    lv_sdl_window_set_title(disp, "Recorder");
}

void lv_linux_indev_init()
{
    lv_sdl_mouse_create();
    g_keyboard_indev = lv_sdl_keyboard_create();
}
#else
#error Unsupported display configuration
#endif

} // namespace

int main()
{
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    std::signal(SIGPIPE, SIG_IGN);

    RecorderApp app;
    UiRecorder ui;
    g_app = &app;

    ui.setActionHandler([&](const std::string& action) {
        if (action == "quit") {
            request_quit();
        } else {
            app.onAction(action);
        }
    });

    app.init();

    lv_init();
#if LV_USE_FREETYPE
    lv_freetype_init(64);
#endif
    lv_linux_disp_init();
    LV_EVENT_KEYBOARD = lv_event_register_id();
    lv_linux_indev_init();

    g_root = lv_screen_active();
    lv_obj_set_size(g_root, kScreenWidth, kScreenHeight);
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_root, lv_color_hex(0xE8E8EC), 0);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, 0);

    g_ui = &ui;
    ui.init(g_root);
    app.setView(&ui);

    lv_obj_add_event_cb(g_root, handle_keyboard_event,
                        static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), nullptr);
#if LV_USE_SDL
    lv_obj_add_event_cb(g_root, handle_keyboard_event, LV_EVENT_KEY, nullptr);
#endif

    g_group = lv_group_create();
    lv_group_add_obj(g_group, g_root);
    lv_group_focus_obj(g_root);
    if (g_keyboard_indev) lv_indev_set_group(g_keyboard_indev, g_group);

    while (!g_quit_requested) {
        app.poll();
        lv_timer_handler();
        usleep(5000);
    }

    app.deinit();
    return 0;
}
