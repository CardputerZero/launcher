#ifndef HAL_AUDIO_H
#define HAL_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

void hal_audio_init(void);
/* Async fire-and-forget, default volume = 100%. */
void hal_audio_play(const char *path);
/* Async, volume_pct = 0..100. */
void hal_audio_play_vol(const char *path, int volume_pct);
/* Blocking (used for shutdown chime so the poweroff waits). */
void hal_audio_play_sync(const char *path);
/* Blocking with volume. */
void hal_audio_play_sync_vol(const char *path, int volume_pct);
void hal_audio_stop(void);
void hal_audio_deinit(void);

/* Short UI sound effect (key click). */
int  hal_audio_click_init(const char *wav_path);
void hal_audio_click_play(void);
void hal_audio_click_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
