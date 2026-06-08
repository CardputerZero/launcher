#include "hal_lvgl_bsp.h"
#include "commount.h"
#include "lvgl/lvgl.h"


sigslot::signal<std::string> cp0_signal_audio;
sigslot::signal<> cp0_signal_network;
sigslot::signal<> cp0_signal_forkexec;
sigslot::signal<> cp0_signal_screenshot;

extern "C" void init_lvgl_event_cpp()
{

}
