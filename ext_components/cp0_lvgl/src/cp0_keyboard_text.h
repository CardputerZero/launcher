#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int cp0_keyboard_utf8_decode_one(const char *text, uint32_t *codepoint, size_t *length);
int cp0_keyboard_utf8_validate(const char *text);
size_t cp0_keyboard_utf8_copy(char *output, size_t output_size, const char *text);
int cp0_keyboard_utf32_to_utf8(uint32_t codepoint, char *output, size_t output_size);

#ifdef __cplusplus
}
#endif
