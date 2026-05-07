#pragma once
#include <stdint.h>
#include <string>
#include <functional>
#include "lvgl/lvgl.h"

// LoRa APP 入口函数
void ui_app_lora_create(lv_obj_t* parent, lv_obj_t* root);
void ui_app_lora_set_go_back(std::function<void(void)> go_back);
void ui_app_lora_destroy(void);
void lora_app_task();

// 供 APPLaunch 设置 go_back_home 回调（兼容旧接口）
#ifdef __cplusplus
extern "C" {
#endif
void lora_set_go_back_home(void (*cb)(void));
#ifdef __cplusplus
}
#endif
