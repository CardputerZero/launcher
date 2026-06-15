#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void APPLaunch_audio_play_file(const char *path);
void APPLaunch_audio_play_asset(const char *name);
void APPLaunch_system_play_asset(const char *name);
int APPLaunch_volume_read(void);
int APPLaunch_volume_write(int val);

#ifdef __cplusplus
}
#endif
