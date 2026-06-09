#include "hal_lvgl_bsp.h"
#include "commount.h"
#include "lvgl/lvgl.h"
#include <thread>

#define def_hal_fun(arg, name) eventpp::CallbackList<arg> name;
#include "signal_register_plan.h"
#undef def_hal_fun

eventpp::EventQueue<CP0_C_EVENT_t, void(const std::list<std::string>)> cp0_task_queue;
int queue_run_flage = 1;
extern "C" void init_lvgl_event_cpp()
{
    cp0_task_queue.appendListener(CP0_C_EVENT_END, [&queue_run_flage](const std::list<std::string> args)
                                  { queue_run_flage = 0; });
    std::thread t([cp0_task_queue, &queue_run_flage]()
                  {
        while (queue_run_flage)
            cp0_task_queue.process(); });
    t.detach();
}
