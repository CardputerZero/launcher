#pragma once

#include "keyboard_input.h"

#ifdef __cplusplus
extern "C" {
#endif

void cp0_keyboard_queue_init(void);
int cp0_keyboard_queue_push(const struct key_item *item);
struct key_item *cp0_keyboard_queue_pop(void);
int cp0_keyboard_queue_has_data(void);
void cp0_keyboard_queue_clear(void);

#ifdef __cplusplus
}
#endif
