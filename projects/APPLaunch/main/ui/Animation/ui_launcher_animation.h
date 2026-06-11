#pragma once

#include "lvgl/lvgl.h"

typedef void (*launcher_anim_ready_cb_t)(lv_anim_t *);

#ifdef __cplusplus
extern "C" {
#endif

void launcher_home_animate_right(lv_obj_t **items, launcher_anim_ready_cb_t ready_cb);
void launcher_home_animate_left(lv_obj_t **items, launcher_anim_ready_cb_t ready_cb);

#ifdef __cplusplus
}
#endif
