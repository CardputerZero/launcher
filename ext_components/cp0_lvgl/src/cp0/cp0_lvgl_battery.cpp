#include "hal_lvgl_bsp.h"
#include "lvgl/lvgl.h"
#include "cp0_lvgl.h"
#include <cstdio>
#include <memory>
#include <mutex>
#include <iostream>
#include <fstream>
#include <string>

class BatterySystem
{
private:
    /* data */
public:
    BatterySystem() = default;
    void pub()
    {
        if (lv_c_event[CP0_C_EVENT_BATTERY] != 0)
        {
            int capacity = get_battery_value();
            if (capacity >= 0)
            {
                // lv_lock();
                lv_obj_t *root = lv_display_get_screen_active(NULL);
                if (root != NULL)
                    lv_obj_send_event(root, (lv_event_code_t)lv_c_event[CP0_C_EVENT_BATTERY], (void *)&capacity);
                // lv_unlock();
            }
        }
    }
    ~BatterySystem() = default;

private:
    std::string scanf_battery_path()
    {
        return "/sys/class/power_supply/bq27220-0/capacity";
    }
    int get_battery_value()
    {
        int capacity;
        std::string capacity_path = scanf_battery_path();
        std::ifstream file(capacity_path);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file: " << capacity_path << std::endl;
            return -1;
        }
        file >> capacity;
        if (file.fail())
        {
            std::cerr << "Failed to read capacity value" << std::endl;
            return -1;
        }
        file.close();
        return capacity;
    }
};

extern "C" void init_battery()
{
    std::shared_ptr<BatterySystem> battery = std::make_shared<BatterySystem>();
    cp0_signal_battery_pub.append([battery](std::function<void()> fun)
                                  { battery->pub(); });
}