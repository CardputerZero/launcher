#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <poll.h>
#include <locale.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#ifdef __linux__
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <linux/input.h>
#include <libinput.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#else
#include "compat/input_keys.h"
#endif
#include "keyboard_input.h"
#include "lvgl/lvgl.h"

/* ============================================================
 *  Global queue
 * ============================================================ */
struct keyboard_queue_t keyboard_queue;
pthread_mutex_t keyboard_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int LVGL_HOME_KEY_FLAG = 0;
volatile int LVGL_RUN_FLAGE = 1;
volatile uint32_t LV_EVENT_KEYBOARD;

static volatile int keyboard_paused_flag = 0;
#ifdef __linux__
static struct libinput *g_libinput = NULL;
void keyboard_pause(void) {
    keyboard_paused_flag = 1;
    if (g_libinput) libinput_suspend(g_libinput);
    printf("[KBD] keyboard_pause()\n");
}
void keyboard_resume(void) {
    if (g_libinput) libinput_resume(g_libinput);
    keyboard_paused_flag = 0;
    printf("[KBD] keyboard_resume()\n");
}
#else
void keyboard_pause(void) { keyboard_paused_flag = 1; }
void keyboard_resume(void) { keyboard_paused_flag = 0; }
#endif

/* ============================================================
 *  Debug: key_state name
 * ============================================================ */
const char *kbd_state_name(int state)
{
    switch (state) {
    case 0:  return "UP";
    case 1:  return "DOWN";
    case 2:  return "REPEAT";
    default: return "???";
    }
}

/* ============================================================
 *  Debug: dump all ASCII printable + letter + digit mappings once
 *  Call on startup so we can see how each key_code maps to utf8.
 * ============================================================ */
#ifdef __linux__
void kbd_dump_keymap_table(void)
{
    /* Linux evdev KEY_* values we care about: 2..53 covers 1..0 qwerty zxcvbnm */
    static const struct { uint32_t code; const char *name; } keys[] = {
        {KEY_1,"1"},{KEY_2,"2"},{KEY_3,"3"},{KEY_4,"4"},{KEY_5,"5"},
        {KEY_6,"6"},{KEY_7,"7"},{KEY_8,"8"},{KEY_9,"9"},{KEY_0,"0"},
        {KEY_Q,"q"},{KEY_W,"w"},{KEY_E,"e"},{KEY_R,"r"},{KEY_T,"t"},
        {KEY_Y,"y"},{KEY_U,"u"},{KEY_I,"i"},{KEY_O,"o"},{KEY_P,"p"},
        {KEY_A,"a"},{KEY_S,"s"},{KEY_D,"d"},{KEY_F,"f"},{KEY_G,"g"},
        {KEY_H,"h"},{KEY_J,"j"},{KEY_K,"k"},{KEY_L,"l"},
        {KEY_Z,"z"},{KEY_X,"x"},{KEY_C,"c"},{KEY_V,"v"},{KEY_B,"b"},
        {KEY_N,"n"},{KEY_M,"m"},
        {KEY_MINUS,"-"},{KEY_EQUAL,"="},{KEY_LEFTBRACE,"["},{KEY_RIGHTBRACE,"]"},
        {KEY_SEMICOLON,";"},{KEY_APOSTROPHE,"'"},{KEY_GRAVE,"`"},
        {KEY_BACKSLASH,"\\"},{KEY_COMMA,","},{KEY_DOT,"."},{KEY_SLASH,"/"},
        {KEY_SPACE,"SPACE"},{KEY_ENTER,"ENTER"},{KEY_ESC,"ESC"},
        {KEY_BACKSPACE,"BS"},{KEY_TAB,"TAB"},
        {KEY_UP,"UP"},{KEY_DOWN,"DOWN"},{KEY_LEFT,"LEFT"},{KEY_RIGHT,"RIGHT"},
        {KEY_HOME,"HOME"},{KEY_END,"END"},{KEY_DELETE,"DEL"},{KEY_INSERT,"INS"},
        {KEY_LEFTSHIFT,"LSHIFT"},{KEY_LEFTCTRL,"LCTRL"},{KEY_LEFTALT,"LALT"},
    };
    printf("[KBD] ==== evdev key_code -> label table ====\n");
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
        printf("[KBD]   code=%3u  %s\n", keys[i].code, keys[i].name);
    }
    printf("[KBD] ==== end ====\n");
    fflush(stdout);
}
#else
void kbd_dump_keymap_table(void) {}
#endif

__attribute__((weak)) void ui_global_hint_on_key(const struct key_item *elm)
{
    (void)elm;
}

static const char *getenv_default(const char *name, const char *dflt)
{
    const char *value = getenv(name);
    return (value && value[0] != '\0') ? value : dflt;
}

static int cp0_evdev_process_key(uint16_t code)
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

static void cp0_keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    data->state = LV_INDEV_STATE_RELEASED;
    data->continue_reading = false;

    pthread_mutex_lock(&keyboard_mutex);
    if (!STAILQ_EMPTY(&keyboard_queue)) {
        struct key_item *elm = STAILQ_FIRST(&keyboard_queue);
        STAILQ_REMOVE_HEAD(&keyboard_queue, entries);

        char utf8_dbg[64] = "";
        int di = 0;
        for (int i = 0; i < (int)sizeof(elm->utf8) && elm->utf8[i] && di < 60; i++) {
            unsigned char c = (unsigned char)elm->utf8[i];
            if (c >= 0x20 && c < 0x7f)
                utf8_dbg[di++] = (char)c;
            else
                di += snprintf(utf8_dbg + di, 64 - di, "\\x%02x", c);
        }
        utf8_dbg[di] = '\0';
        printf("[INDEV] dequeue code=%u state=%s sym=%s utf8='%s' cp=0x%x active_screen=%p\n",
               elm->key_code, kbd_state_name(elm->key_state), elm->sym_name,
               utf8_dbg, elm->codepoint, (void *)lv_screen_active());

        lv_obj_t *root = lv_screen_active();
        if (root)
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

static void cp0_create_lvgl_input_devices(void)
{
    const char *mouse_device = getenv_default("LV_LINUX_MOUSE_DEVICE", NULL);
    if (mouse_device)
        lv_evdev_create(LV_INDEV_TYPE_POINTER, mouse_device);

    lv_indev_t *indev = lv_indev_create();
    if (indev != NULL) {
        lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(indev, cp0_keypad_read_cb);
    }
}

/* ============================================================
 *  Parameters
 * ============================================================ */
#define EVDEV_KEYCODE_OFFSET   8
#define REPEAT_DELAY_MS      500   /* delay before first repeat */
#define REPEAT_RATE_MS        30   /* interval between subsequent repeats */

/* ============================================================
 *  libinput open/close callbacks
 * ============================================================ */
static int open_restricted(const char *path, int flags, void *user_data) {
    int fd = open(path, flags);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
        return -errno;
    }
    /* Grab the device exclusively. Without this, the kernel VT keyboard
     * handler also feeds keystrokes from the integrated TCA8418 keypad to
     * the foreground tty — leaking keys into any shell on tty1 / HDMI
     * console at the same time APPLaunch is reading them. EBUSY here is
     * non-fatal: another grabber already holds it, libinput will read
     * normally without the VT-leak protection. */
    if (ioctl(fd, EVIOCGRAB, 1) < 0 && errno != EBUSY) {
        fprintf(stderr, "[KBD] EVIOCGRAB %s failed: %s\n", path, strerror(errno));
    }
    return fd;
}
static void close_restricted(int fd, void *user_data) { close(fd); }
static const struct libinput_interface interface = {
    .open_restricted  = open_restricted,
    .close_restricted = close_restricted,
};

/* ============================================================
 *  TCA8418 custom keycode mapping table (same as the original one)
 * ============================================================ */
struct tca8418_keymap_entry {
    uint32_t    keycode;
    const char *sym_name;
    const char *utf8;
};
static const struct tca8418_keymap_entry tca8418_keymap[] = {
    { 183, "exclam",       "!"  },

    { 184, "at",           "@"  },
    { 185, "numbersign",   "#"  },
    { 186, "dollar",       "$"  },
    { 187, "percent",      "%"  },
    { 188, "asciicircum",  "^"  },
    { 189, "ampersand",    "&"  },
    { 190, "asterisk",     "*"  },
    { 191, "parenleft",    "("  },
    { 192, "parenright",   ")"  },

    { 193, "asciitilde",   "~"  },
    { 194, "grave",        "`"  },
    { 195, "plus",         "+"  },
    { 196, "minus",        "-"  },
    { 197, "slash",        "/"  },
    { 198, "backslash",    "\\" },
    { 199, "braceleft",    "{"  },
    { 200, "braceright",   "}"  },
    { 201, "bracketleft",  "["  },
    { 202, "bracketright", "]"  },

    { 231, "comma",        ","  },
    { 232, "period",       "."  },
    { 233, "bar",          "|"  },
    { 209, "equal",        "="  },
    { 210, "colon",        ":"  },
    { 211, "semicolon",    ";"  },
    { 212, "underscore",   "_"  },
    { 213, "question",     "?"  },

    { 214, "less",         "<"  },
    { 215, "greater",      ">"  },
    { 216, "apostrophe",   "'"  },
    { 217, "quotedbl",     "\"" },
};









#define TCA8418_KEYMAP_SIZE (sizeof(tca8418_keymap)/sizeof(tca8418_keymap[0]))

static const struct tca8418_keymap_entry *
tca8418_keymap_lookup(uint32_t keycode) {
    for (size_t i = 0; i < TCA8418_KEYMAP_SIZE; i++)
        if (tca8418_keymap[i].keycode == keycode)
            return &tca8418_keymap[i];
    return NULL;
}


/* ============================================================
 *  Control-key -> terminal control-character mapping table
 *  When xkbcommon does not produce utf8 for function keys, fill in ANSI escape sequences as fallback
 * ============================================================ */
struct ctrl_key_utf8_entry {
    uint32_t    keycode;   /* KEY_xxx from linux/input.h */
    const char *utf8;      /* terminal control character / ANSI escape sequence */
};

static const struct ctrl_key_utf8_entry ctrl_key_utf8_map[] = {
    /* ---- single-byte control characters ---- */
    { KEY_ENTER,     "\r"      },   /* CR  (0x0D) */
    { KEY_KPENTER,   "\r"      },   /* CR  keypad Enter */
    { KEY_BACKSPACE, "\x7f"    },   /* DEL (0x7F) */
    { KEY_TAB,       "\t"      },   /* HT  (0x09) */
    { KEY_ESC,       "\x1b"    },   /* ESC (0x1B) */

    /* ---- ANSI arrow keys ---- */
    { KEY_UP,        "\033[A"  },   /* CSI A */
    { KEY_DOWN,      "\033[B"  },   /* CSI B */
    { KEY_RIGHT,     "\033[C"  },   /* CSI C */
    { KEY_LEFT,      "\033[D"  },   /* CSI D */

    /* ---- ANSI editing keys ---- */
    { KEY_HOME,      "\033[H"  },   /* CSI H  (or "\033[1~") */
    { KEY_END,       "\033[F"  },   /* CSI F  (or "\033[4~") */
    { KEY_DELETE,    "\033[3~" },   /* SS3 ~ */
    { KEY_INSERT,    "\033[2~" },   /* CSI ~ */
    { KEY_PAGEUP,    "\033[5~" },   /* CSI ~ */
    { KEY_PAGEDOWN,  "\033[6~" },   /* CSI ~ */

    /* ---- F1-F12 ---- */
    { KEY_F1,        "\033OP"  },
    { KEY_F2,        "\033OQ"  },
    { KEY_F3,        "\033OR"  },
    { KEY_F4,        "\033OS"  },
    { KEY_F5,        "\033[15~"},
    { KEY_F6,        "\033[17~"},
    { KEY_F7,        "\033[18~"},
    { KEY_F8,        "\033[19~"},
    { KEY_F9,        "\033[20~"},
    { KEY_F10,       "\033[21~"},
    { KEY_F11,       "\033[23~"},
    { KEY_F12,       "\033[24~"},
};

static const char *ctrl_key_utf8_lookup(uint32_t keycode) {
    for (size_t i = 0; i < sizeof(ctrl_key_utf8_map) / sizeof(ctrl_key_utf8_map[0]); i++)
        if (ctrl_key_utf8_map[i].keycode == keycode)
            return ctrl_key_utf8_map[i].utf8;
    return NULL;
}





/* ============================================================
 *  Keyboard context
 * ============================================================ */
struct kbd_ctx {
    struct libinput        *li;
    struct libinput_device *dev;

    struct xkb_context        *ctx;
    struct xkb_keymap         *keymap;
    struct xkb_state          *state;
    struct xkb_compose_table  *compose_table;
    struct xkb_compose_state  *compose_state;

    /* key repeat */
    int      repeat_fd;
    bool     repeating;
    struct key_item repeat_template;   /* save the last pressed key for repeat copies */
};

/* ============================================================
 *  xkbcommon log callback
 * ============================================================ */
static void uxkb_log(struct xkb_context *ctx, enum xkb_log_level level,
                     const char *fmt, va_list args)
{
    (void)ctx; (void)level;
    vfprintf(stderr, fmt, args);
}

/* ============================================================
 *  modifier bitmask
 * ============================================================ */
static uint32_t get_mods(struct xkb_state *state) {
    uint32_t m = 0;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0)
        m |= KBD_MOD_SHIFT;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0)
        m |= KBD_MOD_CTRL;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0)
        m |= KBD_MOD_ALT;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0)
        m |= KBD_MOD_LOGO;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_EFFECTIVE) > 0)
        m |= KBD_MOD_CAPS;
    if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_NUM, XKB_STATE_MODS_EFFECTIVE) > 0)
        m |= KBD_MOD_NUM;
    return m;
}

/* ============================================================
 *  LED update (via libinput; no need to write evdev directly)
 * ============================================================ */
static void update_leds(struct kbd_ctx *kc) {
    enum libinput_led leds = 0;
    if (xkb_state_led_name_is_active(kc->state, XKB_LED_NAME_NUM) > 0)
        leds |= LIBINPUT_LED_NUM_LOCK;
    if (xkb_state_led_name_is_active(kc->state, XKB_LED_NAME_CAPS) > 0)
        leds |= LIBINPUT_LED_CAPS_LOCK;
    if (xkb_state_led_name_is_active(kc->state, XKB_LED_NAME_SCROLL) > 0)
        leds |= LIBINPUT_LED_SCROLL_LOCK;
    libinput_device_led_update(kc->dev, leds);
}

/* ============================================================
 *  Enqueue
 * ============================================================ */
static void enqueue_key(const struct key_item *src) {
    struct key_item *elm = malloc(sizeof(*elm));
    if (!elm) return;
    *elm = *src;
    elm->flage = 0;  // mark as needing free

    /* DEBUG: every raw key event from keyboard thread */
    char utf8_dbg[64] = "";
    int di = 0;
    for (int i = 0; i < (int)sizeof(elm->utf8) && elm->utf8[i] && di < 60; i++) {
        unsigned char c = (unsigned char)elm->utf8[i];
        if (c >= 0x20 && c < 0x7f) utf8_dbg[di++] = (char)c;
        else di += snprintf(utf8_dbg+di, 64-di, "\\x%02x", c);
    }
    utf8_dbg[di] = '\0';
    printf("[KBD] enqueue code=%u state=%s sym=%s utf8='%s' cp=0x%x mods=0x%x run=%d home_flag=%d\n",
           elm->key_code, kbd_state_name(elm->key_state), elm->sym_name,
           utf8_dbg, elm->codepoint, elm->mods, LVGL_RUN_FLAGE, LVGL_HOME_KEY_FLAG);

    if(elm->key_code == KEY_ESC) {
        LVGL_HOME_KEY_FLAG = elm->key_state;
        printf("[KBD] LVGL_HOME_KEY_FLAG := %d\n", LVGL_HOME_KEY_FLAG);
    }

    if(LVGL_RUN_FLAGE)
    {
        pthread_mutex_lock(&keyboard_mutex);
        STAILQ_INSERT_TAIL(&keyboard_queue, elm, entries);
        pthread_mutex_unlock(&keyboard_mutex);
    }
    else
    {
        printf("[KBD] dropped (LVGL_RUN_FLAGE=0, external app running)\n");
        free(elm);
    }

}

/* ============================================================
 *  Key repeat control
 * ============================================================ */
static void repeat_start(struct kbd_ctx *kc) {
    struct itimerspec ts = {
        .it_interval = { .tv_sec = 0, .tv_nsec = (long)REPEAT_RATE_MS  * 1000000L },
        .it_value    = { .tv_sec = 0, .tv_nsec = (long)REPEAT_DELAY_MS * 1000000L },
    };
    timerfd_settime(kc->repeat_fd, 0, &ts, NULL);
    kc->repeating = true;
}
static void repeat_stop(struct kbd_ctx *kc) {
    struct itimerspec ts = {0};
    timerfd_settime(kc->repeat_fd, 0, &ts, NULL);
    kc->repeating = false;
}

/* Encode a UTF-32 code point as UTF-8 and return the byte count */
static int utf32_to_utf8(uint32_t cp, char *out, size_t n) {
    if (n < 1) return 0;
    if (cp < 0x80) {
        if (n < 2) return 0;
        out[0] = (char)cp; out[1] = '\0'; return 1;
    } else if (cp < 0x800) {
        if (n < 3) return 0;
        out[0] = 0xC0 | (cp >> 6);
        out[1] = 0x80 | (cp & 0x3F);
        out[2] = '\0'; return 2;
    } else if (cp < 0x10000) {
        if (n < 4) return 0;
        out[0] = 0xE0 | (cp >> 12);
        out[1] = 0x80 | ((cp >> 6) & 0x3F);
        out[2] = 0x80 | (cp & 0x3F);
        out[3] = '\0'; return 3;
    } else if (cp < 0x110000) {
        if (n < 5) return 0;
        out[0] = 0xF0 | (cp >> 18);
        out[1] = 0x80 | ((cp >> 12) & 0x3F);
        out[2] = 0x80 | ((cp >> 6)  & 0x3F);
        out[3] = 0x80 | (cp & 0x3F);
        out[4] = '\0'; return 4;
    }
    out[0] = '\0'; return 0;
}

/* ============================================================
 *  Core: handle one key event
 * ============================================================ */
static void process_key(struct kbd_ctx *kc, uint32_t code, int pressed)
{
    xkb_keycode_t keycode = code + EVDEV_KEYCODE_OFFSET;
    struct key_item item = {0};
    item.key_code  = code;
    item.key_state = pressed ? KBD_KEY_PRESSED : KBD_KEY_RELEASED;

    /* ---------- 1. TCA8418 custom keycodes first ---------- */
    const struct tca8418_keymap_entry *mapped = tca8418_keymap_lookup(code);
    if (mapped) {
        xkb_keysym_t sym = xkb_keysym_from_name(mapped->sym_name,
                                                XKB_KEYSYM_NO_FLAGS);
        snprintf(item.sym_name, sizeof(item.sym_name), "%s", mapped->sym_name);
        snprintf(item.utf8,     sizeof(item.utf8),     "%s", mapped->utf8);
        item.keysym    = sym;
        item.codepoint = (sym != XKB_KEY_NoSymbol) ? xkb_keysym_to_utf32(sym) : 0;
        item.mods      = get_mods(kc->state);

        /* repeat handling */
        if (pressed) {
            kc->repeat_template = item;
            kc->repeat_template.key_state = KBD_KEY_REPEATED;
            repeat_start(kc);
        } else if (kc->repeating && kc->repeat_template.key_code == code) {
            repeat_stop(kc);
        }
        enqueue_key(&item);
        return;
    }

    /* ---------- 2. standard xkbcommon flow ---------- */
    const xkb_keysym_t *syms;
    int num = xkb_state_key_get_syms(kc->state, keycode, &syms);
    xkb_keysym_t one_sym = XKB_KEY_NoSymbol;

    if (num == 1) {
        /* handle Lock modifiers (following uterm + libxkbcommon recommendations) */
        one_sym = xkb_state_key_get_one_sym(kc->state, keycode);
    } else if (num > 1) {
        one_sym = syms[0];
    }

    /* ---------- 3. Compose handling (dead keys, etc.) ---------- */
    enum xkb_compose_status cstatus = XKB_COMPOSE_NOTHING;
    bool compose_produced_utf8 = false;

    if (kc->compose_state && pressed) {
        xkb_compose_state_feed(kc->compose_state, one_sym);
        cstatus = xkb_compose_state_get_status(kc->compose_state);

        if (cstatus == XKB_COMPOSE_COMPOSED) {
            xkb_keysym_t csym = xkb_compose_state_get_one_sym(kc->compose_state);
            if (csym != XKB_KEY_NoSymbol) {
                one_sym = csym;
            }
            /* get the composed UTF-8 string */
            int n = xkb_compose_state_get_utf8(kc->compose_state,
                                               item.utf8, sizeof(item.utf8));
            if (n > 0) compose_produced_utf8 = true;

            /* If neither keysym nor utf8 is available, treat it as canceled */
            if (csym == XKB_KEY_NoSymbol && !compose_produced_utf8)
                cstatus = XKB_COMPOSE_CANCELLED;
        }
        if (cstatus == XKB_COMPOSE_COMPOSED || cstatus == XKB_COMPOSE_CANCELLED)
            xkb_compose_state_reset(kc->compose_state);
    }

    /* ---------- 4. update xkb state (must be after get_syms) ---------- */
    enum xkb_state_component changed = 0;
    if (pressed)
        changed = xkb_state_update_key(kc->state, keycode, XKB_KEY_DOWN);
    else
        changed = xkb_state_update_key(kc->state, keycode, XKB_KEY_UP);
    if (changed & XKB_STATE_LEDS)
        update_leds(kc);

    /* ---------- 5. filter events that are composing or canceled ---------- */
    if (cstatus == XKB_COMPOSE_COMPOSING || cstatus == XKB_COMPOSE_CANCELLED)
        return;
    if (num <= 0 && !compose_produced_utf8)
        return;

    /* ---------- 6. fill item ---------- */
    xkb_keysym_get_name(one_sym, item.sym_name, sizeof(item.sym_name));
    item.keysym    = one_sym;
    item.codepoint = (one_sym != XKB_KEY_NoSymbol)
                         ? xkb_keysym_to_utf32(one_sym) : 0;
    item.mods      = get_mods(kc->state);

    /* If compose did not provide utf8, get it from xkb_state */
    if (item.utf8[0] == '\0') {
        xkb_state_key_get_utf8(kc->state, keycode,
                               item.utf8, sizeof(item.utf8));
        if (item.utf8[0] == '\0' && item.codepoint != 0) {
            /* get_utf8 filters control characters; fall back to manual encoding */
            utf32_to_utf8(item.codepoint, item.utf8, sizeof(item.utf8));
        }
        if (item.codepoint == 0)
            item.codepoint = xkb_state_key_get_utf32(kc->state, keycode);
    }
    
    /* ---------- 6.5 control-key fallback mapping ---------- */
    /* xkbcommon does not produce utf8 for function keys (UP/DOWN/ENTER/BACKSPACE, etc.),
    * manually fill ANSI/VT100 terminal control characters here for upper layers */
    if (item.utf8[0] == '\0') {
        const char *ctrl = ctrl_key_utf8_lookup(code);
        if (ctrl) {
            snprintf(item.utf8, sizeof(item.utf8), "%s", ctrl);
        }
    }

    /* ---------- 7. repeat control ---------- */
    if (pressed && xkb_keymap_key_repeats(kc->keymap, keycode)) {
        kc->repeat_template = item;
        kc->repeat_template.key_state = KBD_KEY_REPEATED;
        repeat_start(kc);
    } else if (!pressed && kc->repeating &&
               kc->repeat_template.key_code == code) {
        repeat_stop(kc);
    }

    /* ---------- 8. Enqueue ---------- */
    enqueue_key(&item);
}

/* ============================================================
 *  xkb initialization (with rmlvo fallback + compose)
 * ============================================================ */
static int init_xkb(struct kbd_ctx *kc,
                    const char *layout, const char *locale)
{
    kc->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!kc->ctx) { fprintf(stderr, "xkb_context_new failed\n"); return -1; }
    xkb_context_set_log_fn(kc->ctx, uxkb_log);

    struct xkb_rule_names rmlvo = {
        .rules   = "evdev",
        .model   = NULL,
        .layout  = layout ? layout : "us",
        .variant = NULL,
        .options = NULL,
    };
    kc->keymap = xkb_keymap_new_from_names(kc->ctx, &rmlvo,
                                           XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!kc->keymap) {
        /* empty rmlvo fallback */
        struct xkb_rule_names empty = {0};
        kc->keymap = xkb_keymap_new_from_names(kc->ctx, &empty,
                                               XKB_KEYMAP_COMPILE_NO_FLAGS);
    }
    if (!kc->keymap) { fprintf(stderr, "failed to create keymap\n"); return -1; }

    kc->state = xkb_state_new(kc->keymap);
    if (!kc->state) { fprintf(stderr, "failed to create xkb_state\n"); return -1; }

    /* Compose table */
    if (!locale || !*locale) {
        locale = getenv("LC_ALL");
        if (!locale || !*locale) locale = getenv("LC_CTYPE");
        if (!locale || !*locale) locale = getenv("LANG");
        if (!locale || !*locale) locale = "C";
    }
    kc->compose_table = xkb_compose_table_new_from_locale(
        kc->ctx, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (kc->compose_table) {
        kc->compose_state = xkb_compose_state_new(kc->compose_table,
                                                  XKB_COMPOSE_STATE_NO_FLAGS);
        if (!kc->compose_state)
            fprintf(stderr, "Warning: failed to create compose_state; disabling compose\n");
    } else {
        fprintf(stderr, "Warning: locale=%s has no compose table\n", locale);
    }
    return 0;
}

static void free_xkb(struct kbd_ctx *kc) {
    if (kc->compose_state) xkb_compose_state_unref(kc->compose_state);
    if (kc->compose_table) xkb_compose_table_unref(kc->compose_table);
    if (kc->state)  xkb_state_unref(kc->state);
    if (kc->keymap) xkb_keymap_unref(kc->keymap);
    if (kc->ctx)    xkb_context_unref(kc->ctx);
}

/* Optional: rebuild state on VT wakeup while preserving locked mods/layout (see uxkb_dev_wake_up) */
static void kbd_wake_up(struct kbd_ctx *kc) {
    xkb_mod_mask_t locked_mods = xkb_state_serialize_mods(kc->state,
                                                          XKB_STATE_MODS_LOCKED);
    xkb_layout_index_t locked_layout = xkb_state_serialize_layout(
        kc->state, XKB_STATE_LAYOUT_LOCKED);
    xkb_state_unref(kc->state);
    kc->state = xkb_state_new(kc->keymap);
    if (!kc->state) return;
    xkb_state_update_mask(kc->state, 0, 0, locked_mods, 0, 0, locked_layout);
    update_leds(kc);
    if (kc->compose_state) xkb_compose_state_reset(kc->compose_state);
}

/* ============================================================
 *  Thread main loop
 * ============================================================ */
void *keyboard_read_thread(void *argv) {
    STAILQ_INIT(&keyboard_queue);

    const char *device_path = argv ? (const char *)argv
        : "/dev/input/by-path/platform-3f804000.i2c-event";

    struct kbd_ctx kc = {0};
    kc.repeat_fd = -1;

    /* ---------- 1. libinput ---------- */
    kc.li = libinput_path_create_context(&interface, NULL);
    if (!kc.li) { fprintf(stderr, "failed to create libinput context\n"); goto out; }

    kc.dev = libinput_path_add_device(kc.li, device_path);
    if (!kc.dev) {
        fprintf(stderr, "Failed to add device %s (root permissions may be required)\n", device_path);
        goto out;
    }
    if (!libinput_device_has_capability(kc.dev, LIBINPUT_DEVICE_CAP_KEYBOARD)) {
        fprintf(stderr, "%s is not a keyboard device\n", device_path);
        goto out;
    }

    /* ---------- 2. xkbcommon ---------- */
    if (init_xkb(&kc, "us", NULL) < 0) goto out;

    /* ---------- 3. key repeat timerfd ---------- */
    kc.repeat_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (kc.repeat_fd < 0) { perror("timerfd_create"); goto out; }

    /* ---------- 4. event loop ---------- */
    int li_fd = libinput_get_fd(kc.li);
    struct pollfd pfds[2] = {
        { .fd = li_fd,        .events = POLLIN },
        { .fd = kc.repeat_fd, .events = POLLIN },
    };

    g_libinput = kc.li;
    printf("Start listening for keyboard input (%s)\n", device_path);
    libinput_dispatch(kc.li);

    while (1) {
        if (keyboard_paused_flag) {
            usleep(50000);
            continue;
        }
        int pr = poll(pfds, 2, 100);
        if (pr < 0) {
            if (errno == EINTR) continue;
            perror("poll"); break;
        }
        if (pr == 0) continue;

        /* keyboard event */
        if (pfds[0].revents & POLLIN) {
            libinput_dispatch(kc.li);
            struct libinput_event *ev;
            while ((ev = libinput_get_event(kc.li)) != NULL) {
                if (libinput_event_get_type(ev) == LIBINPUT_EVENT_KEYBOARD_KEY) {
                    struct libinput_event_keyboard *kev =
                        libinput_event_get_keyboard_event(ev);
                    uint32_t code = libinput_event_keyboard_get_key(kev);
                    enum libinput_key_state ks =
                        libinput_event_keyboard_get_key_state(kev);
                    process_key(&kc, code,
                                ks == LIBINPUT_KEY_STATE_PRESSED ? 1 : 0);
                }
                libinput_event_destroy(ev);
            }
        }

        /* repeat timer triggered */
        if (pfds[1].revents & POLLIN) {
            uint64_t exp;
            while (read(kc.repeat_fd, &exp, sizeof(exp)) == sizeof(exp)) {
                if (kc.repeating) {
                    /* refresh mods (prevents Shift, etc. from changing during repeat) */
                    kc.repeat_template.mods = get_mods(kc.state);
                    enqueue_key(&kc.repeat_template);
                }
            }
        }
    }

out:
    if (kc.repeat_fd >= 0) close(kc.repeat_fd);
    free_xkb(&kc);
    if (kc.li) libinput_unref(kc.li);
    return NULL;
}

void init_input(void)
{
    static int input_initialized = 0;
    if (input_initialized)
        return;

    if (LV_EVENT_KEYBOARD == 0)
        LV_EVENT_KEYBOARD = lv_event_register_id();

    pthread_t keyboard_read_thread_id;
    const char *keyboard_device = getenv("APPLAUNCH_LINUX_KEYBOARD_DEVICE");
    if (pthread_create(&keyboard_read_thread_id, NULL, keyboard_read_thread, (void *)keyboard_device) != 0) {
        perror("pthread_create keyboard_read_thread");
        return;
    }

    pthread_detach(keyboard_read_thread_id);
    cp0_create_lvgl_input_devices();
    input_initialized = 1;
}
