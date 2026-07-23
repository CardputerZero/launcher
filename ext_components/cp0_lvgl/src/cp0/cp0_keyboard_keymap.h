#pragma once

#include <stdint.h>
#include "../cp0_keyboard_key_contract.h"

struct cp0_keyboard_keymap_entry {
    uint32_t keycode;
    const char *sym_name;
    const char *utf8;
};

void cp0_keyboard_keymap_load(void);
const struct cp0_keyboard_keymap_entry *cp0_keyboard_keymap_lookup(uint32_t keycode);
