#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <string>
#include "ui/ui.h"
#include "keyboard_input.h"
#include "cp0_lvgl_app.h"
#include "cp0_lvgl_file.hpp"
#include "hal_lvgl_bsp.h"
#include "global_config.h"
#if CONFIG_BACKWARD_CPP_ENABLED
#define BACKWARD_HAS_DW 1
#include "backward.hpp"
#include "backward.h"
#endif

#include "thpool.h"
extern "C" {
    threadpool g_launch_thread_pool;
}
static const char* lock_file = NULL;
static const char *getenv_default(const char *name, const char *dflt)
{
    return getenv(name) ? : dflt;
}


#if LV_USE_EVDEV
static void lv_linux_input_env_init(void)
{
    const std::string default_keyboard_device = cp0_file_path("keyboard_device");
    const std::string default_keyboard_map = cp0_file_path("keyboard_map");
    const char *keyboard_device = getenv_default("LV_LINUX_KEYBOARD_DEVICE", default_keyboard_device.c_str());
    const char *keyboard_map = getenv_default("LV_LINUX_KEYBOARD_MAP", default_keyboard_map.c_str());
    setenv("APPLAUNCH_LINUX_KEYBOARD_DEVICE", keyboard_device, 1);
    setenv("APPLAUNCH_LINUX_KEYBOARD_MAP", keyboard_map, 1);
}
#else
static void lv_linux_input_env_init(void)
{
}
#endif

void APPLaunch_lock()
{
    static int home_back_status = 0;
    static std::chrono::time_point<std::chrono::steady_clock> start_time;

    int holder_pid = 0;
    cp0_process_check_lock(lock_file, &holder_pid);

    static int lvgl_lock = 0;
    if (holder_pid == 0) {
        if (lvgl_lock == 1) {
            LVGL_RUN_FLAGE = 1;
            lvgl_lock = 0;
            lv_obj_invalidate(lv_scr_act());
        }
    } else {
        if (LVGL_HOME_KEY_FLAG) {
            if (home_back_status == 0) {
                home_back_status = 1;
                start_time = std::chrono::steady_clock::now();
            }
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
            if (secs >= 5) {
                cp0_process_kill(holder_pid, 3000);
                home_back_status = 0;
            }
        } else {
            home_back_status = 0;
        }
        lvgl_lock = 1;
        LVGL_RUN_FLAGE = 0;
    }
}

int main(void)
{
    setenv("XDG_RUNTIME_DIR", "/run/user/1000", 1);
    setenv("PIPEWIRE_RUNTIME_DIR", "/run/user/1000", 1);
    setenv("PULSE_SERVER", "unix:/run/user/1000/pulse/native", 1);
    
    static const std::string default_lock_file = cp0_file_path("lock_file");
    lock_file = default_lock_file.c_str();
    g_launch_thread_pool = thpool_init(3);
    lv_init();
    printf("[BOOT] lv_init() done\n");

    lv_linux_input_env_init();

    printf("[BOOT] cp0_lvgl_init() starting...\n");
    cp0_lvgl_init();
    printf("[BOOT] cp0_lvgl_init() done\n");

    if (LV_EVENT_KEYBOARD == 0)
        LV_EVENT_KEYBOARD = lv_event_register_id();

    // Restore saved brightness
    {
        int saved_bright = cp0_config_get_int("brightness", -1);
        if (saved_bright > 0)
            cp0_backlight_write(saved_bright);
    }

    // Restore saved volume
    {
        int saved_vol = cp0_config_get_int("volume", -1);
        if (saved_vol >= 0)
            cp0_volume_write(saved_vol);
    }
    ui_init();

    // Force full-screen refresh immediately after init
    printf("[BOOT] ui_init done, forcing full refresh...\n");
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    printf("[BOOT] First frame flushed to fb0.\n");

    /*Handle LVGL tasks*/
    printf("Entering main loop (FULL render mode)...\n");
    while(1) {
        APPLaunch_lock();
        lv_timer_handler();
        usleep(5000);
    }

    return 0;
}
