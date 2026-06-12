#include "hal_lvgl_bsp.h"
#include "commount.h"
#include "cp0_lvgl_app.h"
#include "lvgl/lvgl.h"
#include <stdlib.h>

uint32_t lv_c_event[(2*CP0_C_EVENT_END)] = {0};

static void datetime_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (lv_c_event[CP0_C_EVENT_DATATIME] == 0)
        return;

    lv_obj_t *root = lv_display_get_screen_active(NULL);
    if (root != NULL)
        lv_obj_send_event(root, (lv_event_code_t)lv_c_event[CP0_C_EVENT_DATATIME], NULL);
}

static const char *getenv_default(const char *name, const char *dflt)
{
    const char *value = getenv(name);
    return value ? value : dflt;
}

void init_lvgl_env()
{
    setenv("XDG_RUNTIME_DIR", "/run/user/1000", 1);
    setenv("PIPEWIRE_RUNTIME_DIR", "/run/user/1000", 1);
    setenv("PULSE_SERVER", "unix:/run/user/1000/pulse/native", 1);

#if LV_USE_EVDEV
    const char *default_keyboard_device = cp0_file_path("keyboard_device");
    const char *keyboard_device = getenv_default("LV_LINUX_KEYBOARD_DEVICE", default_keyboard_device);
    setenv("APPLAUNCH_LINUX_KEYBOARD_DEVICE", keyboard_device, 1);

    const char *default_keyboard_map = cp0_file_path("keyboard_map");
    const char *keyboard_map = getenv_default("LV_LINUX_KEYBOARD_MAP", default_keyboard_map);
    setenv("APPLAUNCH_LINUX_KEYBOARD_MAP", keyboard_map, 1);
#endif
}

void init_lvgl_saved_settings()
{
    int saved_bright = cp0_config_get_int("brightness", -1);
    if (saved_bright > 0)
        cp0_backlight_write(saved_bright);

    int saved_vol = cp0_config_get_int("volume", -1);
    if (saved_vol >= 0)
        cp0_volume_write(saved_vol);
}

void init_lvgl_event()
{
    for (int i = 0; i < CP0_C_EVENT_END; i++)
        lv_c_event[i] = lv_event_register_id();
    lv_timer_create(datetime_timer_cb, 60000, NULL);
    init_lvgl_event_cpp();
}
