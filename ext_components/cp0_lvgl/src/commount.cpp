#include "hal_lvgl_bsp.h"
#include "commount.h"
#include "cp0_lvgl_app.h"
#include "lvgl/lvgl.h"
#include <cstdlib>
#include <atomic>
#include <thread>
#include <string>

#define def_hal_fun(arg, name) eventpp::CallbackList<arg> name;
#include "signal_register_plan.h"
#undef def_hal_fun

eventpp::EventQueue<CP0_C_EVENT_t, void(const std::list<std::string>)> cp0_task_queue;
static std::atomic_bool queue_run_flage{true};
extern "C" void init_lvgl_event_cpp()
{
    cp0_task_queue.appendListener(CP0_C_EVENT_END, [](const std::list<std::string> args)
                                  { queue_run_flage = false; });
    std::thread t([]()
                  {
        while (queue_run_flage.load()) {
            cp0_task_queue.wait();
            cp0_task_queue.process();
        } });
    t.detach();
}

static int config_get_int(const char *key, int default_val)
{
    int val = default_val;
    cp0_signal_config_api({"GetInt", key ? std::string(key) : std::string(), std::to_string(default_val)},
                          [&](int code, std::string data) {
                              if (code == 0) val = std::atoi(data.c_str());
                          });
    return val;
}

static void saved_volume_write(int val)
{
    cp0_signal_audio_api({"VolumeWrite", std::to_string(val)}, nullptr);
}

extern "C" void init_lvgl_saved_settings()
{
    int saved_bright = config_get_int("brightness", -1);
    if (saved_bright > 0)
        cp0_backlight_write(saved_bright);

    int saved_vol = config_get_int("volume", -1);
    if (saved_vol >= 0)
        saved_volume_write(saved_vol);
}
