#include "cp0_keyboard_key_contract.h"
#include "cp0_keyboard_queue.h"
#include "cp0_keyboard_text.h"

#include "input_keys.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct keyboard_queue_t keyboard_queue;
pthread_mutex_t keyboard_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int LVGL_HOME_KEY_FLAG;
volatile int LVGL_RUN_FLAGE = 1;
volatile uint32_t LV_EVENT_KEYBOARD;

int main(void)
{
    uint32_t codepoint = 0;
    size_t length = 0;
    assert(cp0_keyboard_utf8_decode_one("A", &codepoint, &length) == 0);
    assert(codepoint == 'A' && length == 1);
    assert(cp0_keyboard_utf8_decode_one("\xc2\xa2", &codepoint, &length) == 0);
    assert(codepoint == 0xa2 && length == 2);
    assert(cp0_keyboard_utf8_decode_one("\xe4\xb8\xad", &codepoint, &length) == 0);
    assert(codepoint == 0x4e2d && length == 3);
    assert(cp0_keyboard_utf8_decode_one("\xf0\x9f\x98\x80", &codepoint, &length) == 0);
    assert(codepoint == 0x1f600 && length == 4);
    assert(cp0_keyboard_utf8_validate("A\xe4\xb8\xad\xf0\x9f\x98\x80") == 0);

    assert(cp0_keyboard_utf8_validate(NULL) == -1);
    assert(cp0_keyboard_utf8_decode_one("", &codepoint, &length) == -1);
    assert(cp0_keyboard_utf8_validate("\x80") == -1);
    assert(cp0_keyboard_utf8_validate("\xc0\x80") == -1);
    assert(cp0_keyboard_utf8_validate("\xe0\x80\x80") == -1);
    assert(cp0_keyboard_utf8_validate("\xed\xa0\x80") == -1);
    assert(cp0_keyboard_utf8_validate("\xf4\x90\x80\x80") == -1);
    assert(cp0_keyboard_utf8_validate("\xe4\xb8") == -1);

    char copied[6];
    assert(cp0_keyboard_utf8_copy(copied, sizeof(copied), "A\xf0\x9f\x98\x80Z") == 5);
    assert(strcmp(copied, "A\xf0\x9f\x98\x80") == 0);
    assert(cp0_keyboard_utf8_copy(copied, 4, "\xf0\x9f\x98\x80") == 0);
    assert(copied[0] == '\0');
    assert(cp0_keyboard_utf8_copy(copied, sizeof(copied), "\xc0\x80") == 0);

    char encoded[8];
    assert(cp0_keyboard_utf32_to_utf8(0x1f600, encoded, sizeof(encoded)) == 4);
    assert(strcmp(encoded, "\xf0\x9f\x98\x80") == 0);
    assert(cp0_keyboard_utf32_to_utf8(0xd800, encoded, sizeof(encoded)) == -1);
    assert(cp0_keyboard_utf32_to_utf8(0x110000, encoded, sizeof(encoded)) == -1);
    assert(cp0_keyboard_utf32_to_utf8('A', encoded, 1) == -1);

    assert(strcmp(cp0_keyboard_control_utf8(KEY_INSERT), "\033[2~") == 0);
    assert(strcmp(cp0_keyboard_control_utf8(KEY_KPENTER), "\r") == 0);
    assert(cp0_keyboard_control_utf8(KEY_A) == NULL);

    cp0_keyboard_queue_init();
    cp0_keyboard_queue_init();
    struct key_item source = {0};
    source.key_code = KEY_ESC;
    source.key_state = KBD_KEY_PRESSED;
    strcpy(source.utf8, "\x1b");
    assert(cp0_keyboard_queue_push(&source) == 0);
    assert(LVGL_HOME_KEY_FLAG == KBD_KEY_PRESSED);
    assert(cp0_keyboard_queue_has_data());
    struct key_item *item = cp0_keyboard_queue_pop();
    assert(item && item != &source && item->key_code == KEY_ESC);
    free(item);
    assert(!cp0_keyboard_queue_has_data());

    LVGL_RUN_FLAGE = 0;
    assert(cp0_keyboard_queue_push(&source) == 0);
    assert(!cp0_keyboard_queue_has_data());
    LVGL_RUN_FLAGE = 1;
    assert(cp0_keyboard_queue_push(NULL) == -1);
    cp0_keyboard_queue_clear();
    return 0;
}
