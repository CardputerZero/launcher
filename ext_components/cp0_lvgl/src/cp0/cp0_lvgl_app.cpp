#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <cstdlib>
#include <functional>
#include <list>
#include <string>

extern "C" {

void cp0_signal_audio_api_play_file(const char *path)
{
    if (path && path[0]) {
        cp0_signal_audio_api({"PlayFile", std::string(path)}, nullptr);
    }
}

int cp0_volume_read(void)
{
    int volume = -1;
    cp0_signal_audio_api({"VolumeRead"}, [&](int code, std::string data) {
        if (code == 0) {
            volume = std::atoi(data.c_str());
        }
    });
    return volume;
}

int cp0_volume_write(int val)
{
    int volume = -1;
    cp0_signal_audio_api({"VolumeWrite", std::to_string(val)}, [&](int code, std::string data) {
        if (code == 0) {
            volume = std::atoi(data.c_str());
        }
    });
    return volume;
}

}
