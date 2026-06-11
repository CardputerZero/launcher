#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <string>

extern "C" {

void cp0_audio_play(const char *path)
{
    if (path && path[0])
        cp0_signal_audio_play(std::string(path));
}

}
