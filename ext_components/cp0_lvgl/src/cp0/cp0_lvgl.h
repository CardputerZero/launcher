#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void init_freambuffer_disp();
void init_input();
void init_filesystem(void);
void init_audio();
void init_pty(void);
void init_config(void);
void init_process(void);
void init_screenshot(void);
void init_lora(void);
void init_wifi();
void init_settings(void);
void init_osinfo(void);
void init_bq27220(void);
void init_imu(void);
void init_battery();
void init_camera(void);
#ifdef __cplusplus
}
#endif
