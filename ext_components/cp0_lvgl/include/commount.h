#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void init_lvgl_event();
void init_lvgl_event_cpp();
void init_lvgl_env();
void init_lvgl_saved_settings();
void init_sudo_signals();
#ifdef __cplusplus
void deinit_sudo(void) noexcept;
#else
void deinit_sudo(void);
#endif

#ifdef __cplusplus
}
#endif
