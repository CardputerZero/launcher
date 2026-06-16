#ifndef HAL_AUDIO_H
#define HAL_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

void hal_audio_init(void);
void hal_audio_play(const char *path);
void hal_audio_play_sync(const char *path);
void hal_audio_stop(void);
void hal_audio_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
