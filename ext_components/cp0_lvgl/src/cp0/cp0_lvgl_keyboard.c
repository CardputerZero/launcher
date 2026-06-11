#include "hal_lvgl_bsp.h"
#include "lvgl/lvgl.h"
#include "commount.h"
#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <libudev.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include "cp0_lvgl.h"


#define CP0_DEFAULT_INPUT_SEAT "seat-cardputer-zero"
#define CP0_KEY_QUEUE_SIZE 10
#define CP0_EVDEV_KEYCODE_OFFSET 8


typedef struct {
    uint32_t code;
    uint32_t key;
} cp0_keymap_entry_t;

typedef struct {
    uint32_t keycode;
    const char *utf8;
} cp0_ctrl_key_utf8_entry_t;

static const cp0_keymap_entry_t cp0_tca8418_keymap[64] = {
    {183, '!'},  {184, '@'}, {185, '#'}, {186, '$'},  {187, '%'}, {188, '^'},
    {189, '&'},  {190, '*'}, {191, '('}, {192, ')'},  {193, '~'}, {194, '`'},
    {195, '+'},  {196, '-'}, {197, '/'}, {198, '\\'}, {199, '{'}, {200, '}'},
    {201, '['},  {202, ']'}, {231, ','}, {232, '.'},  {233, '|'}, {209, '='},
    {210, ':'},  {211, ';'}, {212, '_'}, {213, '?'},  {214, '<'}, {215, '>'},
    {216, '\''}, {217, '"'}, {0,0},
};

static const cp0_ctrl_key_utf8_entry_t cp0_ctrl_key_utf8_map[] = {
    {KEY_ENTER, "\r"},     {KEY_KPENTER, "\r"},  {KEY_BACKSPACE, "\x7f"},
    {KEY_TAB, "\t"},       {KEY_ESC, "\x1b"},    {KEY_UP, "\033[A"},
    {KEY_DOWN, "\033[B"},  {KEY_RIGHT, "\033[C"}, {KEY_LEFT, "\033[D"},
    {KEY_HOME, "\033[H"},  {KEY_END, "\033[F"},  {KEY_DELETE, "\033[3~"},
    {KEY_INSERT, "\033[2~"}, {KEY_PAGEUP, "\033[5~"}, {KEY_PAGEDOWN, "\033[6~"},
    {KEY_F1, "\033OP"},    {KEY_F2, "\033OQ"},   {KEY_F3, "\033OR"},
    {KEY_F4, "\033OS"},    {KEY_F5, "\033[15~"}, {KEY_F6, "\033[17~"},
    {KEY_F7, "\033[18~"},  {KEY_F8, "\033[19~"}, {KEY_F9, "\033[20~"},
    {KEY_F10, "\033[21~"}, {KEY_F11, "\033[23~"}, {KEY_F12, "\033[24~"},
};

typedef struct {
    struct udev *udev;
    struct libinput *li;
    pthread_t thread;
    pthread_mutex_t lock;

    struct xkb_context *xkb_ctx;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;

    lv_point_t pointer_point;
    lv_indev_state_t pointer_state;
    lv_coord_t hor_res;
    lv_coord_t ver_res;

    cp0_key_event_t key_queue[CP0_KEY_QUEUE_SIZE];
    size_t key_head;
    size_t key_tail;
    cp0_key_event_t last_key;
} cp0_input_ctx_t;

static cp0_input_ctx_t g_input_ctx = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .pointer_state = LV_INDEV_STATE_RELEASED,
    .last_key = {.key_state = LV_INDEV_STATE_RELEASED},
};

static const char *getenv_default(const char *name, const char *dflt)
{
    const char *value = getenv(name);
    return (value && value[0] != '\0') ? value : dflt;
}

static int open_restricted(const char *path, int flags, void *user_data)
{
    (void)user_data;
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data)
{
    (void)user_data;
    close(fd);
}

static const struct libinput_interface cp0_libinput_interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static void cp0_key_queue_push(cp0_input_ctx_t *ctx, const cp0_key_event_t *event)
{
    size_t next_tail = (ctx->key_tail + 1) % CP0_KEY_QUEUE_SIZE;
    if (next_tail == ctx->key_head)
        ctx->key_head = (ctx->key_head + 1) % CP0_KEY_QUEUE_SIZE;

    ctx->key_queue[ctx->key_tail] = *event;
    ctx->key_tail = next_tail;
}

static int cp0_key_queue_pop(cp0_input_ctx_t *ctx, cp0_key_event_t *event)
{
    if (ctx->key_head == ctx->key_tail)
        return 0;

    *event = ctx->key_queue[ctx->key_head];
    ctx->key_head = (ctx->key_head + 1) % CP0_KEY_QUEUE_SIZE;
    return 1;
}

static const cp0_keymap_entry_t *cp0_tca8418_key_lookup(uint32_t code)
{
    for (size_t i = 0;; i++) {
        if (cp0_tca8418_keymap[i].code == 0 && cp0_tca8418_keymap[i].key == 0)
            break;
        if (cp0_tca8418_keymap[i].code == code)
            return &cp0_tca8418_keymap[i];
    }
    return NULL;
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

static uint32_t cp0_keycode_to_lv_key(cp0_input_ctx_t *ctx, uint32_t code)
{
    const cp0_keymap_entry_t *mapped = cp0_tca8418_key_lookup(code);
    if (mapped != NULL)
        return mapped->key;

    switch (code) {
    case KEY_BACKSPACE:
        return LV_KEY_BACKSPACE;
    case KEY_ENTER:
    case KEY_KPENTER:
        return LV_KEY_ENTER;
    case KEY_UP:
        return LV_KEY_UP;
    case KEY_DOWN:
        return LV_KEY_DOWN;
    case KEY_LEFT:
        return LV_KEY_LEFT;
    case KEY_RIGHT:
        return LV_KEY_RIGHT;
    case KEY_TAB:
        return LV_KEY_NEXT;
    case KEY_HOME:
        return LV_KEY_HOME;
    case KEY_END:
        return LV_KEY_END;
    case KEY_DELETE:
        return LV_KEY_DEL;
    case KEY_ESC:
        return LV_KEY_ESC;
    default:
        break;
    }

    if (ctx->xkb_state == NULL)
        return 0;

    xkb_keycode_t keycode = code + CP0_EVDEV_KEYCODE_OFFSET;
    char utf8[8] = {0};
    if (xkb_state_key_get_utf8(ctx->xkb_state, keycode, utf8, sizeof(utf8)) > 0)
        return cp0_utf8_first_codepoint(utf8);

    xkb_keysym_t sym = xkb_state_key_get_one_sym(ctx->xkb_state, keycode);
    uint32_t codepoint = xkb_keysym_to_utf32(sym);
    return codepoint >= 0x20 ? codepoint : 0;
}

static const char *cp0_ctrl_key_utf8_lookup(uint32_t code)
{
    for (size_t i = 0; i < sizeof(cp0_ctrl_key_utf8_map) / sizeof(cp0_ctrl_key_utf8_map[0]); i++) {
        if (cp0_ctrl_key_utf8_map[i].keycode == code)
            return cp0_ctrl_key_utf8_map[i].utf8;
    }
    return NULL;
}

static void cp0_fill_key_event(cp0_input_ctx_t *ctx, cp0_key_event_t *event, uint32_t code,
                               lv_indev_state_t state)
{
    memset(event, 0, sizeof(*event));
    event->key_code = code;
    event->key_state = state;
    event->lv_key = cp0_keycode_to_lv_key(ctx, code);

    if (ctx->xkb_state != NULL) {
        xkb_keycode_t keycode = code + CP0_EVDEV_KEYCODE_OFFSET;
        xkb_keysym_t sym = xkb_state_key_get_one_sym(ctx->xkb_state, keycode);
        event->keysym = sym;
        event->codepoint = xkb_keysym_to_utf32(sym);
        xkb_keysym_get_name(sym, event->sym_name, sizeof(event->sym_name));
        xkb_state_key_get_utf8(ctx->xkb_state, keycode, event->utf8, sizeof(event->utf8));
    }

    if (event->utf8[0] == '\0') {
        const char *ctrl_utf8 = cp0_ctrl_key_utf8_lookup(code);
        if (ctrl_utf8 != NULL)
            snprintf(event->utf8, sizeof(event->utf8), "%s", ctrl_utf8);
    }

    if (event->utf8[0] == '\0') {
        const cp0_keymap_entry_t *mapped = cp0_tca8418_key_lookup(code);
        if (mapped != NULL)
            snprintf(event->utf8, sizeof(event->utf8), "%c", (char)mapped->key);
    }
}

static void cp0_send_keyboard_event(const cp0_key_event_t *event)
{
    lv_obj_t *root = lv_display_get_screen_active(NULL);
    if (root != NULL)
        lv_obj_send_event(root, (lv_event_code_t)lv_c_event[CP0_C_EVENT_KEYBOARD], (void *)event);
}

static int cp0_init_xkb(cp0_input_ctx_t *ctx)
{
    ctx->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (ctx->xkb_ctx == NULL)
        return -1;

    struct xkb_rule_names names = {
        .rules = getenv_default("LV_LINUX_XKB_RULES", "evdev"),
        .model = getenv_default("LV_LINUX_XKB_MODEL", "pc101"),
        .layout = getenv_default("LV_LINUX_XKB_LAYOUT", "us"),
        .variant = getenv("LV_LINUX_XKB_VARIANT"),
        .options = getenv("LV_LINUX_XKB_OPTIONS"),
    };

    ctx->xkb_keymap = xkb_keymap_new_from_names(ctx->xkb_ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (ctx->xkb_keymap == NULL) {
        struct xkb_rule_names fallback = {0};
        ctx->xkb_keymap = xkb_keymap_new_from_names(ctx->xkb_ctx, &fallback, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }
    if (ctx->xkb_keymap == NULL)
        return -1;

    ctx->xkb_state = xkb_state_new(ctx->xkb_keymap);
    return ctx->xkb_state == NULL ? -1 : 0;
}

static void cp0_handle_pointer_event(cp0_input_ctx_t *ctx, struct libinput_event *event)
{
    struct libinput_event_pointer *pointer_event = NULL;
    struct libinput_event_touch *touch_event = NULL;
    enum libinput_event_type type = libinput_event_get_type(event);
    lv_coord_t hor_res = ctx->hor_res;
    lv_coord_t ver_res = ctx->ver_res;
    if (hor_res <= 0 || ver_res <= 0)
        return;

    pthread_mutex_lock(&ctx->lock);
    switch (type) {
    case LIBINPUT_EVENT_POINTER_MOTION:
        pointer_event = libinput_event_get_pointer_event(event);
        ctx->pointer_point.x = (lv_coord_t)LV_CLAMP(0, ctx->pointer_point.x +
                                                       libinput_event_pointer_get_dx(pointer_event),
                                                   hor_res - 1);
        ctx->pointer_point.y = (lv_coord_t)LV_CLAMP(0, ctx->pointer_point.y +
                                                       libinput_event_pointer_get_dy(pointer_event),
                                                   ver_res - 1);
        break;
    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
        pointer_event = libinput_event_get_pointer_event(event);
        ctx->pointer_point.x =
            (lv_coord_t)libinput_event_pointer_get_absolute_x_transformed(pointer_event, hor_res);
        ctx->pointer_point.y =
            (lv_coord_t)libinput_event_pointer_get_absolute_y_transformed(pointer_event, ver_res);
        break;
    case LIBINPUT_EVENT_POINTER_BUTTON:
        pointer_event = libinput_event_get_pointer_event(event);
        ctx->pointer_state =
            libinput_event_pointer_get_button_state(pointer_event) == LIBINPUT_BUTTON_STATE_PRESSED
                ? LV_INDEV_STATE_PRESSED
                : LV_INDEV_STATE_RELEASED;
        break;
    case LIBINPUT_EVENT_TOUCH_DOWN:
    case LIBINPUT_EVENT_TOUCH_MOTION:
        touch_event = libinput_event_get_touch_event(event);
        ctx->pointer_point.x = (lv_coord_t)libinput_event_touch_get_x_transformed(touch_event, hor_res);
        ctx->pointer_point.y = (lv_coord_t)libinput_event_touch_get_y_transformed(touch_event, ver_res);
        ctx->pointer_state = LV_INDEV_STATE_PRESSED;
        break;
    case LIBINPUT_EVENT_TOUCH_UP:
        ctx->pointer_state = LV_INDEV_STATE_RELEASED;
        break;
    default:
        break;
    }
    pthread_mutex_unlock(&ctx->lock);
}

static void cp0_handle_keyboard_event(cp0_input_ctx_t *ctx, struct libinput_event *event)
{
    if (libinput_event_get_type(event) != LIBINPUT_EVENT_KEYBOARD_KEY)
        return;

    struct libinput_event_keyboard *keyboard_event = libinput_event_get_keyboard_event(event);
    uint32_t code = libinput_event_keyboard_get_key(keyboard_event);
    enum libinput_key_state key_state = libinput_event_keyboard_get_key_state(keyboard_event);
    lv_indev_state_t state =
        key_state == LIBINPUT_KEY_STATE_PRESSED ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

    cp0_key_event_t event_data;
    cp0_fill_key_event(ctx, &event_data, code, state);

    if (ctx->xkb_state != NULL) {
        xkb_state_update_key(ctx->xkb_state, code + CP0_EVDEV_KEYCODE_OFFSET,
                             key_state == LIBINPUT_KEY_STATE_PRESSED ? XKB_KEY_DOWN : XKB_KEY_UP);
    }

    pthread_mutex_lock(&ctx->lock);
    cp0_key_queue_push(ctx, &event_data);
    pthread_mutex_unlock(&ctx->lock);
}

static void *cp0_input_thread(void *arg)
{
    cp0_input_ctx_t *ctx = (cp0_input_ctx_t *)arg;
    struct pollfd fds = {
        .fd = libinput_get_fd(ctx->li),
        .events = POLLIN,
    };

    while (1) {
        if (poll(&fds, 1, -1) <= 0)
            continue;

        libinput_dispatch(ctx->li);
        struct libinput_event *event = NULL;
        while ((event = libinput_get_event(ctx->li)) != NULL) {
            cp0_handle_pointer_event(ctx, event);
            cp0_handle_keyboard_event(ctx, event);
            libinput_event_destroy(event);
        }
    }

    return NULL;
}

static void cp0_pointer_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    cp0_input_ctx_t *ctx = &g_input_ctx;

    pthread_mutex_lock(&ctx->lock);
    data->point = ctx->pointer_point;
    data->state = ctx->pointer_state;
    pthread_mutex_unlock(&ctx->lock);
}

static void cp0_keyboard_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    cp0_input_ctx_t *ctx = &g_input_ctx;
    bool has_event = false;
    bool continue_reading = false;
    cp0_key_event_t event;

    pthread_mutex_lock(&ctx->lock);
    event = ctx->last_key;
    if (cp0_key_queue_pop(ctx, &event)) {
        ctx->last_key = event;
        has_event = true;
    }
    data->key = event.lv_key;
    data->state = (lv_indev_state_t)event.key_state;
    continue_reading = ctx->key_head != ctx->key_tail;
    data->continue_reading = continue_reading;
    pthread_mutex_unlock(&ctx->lock);

    if (has_event)
        cp0_send_keyboard_event(&event);
}


void init_input()
{
    cp0_input_ctx_t *ctx = &g_input_ctx;
    const char *seat = getenv_default("LV_LINUX_INPUT_SEAT", CP0_DEFAULT_INPUT_SEAT);
    lv_display_t *disp = lv_display_get_default();

    if (disp != NULL) {
        ctx->hor_res = lv_display_get_horizontal_resolution(disp);
        ctx->ver_res = lv_display_get_vertical_resolution(disp);
    }

    if (cp0_init_xkb(ctx) != 0)
        fprintf(stderr, "cp0_lvgl: xkb init failed, keyboard text input will be limited\n");

    ctx->udev = udev_new();
    if (ctx->udev == NULL) {
        fprintf(stderr, "cp0_lvgl: udev_new failed\n");
        return;
    }

    ctx->li = libinput_udev_create_context(&cp0_libinput_interface, NULL, ctx->udev);
    if (ctx->li == NULL) {
        fprintf(stderr, "cp0_lvgl: libinput_udev_create_context failed\n");
        return;
    }

    if (libinput_udev_assign_seat(ctx->li, seat) != 0) {
        fprintf(stderr, "cp0_lvgl: failed to assign input seat %s\n", seat);
        return;
    }

    lv_indev_t *pointer = lv_indev_create();
    if (pointer != NULL) {
        lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(pointer, cp0_pointer_read_cb);
    }

    lv_indev_t *keyboard = lv_indev_create();
    if (keyboard != NULL) {
        lv_indev_set_type(keyboard, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(keyboard, cp0_keyboard_read_cb);
    }

    if (pthread_create(&ctx->thread, NULL, cp0_input_thread, ctx) != 0) {
        fprintf(stderr, "cp0_lvgl: failed to start input thread\n");
        return;
    }
    pthread_detach(ctx->thread);
}
