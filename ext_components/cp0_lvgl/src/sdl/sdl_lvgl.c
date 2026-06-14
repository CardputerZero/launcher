#include "hal_lvgl_bsp.h"
#include "lvgl/lvgl.h"

#if LV_USE_SDL

#include "lvgl/src/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/src/drivers/sdl/lv_sdl_private.h"
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"

#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef KEYBOARD_BUFFER_SIZE
#define KEYBOARD_BUFFER_SIZE 32
#endif

#define CP0_KBD_MOD_SHIFT (1u << 0)
#define CP0_KBD_MOD_CTRL  (1u << 1)
#define CP0_KBD_MOD_ALT   (1u << 2)
#define CP0_KBD_MOD_LOGO  (1u << 3)

uint32_t LV_C_EVENT_KEYBOARD;
uint32_t LV_C_EVENT_BATTERY;
uint32_t LV_C_EVENT_NETWORK;
uint32_t LV_C_EVENT_DATATIME;

typedef struct {
    char buf[KEYBOARD_BUFFER_SIZE];
    bool dummy_read;

    cp0_key_event_t current;
    cp0_key_event_t last;
    size_t last_utf8_len;
    bool current_valid;
} cp0_sdl_keyboard_t;

static void cp0_sdl_keyboard_read(lv_indev_t *indev, lv_indev_data_t *data);
static void cp0_sdl_keyboard_delete_cb(lv_event_t *event);

void init_lvgl_event(void)
{
    LV_C_EVENT_KEYBOARD = lv_event_register_id();
    LV_C_EVENT_BATTERY = lv_event_register_id();
    LV_C_EVENT_NETWORK = lv_event_register_id();
    LV_C_EVENT_DATATIME = lv_event_register_id();
}

static const char *getenv_default(const char *name, const char *dflt)
{
    const char *value = getenv(name);
    return (value && value[0] != '\0') ? value : dflt;
}

static uint32_t cp0_utf8_first_codepoint(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    if (p[0] == '\0')
        return 0;
    if (p[0] < 0x80)
        return p[0];
    if ((p[0] & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80)
        return ((uint32_t)(p[0] & 0x1f) << 6) | (uint32_t)(p[1] & 0x3f);
    if ((p[0] & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80)
        return ((uint32_t)(p[0] & 0x0f) << 12) | ((uint32_t)(p[1] & 0x3f) << 6) |
               (uint32_t)(p[2] & 0x3f);
    if ((p[0] & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80 &&
        (p[3] & 0xc0) == 0x80)
        return ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3f) << 12) |
               ((uint32_t)(p[2] & 0x3f) << 6) | (uint32_t)(p[3] & 0x3f);
    return 0;
}

static size_t cp0_utf8_first_len(const char *s)
{
    const unsigned char ch = (unsigned char)s[0];
    if (ch == '\0')
        return 0;
    if (ch < 0x80)
        return 1;
    if ((ch & 0xe0) == 0xc0)
        return 2;
    if ((ch & 0xf0) == 0xe0)
        return 3;
    if ((ch & 0xf8) == 0xf0)
        return 4;
    return 1;
}

static uint32_t cp0_sdl_ctrl_to_lv_key(SDL_Keycode key)
{
    switch (key) {
    case SDLK_RIGHT:
    case SDLK_KP_PLUS:
        return LV_KEY_RIGHT;
    case SDLK_LEFT:
    case SDLK_KP_MINUS:
        return LV_KEY_LEFT;
    case SDLK_UP:
        return LV_KEY_UP;
    case SDLK_DOWN:
        return LV_KEY_DOWN;
    case SDLK_ESCAPE:
        return LV_KEY_ESC;
    case SDLK_BACKSPACE:
        return LV_KEY_BACKSPACE;
    case SDLK_DELETE:
        return LV_KEY_DEL;
    case SDLK_KP_ENTER:
    case SDLK_RETURN:
        return LV_KEY_ENTER;
    case SDLK_TAB:
        return LV_KEY_NEXT;
    case SDLK_PAGEDOWN:
        return LV_KEY_NEXT;
    case SDLK_PAGEUP:
        return LV_KEY_PREV;
    case SDLK_HOME:
        return LV_KEY_HOME;
    case SDLK_END:
        return LV_KEY_END;
    default:
        return 0;
    }
}

static const char *cp0_sdl_ctrl_utf8(SDL_Keycode key)
{
    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return "\r";
    case SDLK_BACKSPACE:
        return "\x7f";
    case SDLK_TAB:
        return "\t";
    case SDLK_ESCAPE:
        return "\x1b";
    case SDLK_UP:
        return "\033[A";
    case SDLK_DOWN:
        return "\033[B";
    case SDLK_RIGHT:
        return "\033[C";
    case SDLK_LEFT:
        return "\033[D";
    case SDLK_HOME:
        return "\033[H";
    case SDLK_END:
        return "\033[F";
    case SDLK_DELETE:
        return "\033[3~";
    case SDLK_PAGEUP:
        return "\033[5~";
    case SDLK_PAGEDOWN:
        return "\033[6~";
    case SDLK_F1:
        return "\033OP";
    case SDLK_F2:
        return "\033OQ";
    case SDLK_F3:
        return "\033OR";
    case SDLK_F4:
        return "\033OS";
    case SDLK_F5:
        return "\033[15~";
    case SDLK_F6:
        return "\033[17~";
    case SDLK_F7:
        return "\033[18~";
    case SDLK_F8:
        return "\033[19~";
    case SDLK_F9:
        return "\033[20~";
    case SDLK_F10:
        return "\033[21~";
    case SDLK_F11:
        return "\033[23~";
    case SDLK_F12:
        return "\033[24~";
    default:
        return NULL;
    }
}

static uint32_t cp0_sdl_scancode_to_linux_key(SDL_Scancode scancode)
{
    switch (scancode) {
    case SDL_SCANCODE_A:
        return KEY_A;
    case SDL_SCANCODE_B:
        return KEY_B;
    case SDL_SCANCODE_C:
        return KEY_C;
    case SDL_SCANCODE_D:
        return KEY_D;
    case SDL_SCANCODE_E:
        return KEY_E;
    case SDL_SCANCODE_F:
        return KEY_F;
    case SDL_SCANCODE_G:
        return KEY_G;
    case SDL_SCANCODE_H:
        return KEY_H;
    case SDL_SCANCODE_I:
        return KEY_I;
    case SDL_SCANCODE_J:
        return KEY_J;
    case SDL_SCANCODE_K:
        return KEY_K;
    case SDL_SCANCODE_L:
        return KEY_L;
    case SDL_SCANCODE_M:
        return KEY_M;
    case SDL_SCANCODE_N:
        return KEY_N;
    case SDL_SCANCODE_O:
        return KEY_O;
    case SDL_SCANCODE_P:
        return KEY_P;
    case SDL_SCANCODE_Q:
        return KEY_Q;
    case SDL_SCANCODE_R:
        return KEY_R;
    case SDL_SCANCODE_S:
        return KEY_S;
    case SDL_SCANCODE_T:
        return KEY_T;
    case SDL_SCANCODE_U:
        return KEY_U;
    case SDL_SCANCODE_V:
        return KEY_V;
    case SDL_SCANCODE_W:
        return KEY_W;
    case SDL_SCANCODE_X:
        return KEY_X;
    case SDL_SCANCODE_Y:
        return KEY_Y;
    case SDL_SCANCODE_Z:
        return KEY_Z;
    case SDL_SCANCODE_1:
        return KEY_1;
    case SDL_SCANCODE_2:
        return KEY_2;
    case SDL_SCANCODE_3:
        return KEY_3;
    case SDL_SCANCODE_4:
        return KEY_4;
    case SDL_SCANCODE_5:
        return KEY_5;
    case SDL_SCANCODE_6:
        return KEY_6;
    case SDL_SCANCODE_7:
        return KEY_7;
    case SDL_SCANCODE_8:
        return KEY_8;
    case SDL_SCANCODE_9:
        return KEY_9;
    case SDL_SCANCODE_0:
        return KEY_0;
    case SDL_SCANCODE_RETURN:
        return KEY_ENTER;
    case SDL_SCANCODE_ESCAPE:
        return KEY_ESC;
    case SDL_SCANCODE_BACKSPACE:
        return KEY_BACKSPACE;
    case SDL_SCANCODE_TAB:
        return KEY_TAB;
    case SDL_SCANCODE_SPACE:
        return KEY_SPACE;
    case SDL_SCANCODE_MINUS:
        return KEY_MINUS;
    case SDL_SCANCODE_EQUALS:
        return KEY_EQUAL;
    case SDL_SCANCODE_LEFTBRACKET:
        return KEY_LEFTBRACE;
    case SDL_SCANCODE_RIGHTBRACKET:
        return KEY_RIGHTBRACE;
    case SDL_SCANCODE_BACKSLASH:
        return KEY_BACKSLASH;
    case SDL_SCANCODE_SEMICOLON:
        return KEY_SEMICOLON;
    case SDL_SCANCODE_APOSTROPHE:
        return KEY_APOSTROPHE;
    case SDL_SCANCODE_GRAVE:
        return KEY_GRAVE;
    case SDL_SCANCODE_COMMA:
        return KEY_COMMA;
    case SDL_SCANCODE_PERIOD:
        return KEY_DOT;
    case SDL_SCANCODE_SLASH:
        return KEY_SLASH;
    case SDL_SCANCODE_CAPSLOCK:
        return KEY_CAPSLOCK;
    case SDL_SCANCODE_F1:
        return KEY_F1;
    case SDL_SCANCODE_F2:
        return KEY_F2;
    case SDL_SCANCODE_F3:
        return KEY_F3;
    case SDL_SCANCODE_F4:
        return KEY_F4;
    case SDL_SCANCODE_F5:
        return KEY_F5;
    case SDL_SCANCODE_F6:
        return KEY_F6;
    case SDL_SCANCODE_F7:
        return KEY_F7;
    case SDL_SCANCODE_F8:
        return KEY_F8;
    case SDL_SCANCODE_F9:
        return KEY_F9;
    case SDL_SCANCODE_F10:
        return KEY_F10;
    case SDL_SCANCODE_F11:
        return KEY_F11;
    case SDL_SCANCODE_F12:
        return KEY_F12;
    case SDL_SCANCODE_INSERT:
        return KEY_INSERT;
    case SDL_SCANCODE_HOME:
        return KEY_HOME;
    case SDL_SCANCODE_PAGEUP:
        return KEY_PAGEUP;
    case SDL_SCANCODE_DELETE:
        return KEY_DELETE;
    case SDL_SCANCODE_END:
        return KEY_END;
    case SDL_SCANCODE_PAGEDOWN:
        return KEY_PAGEDOWN;
    case SDL_SCANCODE_RIGHT:
        return KEY_RIGHT;
    case SDL_SCANCODE_LEFT:
        return KEY_LEFT;
    case SDL_SCANCODE_DOWN:
        return KEY_DOWN;
    case SDL_SCANCODE_UP:
        return KEY_UP;
    case SDL_SCANCODE_LCTRL:
        return KEY_LEFTCTRL;
    case SDL_SCANCODE_LSHIFT:
        return KEY_LEFTSHIFT;
    case SDL_SCANCODE_LALT:
        return KEY_LEFTALT;
    case SDL_SCANCODE_LGUI:
        return KEY_LEFTMETA;
    case SDL_SCANCODE_RCTRL:
        return KEY_RIGHTCTRL;
    case SDL_SCANCODE_RSHIFT:
        return KEY_RIGHTSHIFT;
    case SDL_SCANCODE_RALT:
        return KEY_RIGHTALT;
    case SDL_SCANCODE_RGUI:
        return KEY_RIGHTMETA;
    default:
        return (uint32_t)scancode;
    }
}

static void cp0_send_keyboard_event(const cp0_key_event_t *event)
{
    lv_obj_t *root = lv_display_get_screen_active(NULL);
    if (root != NULL)
        lv_obj_send_event(root, (lv_event_code_t)LV_C_EVENT_KEYBOARD, (void *)event);
}

static void cp0_sdl_fill_key_meta(cp0_sdl_keyboard_t *kbd, const SDL_KeyboardEvent *event)
{
    SDL_Keycode sym = event->keysym.sym;
    SDL_Scancode scancode = event->keysym.scancode;
    SDL_Keymod mods = SDL_GetModState();
    const char *name = SDL_GetKeyName(sym);

    memset(&kbd->current, 0, sizeof(kbd->current));
    kbd->current.key_code = cp0_sdl_scancode_to_linux_key(scancode);
    kbd->current.keysym = (uint32_t)sym;
    kbd->current.mods = 0;
    kbd->current.key_state = LV_INDEV_STATE_PRESSED;
    kbd->current.lv_key = cp0_sdl_ctrl_to_lv_key(sym);
    if (mods & KMOD_SHIFT)
        kbd->current.mods |= CP0_KBD_MOD_SHIFT;
    if (mods & KMOD_CTRL)
        kbd->current.mods |= CP0_KBD_MOD_CTRL;
    if (mods & KMOD_ALT)
        kbd->current.mods |= CP0_KBD_MOD_ALT;
    if (mods & KMOD_GUI)
        kbd->current.mods |= CP0_KBD_MOD_LOGO;
    if (name != NULL)
        snprintf(kbd->current.sym_name, sizeof(kbd->current.sym_name), "%s", name);

    const char *ctrl_utf8 = cp0_sdl_ctrl_utf8(sym);
    if (ctrl_utf8 != NULL)
        snprintf(kbd->current.utf8, sizeof(kbd->current.utf8), "%s", ctrl_utf8);

    kbd->current_valid = true;
}

static void cp0_sdl_set_text_key(cp0_sdl_keyboard_t *kbd, const char *utf8, size_t len)
{
    if (!kbd->current_valid) {
        memset(&kbd->current, 0, sizeof(kbd->current));
        kbd->current.key_state = LV_INDEV_STATE_PRESSED;
        kbd->current_valid = true;
    }

    if (kbd->current.lv_key != 0 && len == 1 && (uint8_t)utf8[0] == (uint8_t)kbd->current.lv_key) {
        if (kbd->current.codepoint == 0)
            kbd->current.codepoint = cp0_utf8_first_codepoint(kbd->current.utf8);
        return;
    }

    size_t n = len < sizeof(kbd->current.utf8) - 1 ? len : sizeof(kbd->current.utf8) - 1;
    memcpy(kbd->current.utf8, utf8, n);
    kbd->current.utf8[n] = '\0';
    kbd->current.codepoint = cp0_utf8_first_codepoint(kbd->current.utf8);
    kbd->current.lv_key = kbd->current.codepoint;
}

static void cp0_sdl_enqueue_text(cp0_sdl_keyboard_t *kbd, const char *text)
{
    size_t used = strlen(kbd->buf);
    size_t incoming = strlen(text);
    if (used + incoming >= sizeof(kbd->buf))
        incoming = sizeof(kbd->buf) - used - 1;
    if (incoming > 0) {
        memcpy(kbd->buf + used, text, incoming);
        kbd->buf[used + incoming] = '\0';
    }
}

static lv_indev_t *cp0_sdl_keyboard_create(void)
{
    cp0_sdl_keyboard_t *kbd = calloc(1, sizeof(*kbd));
    if (kbd == NULL)
        return NULL;

    lv_indev_t *indev = lv_indev_create();
    if (indev == NULL) {
        free(kbd);
        return NULL;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, cp0_sdl_keyboard_read);
    lv_indev_set_driver_data(indev, kbd);
    lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
    lv_indev_add_event_cb(indev, cp0_sdl_keyboard_delete_cb, LV_EVENT_DELETE, indev);
    return indev;
}

static void cp0_sdl_keyboard_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    cp0_sdl_keyboard_t *kbd = lv_indev_get_driver_data(indev);
    size_t len = strlen(kbd->buf);
    data->continue_reading = false;

    if (kbd->dummy_read) {
        kbd->dummy_read = false;
        kbd->last.key_state = LV_INDEV_STATE_RELEASED;
        data->key = kbd->last.lv_key;
        data->state = LV_INDEV_STATE_RELEASED;
        cp0_send_keyboard_event(&kbd->last);
        memset(&kbd->last, 0, sizeof(kbd->last));
        kbd->last_utf8_len = 0;
        return;
    }

    if (len == 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    size_t char_len = cp0_utf8_first_len(kbd->buf);
    if (char_len > len)
        char_len = len;

    cp0_sdl_set_text_key(kbd, kbd->buf, char_len);
    kbd->current.key_state = LV_INDEV_STATE_PRESSED;

    kbd->last = kbd->current;
    kbd->last_utf8_len = char_len;
    data->key = kbd->current.lv_key;
    data->state = LV_INDEV_STATE_PRESSED;
    kbd->dummy_read = true;
    cp0_send_keyboard_event(&kbd->current);

    memmove(kbd->buf, kbd->buf + char_len, len - char_len + 1);
    kbd->current_valid = false;
    data->continue_reading = kbd->buf[0] != '\0';
}

static void cp0_sdl_keyboard_delete_cb(lv_event_t *event)
{
    lv_indev_t *indev = (lv_indev_t *)lv_event_get_user_data(event);
    cp0_sdl_keyboard_t *kbd = lv_indev_get_driver_data(indev);
    if (kbd != NULL) {
        lv_indev_set_driver_data(indev, NULL);
        lv_indev_set_read_cb(indev, NULL);
        free(kbd);
    }
}

void lv_sdl_keyboard_handler(SDL_Event *event)
{
    uint32_t win_id = UINT32_MAX;
    switch (event->type) {
    case SDL_KEYDOWN:
        win_id = event->key.windowID;
        break;
    case SDL_TEXTINPUT:
        win_id = event->text.windowID;
        break;
    default:
        return;
    }

    lv_display_t *disp = lv_sdl_get_disp_from_win_id(win_id);
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev != NULL) {
        if (lv_indev_get_read_cb(indev) == cp0_sdl_keyboard_read) {
            if (disp == NULL || lv_indev_get_display(indev) == disp)
                break;
        }
        indev = lv_indev_get_next(indev);
    }
    if (indev == NULL)
        return;

    cp0_sdl_keyboard_t *kbd = lv_indev_get_driver_data(indev);
    if (event->type == SDL_KEYDOWN) {
        cp0_sdl_fill_key_meta(kbd, &event->key);
        uint32_t ctrl_key = cp0_sdl_ctrl_to_lv_key(event->key.keysym.sym);
        if (ctrl_key == 0)
            return;

        char ctrl_buf[2] = {(char)ctrl_key, '\0'};
        cp0_sdl_enqueue_text(kbd, ctrl_buf);
    }
    else if (event->type == SDL_TEXTINPUT) {
        cp0_sdl_enqueue_text(kbd, event->text.text);
    }

    while (kbd->buf[0] != '\0') {
        lv_indev_read(indev);
        lv_indev_read(indev);
    }
}

static void init_sdl_disp(void)
{
    int width = atoi(getenv_default("LV_SDL_VIDEO_WIDTH", "320"));
    int height = atoi(getenv_default("LV_SDL_VIDEO_HEIGHT", "170"));
    lv_display_t *disp = lv_sdl_window_create(width, height);
    if (disp == NULL) {
        fprintf(stderr, "cp0_lvgl: failed to create SDL display\n");
        return;
    }

    lv_sdl_window_set_title(disp, getenv_default("LV_SDL_WINDOW_TITLE", "M5CardputerZero"));
}

static void init_sdl_input(void)
{
    lv_sdl_mouse_create();
    if (cp0_sdl_keyboard_create() == NULL)
        fprintf(stderr, "cp0_lvgl: failed to create SDL keyboard input\n");
}

void cp0_lvgl_init(void)
{
    init_lvgl_event();
    init_sdl_disp();
    init_sdl_input();
}

#else

void cp0_lvgl_init(void)
{
}

#endif
