#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <list>
#include <string>

extern "C" {

void cp0_signal_audio_api_play_file(const char *path)
{
    if (path && path[0]) {
        cp0_signal_audio_api({"PlayFile", std::string(path)}, nullptr);
    }
}

}
