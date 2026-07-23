#include "cp0_keyboard_text.h"

static int continuation(unsigned char value)
{
    return (value & 0xc0u) == 0x80u;
}

int cp0_keyboard_utf8_decode_one(const char *text, uint32_t *codepoint, size_t *length)
{
    if (!text || !codepoint || !length || text[0] == '\0') return -1;
    const unsigned char *input = (const unsigned char *)text;
    uint32_t decoded = 0;
    size_t bytes = 0;
    if (input[0] <= 0x7f) {
        decoded = input[0];
        bytes = 1;
    } else if (input[0] >= 0xc2 && input[0] <= 0xdf && input[1] != '\0' &&
               continuation(input[1])) {
        decoded = ((uint32_t)(input[0] & 0x1f) << 6) | (uint32_t)(input[1] & 0x3f);
        bytes = 2;
    } else if (input[0] >= 0xe0 && input[0] <= 0xef && input[1] != '\0' &&
               input[2] != '\0' && continuation(input[1]) && continuation(input[2])) {
        decoded = ((uint32_t)(input[0] & 0x0f) << 12) |
                  ((uint32_t)(input[1] & 0x3f) << 6) | (uint32_t)(input[2] & 0x3f);
        if (decoded < 0x800 || (decoded >= 0xd800 && decoded <= 0xdfff)) return -1;
        bytes = 3;
    } else if (input[0] >= 0xf0 && input[0] <= 0xf4 && input[1] != '\0' &&
               input[2] != '\0' && input[3] != '\0' && continuation(input[1]) &&
               continuation(input[2]) && continuation(input[3])) {
        decoded = ((uint32_t)(input[0] & 0x07) << 18) |
                  ((uint32_t)(input[1] & 0x3f) << 12) |
                  ((uint32_t)(input[2] & 0x3f) << 6) | (uint32_t)(input[3] & 0x3f);
        if (decoded < 0x10000 || decoded > 0x10ffff) return -1;
        bytes = 4;
    } else {
        return -1;
    }
    *codepoint = decoded;
    *length = bytes;
    return 0;
}

int cp0_keyboard_utf8_validate(const char *text)
{
    if (!text) return -1;
    while (*text) {
        uint32_t codepoint = 0;
        size_t length = 0;
        if (cp0_keyboard_utf8_decode_one(text, &codepoint, &length) != 0) return -1;
        (void)codepoint;
        text += length;
    }
    return 0;
}

size_t cp0_keyboard_utf8_copy(char *output, size_t output_size, const char *text)
{
    if (!output || output_size == 0) return 0;
    output[0] = '\0';
    if (!text || cp0_keyboard_utf8_validate(text) != 0) return 0;
    size_t written = 0;
    while (*text) {
        uint32_t codepoint = 0;
        size_t length = 0;
        if (cp0_keyboard_utf8_decode_one(text, &codepoint, &length) != 0 ||
            written + length >= output_size)
            break;
        (void)codepoint;
        for (size_t i = 0; i < length; ++i) output[written + i] = text[i];
        written += length;
        text += length;
    }
    output[written] = '\0';
    return written;
}

int cp0_keyboard_utf32_to_utf8(uint32_t codepoint, char *output, size_t output_size)
{
    if (!output || output_size == 0 || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff))
        return -1;
    size_t length = 0;
    if (codepoint <= 0x7f) {
        length = 1;
        if (output_size < 2) return -1;
        output[0] = (char)codepoint;
    } else if (codepoint <= 0x7ff) {
        length = 2;
        if (output_size < 3) return -1;
        output[0] = (char)(0xc0 | (codepoint >> 6));
        output[1] = (char)(0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0xffff) {
        length = 3;
        if (output_size < 4) return -1;
        output[0] = (char)(0xe0 | (codepoint >> 12));
        output[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        output[2] = (char)(0x80 | (codepoint & 0x3f));
    } else {
        length = 4;
        if (output_size < 5) return -1;
        output[0] = (char)(0xf0 | (codepoint >> 18));
        output[1] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        output[2] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        output[3] = (char)(0x80 | (codepoint & 0x3f));
    }
    output[length] = '\0';
    return (int)length;
}
