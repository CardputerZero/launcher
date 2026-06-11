#ifndef __MAIN__H__
#define __MAIN__H__

#include <sys/queue.h>
#include <pthread.h>
#ifdef __cplusplus
extern "C" {
#endif

// modifier bitmask
#define KBD_MOD_SHIFT  (1u << 0)
#define KBD_MOD_CTRL   (1u << 1)
#define KBD_MOD_ALT    (1u << 2)
#define KBD_MOD_LOGO   (1u << 3)
#define KBD_MOD_CAPS   (1u << 4)
#define KBD_MOD_NUM    (1u << 5)

// key state
#define KBD_KEY_RELEASED  0
#define KBD_KEY_PRESSED   1
#define KBD_KEY_REPEATED  2

struct key_item {
    uint32_t key_code;      // Linux evdev key code
    uint32_t keysym;        // primary XKB keysym (xkb_keysym_t)
    uint32_t codepoint;     // corresponding Unicode code point, or 0 if none
    uint32_t mods;          // modifier bitmask (KBD_MOD_*)
    int      key_state;     // 0=released, 1=pressed, 2=repeat
    char     sym_name[65];  // XKB keysym name
    char     utf8[16];      // UTF-8 character (supports multi-byte compose output)
    char     flage;         // whether free is required
    STAILQ_ENTRY(key_item) entries;
};

STAILQ_HEAD(keyboard_queue_t, key_item);
extern struct keyboard_queue_t keyboard_queue;
extern pthread_mutex_t keyboard_mutex;
extern volatile int LVGL_HOME_KEY_FLAG;
extern volatile int LVGL_RUN_FLAGE;
extern volatile uint32_t LV_EVENT_KEYBOARD;

void *keyboard_read_thread(void *argv);
const char *kbd_state_name(int state);
void kbd_dump_keymap_table(void);
#ifdef __cplusplus
}
#endif
#endif