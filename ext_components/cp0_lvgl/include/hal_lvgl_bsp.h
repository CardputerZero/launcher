#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CP0_C_EVENT_KEYBOARD = 0,
    CP0_C_EVENT_BATTERY,
    CP0_C_EVENT_NETWORK,
    CP0_C_EVENT_DATATIME,
    CP0_C_EVENT_END,
} CP0_C_EVENT_t;

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
extern uint32_t lv_c_event[(2*CP0_C_EVENT_END)];

void cp0_lvgl_init(void);
const char *hal_path_audio_dir(void);
#ifdef __cplusplus
}
// #include <sigslot/signal.hpp>
#include "eventpp/callbacklist.h"
#include "eventpp/eventqueue.h"
#include <string>
#include <functional>

#define def_hal_fun(arg, name) extern eventpp::CallbackList<arg> name;
#include "signal_register_plan.h"
#undef def_hal_fun
extern eventpp::EventQueue<CP0_C_EVENT_t, void (const std::list<std::string>)> cp0_task_queue;

std::string cp0_file_path(std::string file);
#endif
