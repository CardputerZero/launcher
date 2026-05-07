#pragma once
#include <stdint.h>
#include <string>
#include "lvgl/lvgl.h"

// LoRa APP 入口函数
void lora_app_init(lv_obj_t* parent);
void lora_app_task();

// 可根据需要扩展更多接口
