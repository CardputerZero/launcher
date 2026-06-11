#include "hal_lvgl_bsp.h"
#include "lvgl/lvgl.h"
#include "cp0_lvgl.h"
#include "cp0_lvgl_app.h"
#include <functional>
#include <memory>

class BatterySystem
{
public:
    void pub()
    {
        if (lv_c_event[CP0_C_EVENT_BATTERY] == 0)
            return;

        cp0_battery_info_t info = cp0_battery_read();
        if (!info.valid)
            return;

        lv_obj_t *root = lv_display_get_screen_active(NULL);
        if (root != NULL)
            lv_obj_send_event(root, (lv_event_code_t)lv_c_event[CP0_C_EVENT_BATTERY], (void *)&info);
    }
};

static void battery_timer_cb(lv_timer_t *timer)
{
    auto *battery = static_cast<BatterySystem *>(lv_timer_get_user_data(timer));
    if (battery != nullptr)
        battery->pub();
}

extern "C" void init_battery()
{
    static std::shared_ptr<BatterySystem> battery;
    if (battery)
        return;

    battery = std::make_shared<BatterySystem>();
    BatterySystem *battery_ptr = battery.get();
    cp0_signal_battery_pub.append([battery_ptr](std::function<void()> fun)
                                  { battery_ptr->pub(); });
    lv_timer_create(battery_timer_cb, 3000, battery_ptr);
}
