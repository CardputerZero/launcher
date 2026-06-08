#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t key_code;
    uint32_t keysym;
    uint32_t codepoint;
    uint32_t mods;
    int key_state;
    char sym_name[65];
    char utf8[16];
    char flage;
    uint32_t lv_key;
} cp0_key_event_t;


void cp0_lvgl_init(void);

extern uint32_t LV_C_EVENT_KEYBOARD;
extern uint32_t LV_C_EVENT_BATTERY;
extern uint32_t LV_C_EVENT_NETWORK;
extern uint32_t LV_C_EVENT_DATATIME;


#ifdef __cplusplus
}
#endif
