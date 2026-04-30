/*
 * ui_global_hint.cpp
 *
 * Transient on-screen hint/toast overlay.
 *
 * Behavior:
 *   (a) ESC held continuously for >= 1.5s -> show
 *       "Hold ESC 5s to return home" for ~1.5s. Short taps (released
 *       before 1.5s) show nothing, so a quick "back" press inside
 *       an app no longer flashes the return-home toast.
 *   (b) Single press of SHIFT (Aa / KEY_LEFTSHIFT) or SYM (physical
 *       "SYM" key on the M5 CardputerZero; currently best-effort mapped)
 *       -> show "Double-tap to lock" for ~1.5s.
 *
 *   Fn key is intentionally NOT hinted (no lock feature yet).
 *
 * The toast object is created lazily on first call as a child of
 * lv_layer_top(), so it floats above any screen. It is never deleted
 * (to avoid delete-inside-event issues); visibility is toggled via
 * LV_OBJ_FLAG_HIDDEN. A single lv_timer performs the auto-hide; each
 * new trigger resets the timer's remaining time.
 */

#include "ui_global_hint.h"
#include "ui.h"
#include "keyboard_input.h"
#include "lvgl/lvgl.h"

#include "compat/input_keys.h"

#include <string.h>

/* KEY_RIGHTSHIFT / KEY_COMPOSE exist in <linux/input.h> but the
 * project's non-Linux compat/input_keys.h does not define them.
 * Provide reasonable fallbacks so the file builds on Darwin / SDL too. */
#ifndef KEY_RIGHTSHIFT
#define KEY_RIGHTSHIFT 54
#endif

/* Standard Linux evdev code for the Fn key. Defined here to avoid
 * relying on any particular <linux/input-event-codes.h> having it. */
#ifndef KEY_FN
#define KEY_FN 0x1d0
#endif

/* Fallback: KEY_COMPOSE is the most common evdev code for a physical
 * "SYM" / "Menu" style key; include it alongside LEFTSHIFT / RIGHTSHIFT
 * so the hint fires regardless of which exact code the TCA8418 driver
 * chose for the SYM key on this board. */
#ifndef KEY_COMPOSE
#define KEY_COMPOSE 127
#endif

#define HINT_SHOW_MS        1500
#define HINT_ESC_HOLD_MS    1500  /* how long ESC must be held before the
                                   * "long-press 5s to return home" toast
                                   * appears. Short taps stay silent. */
#define HINT_ESC_POLL_MS    100   /* how often we re-check "is ESC still
                                   * held and past the threshold?". */
#define HINT_BG_COLOR       0x1F3A5F
#define HINT_BG_OPA         LV_OPA_80
#define HINT_TEXT_COLOR     0xFFFFFF
#define HINT_WIDTH          280
#define HINT_HEIGHT         22
#define HINT_Y_OFFSET       4    /* px below top of screen */

static lv_obj_t  *s_hint_obj   = NULL;
static lv_obj_t  *s_hint_label = NULL;
static lv_timer_t *s_hint_timer = NULL;

/* ESC-hold tracking. We do NOT fire the ESC hint on key-down anymore;
 * instead we record the down-tick and let a poll timer decide. */
static lv_timer_t *s_esc_poll_timer = NULL;
static uint32_t    s_esc_down_tick  = 0;   /* lv_tick_get() snapshot at
                                            * key-down; 0 == not held */
static bool        s_esc_hint_shown = false; /* true iff the currently
                                              * visible toast is the ESC
                                              * one (so release can hide
                                              * only its own hint). */

static void hint_timer_cb(lv_timer_t *t)
{
    /* One-shot: hide the toast and pause the timer. We keep the timer
     * alive (never let its repeat_count hit zero + auto-delete) so
     * subsequent triggers can just reset it without worrying about
     * dangling pointers. */
    if (s_hint_obj) {
        lv_obj_add_flag(s_hint_obj, LV_OBJ_FLAG_HIDDEN);
    }
    s_esc_hint_shown = false;
    if (t) lv_timer_pause(t);
}

/* Forward decl: the poll timer shows the hint, which is defined below. */
static void show_hint(const char *text);

/* Poll while ESC is held. Fires every HINT_ESC_POLL_MS. If ESC is
 * released (LVGL_HOME_KEY_FLAGE == 0) we just pause ourselves — the
 * release path also explicitly pauses, this is belt-and-suspenders.
 * If ESC is still down and has been held >= HINT_ESC_HOLD_MS, show
 * the toast once and pause until the next fresh ESC press. */
static void esc_poll_timer_cb(lv_timer_t *t)
{
    /* ESC no longer held — nothing to do until a new key-down. */
    if (LVGL_HOME_KEY_FLAGE == 0 || s_esc_down_tick == 0) {
        if (t) lv_timer_pause(t);
        return;
    }

    uint32_t elapsed = lv_tick_elaps(s_esc_down_tick);
    if (elapsed >= HINT_ESC_HOLD_MS) {
        show_hint("Hold ESC 5s to return home");
        s_esc_hint_shown = true;
        /* One-shot per hold: don't keep re-triggering show_hint every
         * 100ms while the user continues to hold ESC past 1.5s. The
         * auto-hide timer (HINT_SHOW_MS) will take the toast down. */
        if (t) lv_timer_pause(t);
    }
}

static void ensure_hint_created(void)
{
    if (s_hint_obj != NULL) return;

    lv_obj_t *parent = lv_layer_top();
    if (parent == NULL) return;

    s_hint_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(s_hint_obj);
    lv_obj_set_size(s_hint_obj, HINT_WIDTH, HINT_HEIGHT);
    lv_obj_align(s_hint_obj, LV_ALIGN_TOP_MID, 0, HINT_Y_OFFSET);

    lv_obj_set_style_bg_color(s_hint_obj, lv_color_hex(HINT_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(s_hint_obj, HINT_BG_OPA, 0);
    lv_obj_set_style_radius(s_hint_obj, 6, 0);
    lv_obj_set_style_border_width(s_hint_obj, 0, 0);
    lv_obj_set_style_pad_all(s_hint_obj, 0, 0);
    lv_obj_set_style_shadow_width(s_hint_obj, 0, 0);

    /* Block user interaction — this is purely visual. */
    lv_obj_clear_flag(s_hint_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_hint_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_hint_obj, LV_OBJ_FLAG_IGNORE_LAYOUT);

    s_hint_label = lv_label_create(s_hint_obj);
    lv_obj_set_style_text_color(s_hint_label, lv_color_hex(HINT_TEXT_COLOR), 0);
    /* Prefer the project's Chinese-capable 12pt font; it already falls
     * back to lv_font_montserrat_12 inside ui.c if freetype init failed. */
    lv_font_t *font = g_font_cn_12 ? g_font_cn_12
                                   : (lv_font_t *)&lv_font_montserrat_12;
    lv_obj_set_style_text_font(s_hint_label, font, 0);
    lv_label_set_text(s_hint_label, "");
    lv_obj_center(s_hint_label);

    lv_obj_add_flag(s_hint_obj, LV_OBJ_FLAG_HIDDEN);
}

static void show_hint(const char *text)
{
    ensure_hint_created();
    if (s_hint_obj == NULL || s_hint_label == NULL) return;

    /* Any call to show_hint replaces whatever toast was up. Clear the
     * "visible toast is the ESC one" flag; the ESC path re-sets it right
     * after calling us, so this only affects SHIFT/SYM callers — which
     * means releasing ESC after a SHIFT toast took over won't mistakenly
     * hide the SHIFT toast. */
    s_esc_hint_shown = false;

    lv_label_set_text(s_hint_label, text);
    lv_obj_align(s_hint_obj, LV_ALIGN_TOP_MID, 0, HINT_Y_OFFSET);
    lv_obj_clear_flag(s_hint_obj, LV_OBJ_FLAG_HIDDEN);

    if (s_hint_timer == NULL) {
        s_hint_timer = lv_timer_create(hint_timer_cb, HINT_SHOW_MS, NULL);
    }
    /* Keep the timer alive indefinitely; the callback pauses it after
     * one firing. Resetting here restarts the countdown from zero, so
     * successive hints extend the visible window each time. */
    if (s_hint_timer) {
        lv_timer_set_period(s_hint_timer, HINT_SHOW_MS);
        lv_timer_reset(s_hint_timer);
        lv_timer_resume(s_hint_timer);
    }
}

extern "C" void ui_global_hint_on_key(const struct key_item *elm)
{
    if (elm == NULL) return;

    const uint32_t code = elm->key_code;

    /* ESC has its own gated behavior: arm/disarm on every press/release
     * edge; do not bail on non-PRESSED states like the other keys. */
    if (code == KEY_ESC) {
        if (elm->key_state == KBD_KEY_PRESSED) {
            /* Arm the hold timer. Don't show anything yet — a quick tap
             * (common "go back" gesture) should stay silent. */
            s_esc_down_tick  = lv_tick_get();
            if (s_esc_down_tick == 0) s_esc_down_tick = 1; /* sentinel */
            s_esc_hint_shown = false;

            if (s_esc_poll_timer == NULL) {
                s_esc_poll_timer = lv_timer_create(esc_poll_timer_cb,
                                                   HINT_ESC_POLL_MS,
                                                   NULL);
            }
            if (s_esc_poll_timer) {
                lv_timer_set_period(s_esc_poll_timer, HINT_ESC_POLL_MS);
                lv_timer_reset(s_esc_poll_timer);
                lv_timer_resume(s_esc_poll_timer);
            }
        } else if (elm->key_state == KBD_KEY_RELEASED) {
            /* Released before the threshold -> hint never armed; pause
             * the poll timer. If the hint is currently visible because
             * the user did hold past 1.5s, hide it immediately on release
             * rather than waiting for the auto-hide 1.5s. */
            s_esc_down_tick = 0;
            if (s_esc_poll_timer) lv_timer_pause(s_esc_poll_timer);
            if (s_esc_hint_shown) {
                /* Reuse hint_timer_cb to do the hide + pause bookkeeping
                 * in one place. Passing s_hint_timer keeps the pause-in-
                 * callback behavior consistent. */
                hint_timer_cb(s_hint_timer);
            }
        }
        /* state == KBD_KEY_REPEATED: ignore — the poll timer handles
         * the hold detection regardless of repeat delivery. */
        return;
    }

    /* All other keys: only fire on the initial key-down edge. */
    if (elm->key_state != KBD_KEY_PRESSED) return;

    /* Explicitly skip Fn — no lock feature attached to it. */
    if (code == KEY_FN) return;

    switch (code) {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
        case KEY_COMPOSE:
            show_hint("Double-tap to lock");
            return;

        default:
            break;
    }

    /* Secondary best-effort match for the SYM key: some TCA8418 keymaps
     * tag it with sym_name "Multi_key" / "Menu" / "Sym". Match by name
     * too so we don't miss it if the raw code differs from our fallbacks. */
    if (elm->sym_name[0]) {
        if (strcmp(elm->sym_name, "Multi_key") == 0 ||
            strcmp(elm->sym_name, "Menu")      == 0 ||
            strcmp(elm->sym_name, "Sym")       == 0 ||
            strcmp(elm->sym_name, "SYM")       == 0) {
            show_hint("Double-tap to lock");
        }
    }
}
