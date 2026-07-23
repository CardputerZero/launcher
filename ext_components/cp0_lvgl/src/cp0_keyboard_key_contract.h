#pragma once

#include <stdint.h>
#include <stddef.h>

#include "input_keys.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *cp0_keyboard_control_utf8(uint32_t keycode)
{
    static const struct {
        uint32_t keycode;
        const char *utf8;
    } controls[] = {
        {KEY_ENTER, "\r"}, {KEY_KPENTER, "\r"},
        {KEY_BACKSPACE, "\x7f"}, {KEY_TAB, "\t"}, {KEY_ESC, "\x1b"},
        {KEY_UP, "\033[A"}, {KEY_DOWN, "\033[B"},
        {KEY_RIGHT, "\033[C"}, {KEY_LEFT, "\033[D"},
        {KEY_HOME, "\033[H"}, {KEY_END, "\033[F"},
        {KEY_DELETE, "\033[3~"}, {KEY_INSERT, "\033[2~"},
        {KEY_PAGEUP, "\033[5~"}, {KEY_PAGEDOWN, "\033[6~"},
        {KEY_F1, "\033OP"}, {KEY_F2, "\033OQ"},
        {KEY_F3, "\033OR"}, {KEY_F4, "\033OS"},
        {KEY_F5, "\033[15~"}, {KEY_F6, "\033[17~"},
        {KEY_F7, "\033[18~"}, {KEY_F8, "\033[19~"},
        {KEY_F9, "\033[20~"}, {KEY_F10, "\033[21~"},
        {KEY_F11, "\033[23~"}, {KEY_F12, "\033[24~"},
    };
    for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i)
        if (controls[i].keycode == keycode) return controls[i].utf8;
    return NULL;
}

#ifdef __cplusplus
}
#endif
