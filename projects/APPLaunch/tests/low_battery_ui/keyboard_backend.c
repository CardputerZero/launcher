/* SPDX-License-Identifier: MIT */
#ifdef TEST_SDL_BACKEND
#include "../../../../ext_components/cp0_lvgl/src/sdl/sdl_lvgl_keyboard.c"
#else
#include "../../../../ext_components/cp0_lvgl/src/cp0/cp0_keyboard_lvgl_input.c"
#include <SDL.h>
struct keyboard_queue_t keyboard_queue;
pthread_mutex_t keyboard_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int LVGL_RUN_FLAGE = 1;
volatile uint32_t LV_EVENT_KEYBOARD;
void lv_sdl_keyboard_handler(SDL_Event *event) { (void)event; }
#endif

void test_keyboard_read(lv_indev_t *indev, lv_indev_data_t *data)
{
#ifdef TEST_SDL_BACKEND
    cp0_sdl_keyboard_read(indev, data);
#else
    keypad_read(indev, data);
#endif
}
