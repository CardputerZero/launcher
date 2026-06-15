#include "APPLaunch_api.h"

#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#ifdef HAL_PLATFORM_SDL
#include "hal/hal_audio.h"
#include "hal/hal_settings.h"
#else
#include <cstdlib>
#endif

#include <functional>
#include <list>
#include <string>

extern "C" {

void APPLaunch_audio_play_file(const char *path)
{
    if (!path || !path[0]) {
        return;
    }

#ifdef HAL_PLATFORM_SDL
    hal_audio_play(path);
#else
    cp0_signal_audio_api({"PlayFile", std::string(path)}, nullptr);
#endif
}

void APPLaunch_audio_play_asset(const char *name)
{
    if (!name || !name[0]) {
        return;
    }

#ifdef HAL_PLATFORM_SDL
    const char *path = cp0_file_path_c(name);
    APPLaunch_audio_play_file(path && path[0] ? path : name);
#else
    cp0_signal_audio_api({"Play", std::string(name)}, nullptr);
#endif
}

void APPLaunch_system_play_asset(const char *name)
{
    if (!name || !name[0]) {
        return;
    }

#ifdef HAL_PLATFORM_SDL
    APPLaunch_audio_play_asset(name);
#else
    cp0_signal_system_play(std::string(name));
#endif
}

int APPLaunch_volume_read(void)
{
#ifdef HAL_PLATFORM_SDL
    return hal_volume_read();
#else
    int volume = -1;
    cp0_signal_audio_api({"VolumeRead"}, [&](int code, std::string data) {
        if (code == 0) {
            volume = std::atoi(data.c_str());
        }
    });
    return volume;
#endif
}

int APPLaunch_volume_write(int val)
{
#ifdef HAL_PLATFORM_SDL
    return hal_volume_write(val);
#else
    int volume = -1;
    cp0_signal_audio_api({"VolumeWrite", std::to_string(val)}, [&](int code, std::string data) {
        if (code == 0) {
            volume = std::atoi(data.c_str());
        }
    });
    return volume;
#endif
}

}
