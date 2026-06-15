#include "hal_lvgl_bsp.h"
#include "keyboard_input.h"
#include "commount.h"
#include "lvgl/lvgl.h"
#include "sdl_lvgl.h"


#include "lvgl/src/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/src/drivers/sdl/lv_sdl_private.h"
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"

#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    struct key_item current;
    SDL_Scancode current_scancode;
    bool current_valid;
    struct key_item active_keys[SDL_NUM_SCANCODES];
    bool active_valid[SDL_NUM_SCANCODES];
} cp0_sdl_keyboard_t;

static void cp0_sdl_keyboard_read(lv_indev_t *indev, lv_indev_data_t *data);
static void cp0_sdl_keyboard_delete_cb(lv_event_t *event);

__attribute__((weak)) void ui_global_hint_on_key(const struct key_item *elm)
{
    (void)elm;
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

static uint32_t cp0_evdev_process_key(uint16_t code)
{
    switch (code) {
    case KEY_UP:
        return LV_KEY_UP;
    case KEY_DOWN:
        return LV_KEY_DOWN;
    case KEY_RIGHT:
        return LV_KEY_RIGHT;
    case KEY_LEFT:
        return LV_KEY_LEFT;
    case KEY_ESC:
        return LV_KEY_ESC;
    case KEY_DELETE:
        return LV_KEY_DEL;
    case KEY_BACKSPACE:
        return LV_KEY_BACKSPACE;
    case KEY_ENTER:
        return LV_KEY_ENTER;
    case KEY_NEXT:
        return LV_KEY_NEXT;
    case KEY_TAB:
        return KEY_TAB;
    case KEY_PREVIOUS:
        return LV_KEY_PREV;
    case KEY_HOME:
        return LV_KEY_HOME;
    case KEY_END:
        return LV_KEY_END;
    default:
        return code;
    }
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

static bool cp0_sdl_ctrl_letter(const SDL_KeyboardEvent *event, char *out)
{
    SDL_Keymod mods = SDL_GetModState();
    SDL_Keycode sym = event->keysym.sym;

    if ((mods & KMOD_CTRL) == 0 || sym < SDLK_a || sym > SDLK_z)
        return false;

    *out = (char)(sym - SDLK_a + 1);
    return true;
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

struct keyboard_queue_t keyboard_queue;
pthread_mutex_t keyboard_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int LVGL_HOME_KEY_FLAG = 0;
volatile int LVGL_RUN_FLAGE = 1;
volatile uint32_t LV_EVENT_KEYBOARD;

void keyboard_pause(void) {}
void keyboard_resume(void) {}
void *keyboard_read_thread(void *argv)
{
    (void)argv;
    return NULL;
}
void kbd_dump_keymap_table(void) {}

const char *kbd_state_name(int state)
{
    switch (state) {
    case KBD_KEY_RELEASED:
        return "UP";
    case KBD_KEY_PRESSED:
        return "DOWN";
    case KBD_KEY_REPEATED:
        return "REPEAT";
    default:
        return "???";
    }
}

static bool cp0_sdl_queue_has_data(void)
{
    bool has_data;
    pthread_mutex_lock(&keyboard_mutex);
    has_data = !STAILQ_EMPTY(&keyboard_queue);
    pthread_mutex_unlock(&keyboard_mutex);
    return has_data;
}

static void cp0_sdl_enqueue_key(const struct key_item *event)
{
    struct key_item *elm = malloc(sizeof(*elm));
    if (elm == NULL)
        return;
    *elm = *event;
    elm->flage = 0;

    if (elm->key_code == KEY_ESC)
        LVGL_HOME_KEY_FLAG = elm->key_state;

    if (LVGL_RUN_FLAGE) {
        pthread_mutex_lock(&keyboard_mutex);
        STAILQ_INSERT_TAIL(&keyboard_queue, elm, entries);
        pthread_mutex_unlock(&keyboard_mutex);
    }
    else {
        free(elm);
    }
}

static void cp0_sdl_fill_key_meta(cp0_sdl_keyboard_t *kbd, const SDL_KeyboardEvent *event)
{
    SDL_Keycode sym = event->keysym.sym;
    SDL_Scancode scancode = event->keysym.scancode;
    SDL_Keymod mods = SDL_GetModState();
    const char *name = SDL_GetKeyName(sym);

    memset(&kbd->current, 0, sizeof(kbd->current));
    kbd->current_scancode = scancode;
    kbd->current.key_code = cp0_sdl_scancode_to_linux_key(scancode);
    kbd->current.keysym = (uint32_t)sym;
    kbd->current.mods = 0;
    kbd->current.key_state = event->repeat ? KBD_KEY_REPEATED : KBD_KEY_PRESSED;
    kbd->current.codepoint = cp0_sdl_ctrl_to_lv_key(sym);
    if (mods & KMOD_SHIFT)
        kbd->current.mods |= KBD_MOD_SHIFT;
    if (mods & KMOD_CTRL)
        kbd->current.mods |= KBD_MOD_CTRL;
    if (mods & KMOD_ALT)
        kbd->current.mods |= KBD_MOD_ALT;
    if (mods & KMOD_GUI)
        kbd->current.mods |= KBD_MOD_LOGO;
    if (name != NULL)
        snprintf(kbd->current.sym_name, sizeof(kbd->current.sym_name), "%s", name);

    const char *ctrl_utf8 = cp0_sdl_ctrl_utf8(sym);
    if (ctrl_utf8 != NULL)
        snprintf(kbd->current.utf8, sizeof(kbd->current.utf8), "%s", ctrl_utf8);

    kbd->current_valid = true;
}

static void cp0_sdl_set_text_key(cp0_sdl_keyboard_t *kbd, const char *utf8)
{
    if (!kbd->current_valid) {
        memset(&kbd->current, 0, sizeof(kbd->current));
        kbd->current.key_state = KBD_KEY_PRESSED;
        kbd->current_valid = true;
    }

    size_t len = strlen(utf8);
    size_t n = len < sizeof(kbd->current.utf8) - 1 ? len : sizeof(kbd->current.utf8) - 1;
    memcpy(kbd->current.utf8, utf8, n);
    kbd->current.utf8[n] = '\0';
    kbd->current.codepoint = cp0_utf8_first_codepoint(kbd->current.utf8);
}

static void cp0_sdl_remember_active_key(cp0_sdl_keyboard_t *kbd, SDL_Scancode scancode,
                                        const struct key_item *item)
{
    if (scancode < SDL_NUM_SCANCODES) {
        kbd->active_keys[scancode] = *item;
        kbd->active_valid[scancode] = true;
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
    (void)indev;

    data->state = LV_INDEV_STATE_RELEASED;
    data->continue_reading = false;

    pthread_mutex_lock(&keyboard_mutex);
    if (!STAILQ_EMPTY(&keyboard_queue)) {
        struct key_item *elm = STAILQ_FIRST(&keyboard_queue);
        STAILQ_REMOVE_HEAD(&keyboard_queue, entries);

        lv_obj_t *root = lv_screen_active();
        if (root != NULL)
            lv_obj_send_event(root, (lv_event_code_t)LV_EVENT_KEYBOARD, elm);

        ui_global_hint_on_key(elm);

        data->key = cp0_evdev_process_key(elm->key_code);
        if (data->key) {
            data->state = (lv_indev_state_t)elm->key_state;
            data->continue_reading = !STAILQ_EMPTY(&keyboard_queue);
        }
        free(elm);
    }
    pthread_mutex_unlock(&keyboard_mutex);
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
    case SDL_KEYUP:
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
        char ctrl_char = 0;
        if (ctrl_key != 0 || cp0_sdl_ctrl_letter(&event->key, &ctrl_char)) {
            if (ctrl_char != 0) {
                kbd->current.utf8[0] = ctrl_char;
                kbd->current.utf8[1] = '\0';
                kbd->current.codepoint = (uint32_t)ctrl_char;
            }
            cp0_sdl_enqueue_key(&kbd->current);
            cp0_sdl_remember_active_key(kbd, event->key.keysym.scancode, &kbd->current);
            kbd->current_valid = false;
        }
    }
    else if (event->type == SDL_TEXTINPUT) {
        cp0_sdl_set_text_key(kbd, event->text.text);
        cp0_sdl_enqueue_key(&kbd->current);
        cp0_sdl_remember_active_key(kbd, kbd->current_scancode, &kbd->current);
        kbd->current_valid = false;
    }
    else if (event->type == SDL_KEYUP) {
        SDL_Scancode scancode = event->key.keysym.scancode;
        struct key_item item;
        if (scancode < SDL_NUM_SCANCODES && kbd->active_valid[scancode]) {
            item = kbd->active_keys[scancode];
            kbd->active_valid[scancode] = false;
        }
        else {
            cp0_sdl_fill_key_meta(kbd, &event->key);
            item = kbd->current;
        }
        item.key_state = KBD_KEY_RELEASED;
        cp0_sdl_enqueue_key(&item);
        if (kbd->current_scancode == scancode)
            kbd->current_valid = false;
    }

    while (cp0_sdl_queue_has_data())
        lv_indev_read(indev);
}


void init_sdl_input(void)
{
    static int input_initialized = 0;
    if (input_initialized)
        return;

    STAILQ_INIT(&keyboard_queue);
    if (LV_EVENT_KEYBOARD == 0)
        LV_EVENT_KEYBOARD = lv_event_register_id();

    lv_sdl_mouse_create();
    if (cp0_sdl_keyboard_create() == NULL)
        fprintf(stderr, "cp0_lvgl: failed to create SDL keyboard input\n");

    input_initialized = 1;
}

void init_input(void)
{
    init_sdl_input();
}
