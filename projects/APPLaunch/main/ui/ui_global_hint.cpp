/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * ui_global_hint.cpp
 *
 * Transient on-screen hint/toast overlay.
 *
 * Behavior:
 *   (a) ESC held continuously for >= 1.5s -> show
 *       "Hold ESC 3s to return home" for ~1.5s. Short taps (released
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
#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_app.h"

#include "compat/input_keys.h"

#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

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
                                   * "long-press 3s to return home" toast
                                   * appears. Short taps stay silent. */
#define HINT_ESC_POLL_MS    100   /* how often we re-check "is ESC still
                                   * held and past the threshold?". */
#define HINT_BG_COLOR       0x1F3A5F
#define HINT_BG_OPA         LV_OPA_80
#define HINT_TEXT_COLOR     0xFFFFFF
#define HINT_WIDTH          280
#define HINT_HEIGHT         22
#define HINT_Y_OFFSET       4    /* px below top of screen */

#define MEDIA_OSD_SHOW_MS   1000
#define MEDIA_OSD_STEP      5
#define MEDIA_OSD_WIDTH     190
#define MEDIA_OSD_HEIGHT    82
#define MEDIA_OSD_BG_COLOR  0x20242C
#define MEDIA_OSD_BAR_BG    0x555C66
#define MEDIA_OSD_BAR_FG    0xF2C94C
#define MEDIA_OSD_TEXT      0xFFFFFF

#ifndef KEY_MUTE
#define KEY_MUTE            113
#endif
#ifndef KEY_VOLUMEDOWN
#define KEY_VOLUMEDOWN      114
#endif
#ifndef KEY_VOLUMEUP
#define KEY_VOLUMEUP        115
#endif
#ifndef KEY_BRIGHTNESSDOWN
#define KEY_BRIGHTNESSDOWN  224
#endif
#ifndef KEY_BRIGHTNESSUP
#define KEY_BRIGHTNESSUP    225
#endif

static lv_obj_t  *s_hint_obj   = NULL;
static lv_obj_t  *s_hint_label = NULL;
static lv_timer_t *s_hint_timer = NULL;

static lv_obj_t   *s_media_obj     = NULL;
static lv_obj_t   *s_media_icon    = NULL;
static lv_obj_t   *s_media_title   = NULL;
static lv_obj_t   *s_media_value   = NULL;
static lv_obj_t   *s_media_bar     = NULL;
static lv_timer_t *s_media_timer   = NULL;
static int         s_brightness_pct = -1;
static int         s_volume_pct     = -1;
static bool        s_muted          = false;

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

static int clamp_percent(int value)
{
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

static int read_config_int(const char *key, int default_val)
{
    int val = default_val;
    cp0_signal_config_api({"GetInt", key ? std::string(key) : std::string(), std::to_string(default_val)},
                          [&](int code, std::string data) {
                              if (code == 0) val = std::atoi(data.c_str());
                          });
    return val;
}

static void write_config_int(const char *key, int val)
{
    cp0_signal_config_api({"SetInt", key ? std::string(key) : std::string(), std::to_string(val)}, nullptr);
    cp0_signal_config_api({"Save"}, nullptr);
}

static int read_volume_percent(void)
{
    int volume = -1;
    cp0_signal_audio_api({"VolumeRead"}, [&](int code, std::string data) {
        if (code == 0) volume = std::atoi(data.c_str());
    });
    if (volume < 0) volume = read_config_int("volume", s_volume_pct >= 0 ? s_volume_pct : 50);
    return clamp_percent(volume);
}

static int write_volume_percent(int pct)
{
    pct = clamp_percent(pct);
    int written = pct;
    cp0_signal_audio_api({"VolumeWrite", std::to_string(pct)}, [&](int code, std::string data) {
        if (code == 0) written = clamp_percent(std::atoi(data.c_str()));
    });
    s_volume_pct = written;
    write_config_int("volume", written);
    return written;
}

static int read_brightness_percent(void)
{
    int raw = cp0_backlight_read();
    int mx = cp0_backlight_max();
    if (mx <= 0) mx = 100;
    if (raw < 0) raw = read_config_int("brightness", s_brightness_pct >= 0 ? mx * s_brightness_pct / 100 : mx);
    return clamp_percent(raw * 100 / mx);
}

static int write_brightness_percent(int pct)
{
    pct = clamp_percent(pct);
    int mx = cp0_backlight_max();
    if (mx <= 0) mx = 100;
    int raw = mx * pct / 100;
    if (pct > 0 && raw < 1) raw = 1;
    int written = cp0_backlight_write(raw);
    if (written < 0) written = raw;
    s_brightness_pct = clamp_percent(written * 100 / mx);
    write_config_int("brightness", written);
    return s_brightness_pct;
}

static bool read_system_mute(bool fallback)
{
    FILE *p = popen("pactl get-sink-mute @DEFAULT_SINK@ 2>/dev/null", "r");
    if (!p) return fallback;

    char buf[128];
    bool muted = fallback;
    while (fgets(buf, sizeof(buf), p)) {
        if (strstr(buf, "yes")) {
            muted = true;
            break;
        }
        if (strstr(buf, "no")) {
            muted = false;
            break;
        }
    }
    pclose(p);
    return muted;
}

static bool toggle_system_mute(void)
{
    bool fallback = !s_muted;
    int ret = system("pactl set-sink-mute @DEFAULT_SINK@ toggle >/dev/null 2>&1");
    if (ret != 0) {
        s_muted = fallback;
        return s_muted;
    }
    s_muted = read_system_mute(fallback);
    return s_muted;
}

static void media_timer_cb(lv_timer_t *t)
{
    if (s_media_obj) lv_obj_add_flag(s_media_obj, LV_OBJ_FLAG_HIDDEN);
    if (t) lv_timer_pause(t);
}

/* Forward decl: the poll timer shows the hint, which is defined below. */
static void show_hint(const char *text);

/* Poll while ESC is held. Fires every HINT_ESC_POLL_MS. If ESC is
 * released (LVGL_HOME_KEY_FLAG == 0) we just pause ourselves — the
 * release path also explicitly pauses, this is belt-and-suspenders.
 * If ESC is still down and has been held >= HINT_ESC_HOLD_MS, show
 * the toast once and pause until the next fresh ESC press. */
static void esc_poll_timer_cb(lv_timer_t *t)
{
    /* ESC no longer held — nothing to do until a new key-down. */
    if (LVGL_HOME_KEY_FLAG == 0 || s_esc_down_tick == 0) {
        if (t) lv_timer_pause(t);
        return;
    }

    uint32_t elapsed = lv_tick_elaps(s_esc_down_tick);
    if (elapsed >= HINT_ESC_HOLD_MS) {
        show_hint("Hold ESC 3s to return home");
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
    lv_font_t *font = launcher_fonts().get("AlibabaPuHuiTi-3-55-Regular.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD);
    lv_obj_set_style_text_font(s_hint_label, font, 0);
    lv_label_set_text(s_hint_label, "");
    lv_obj_center(s_hint_label);

    lv_obj_add_flag(s_hint_obj, LV_OBJ_FLAG_HIDDEN);
}

static void ensure_media_created(void)
{
    if (s_media_obj != NULL) return;

    lv_obj_t *parent = lv_layer_top();
    if (parent == NULL) return;

    s_media_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(s_media_obj);
    lv_obj_set_size(s_media_obj, MEDIA_OSD_WIDTH, MEDIA_OSD_HEIGHT);
    lv_obj_center(s_media_obj);
    lv_obj_set_style_bg_color(s_media_obj, lv_color_hex(MEDIA_OSD_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(s_media_obj, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_media_obj, 8, 0);
    lv_obj_set_style_border_width(s_media_obj, 1, 0);
    lv_obj_set_style_border_color(s_media_obj, lv_color_hex(0x3A404A), 0);
    lv_obj_set_style_pad_all(s_media_obj, 0, 0);
    lv_obj_set_style_shadow_width(s_media_obj, 0, 0);
    lv_obj_clear_flag(s_media_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_media_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_media_obj, LV_OBJ_FLAG_IGNORE_LAYOUT);

    s_media_icon = lv_label_create(s_media_obj);
    lv_obj_set_size(s_media_icon, 30, 24);
    lv_obj_set_pos(s_media_icon, 14, 10);
    lv_obj_set_style_text_color(s_media_icon, lv_color_hex(MEDIA_OSD_BAR_FG), 0);
    lv_obj_set_style_text_font(s_media_icon, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_align(s_media_icon, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_media_icon, LV_SYMBOL_VOLUME_MAX);

    lv_font_t *font = launcher_fonts().get("AlibabaPuHuiTi-3-55-Regular.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD);

    s_media_title = lv_label_create(s_media_obj);
    lv_obj_set_size(s_media_title, 76, 18);
    lv_obj_set_pos(s_media_title, 48, 12);
    lv_obj_set_style_text_color(s_media_title, lv_color_hex(MEDIA_OSD_TEXT), 0);
    lv_obj_set_style_text_font(s_media_title, font, 0);
    lv_label_set_long_mode(s_media_title, LV_LABEL_LONG_CLIP);
    lv_label_set_text(s_media_title, "");

    s_media_value = lv_label_create(s_media_obj);
    lv_obj_set_size(s_media_value, 48, 18);
    lv_obj_set_pos(s_media_value, 124, 12);
    lv_obj_set_style_text_color(s_media_value, lv_color_hex(MEDIA_OSD_TEXT), 0);
    lv_obj_set_style_text_font(s_media_value, font, 0);
    lv_obj_set_style_text_align(s_media_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_media_value, "");

    s_media_bar = lv_bar_create(s_media_obj);
    lv_obj_set_size(s_media_bar, 154, 10);
    lv_obj_set_pos(s_media_bar, 18, 50);
    lv_bar_set_range(s_media_bar, 0, 100);
    lv_obj_set_style_radius(s_media_bar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(s_media_bar, 5, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_media_bar, lv_color_hex(MEDIA_OSD_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_media_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_media_bar, lv_color_hex(MEDIA_OSD_BAR_FG), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_media_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    lv_obj_add_flag(s_media_obj, LV_OBJ_FLAG_HIDDEN);
}

static void show_media_bar(const char *title, const char *icon, int pct)
{
    ensure_media_created();
    if (!s_media_obj || !s_media_icon || !s_media_title || !s_media_value || !s_media_bar) return;

    pct = clamp_percent(pct);
    lv_label_set_text(s_media_icon, icon);
    lv_label_set_text(s_media_title, title);
    lv_label_set_text_fmt(s_media_value, "%d%%", pct);
    lv_bar_set_value(s_media_bar, pct, LV_ANIM_OFF);

    lv_obj_set_size(s_media_icon, 30, 24);
    lv_obj_set_pos(s_media_icon, 14, 10);
    lv_obj_set_pos(s_media_title, 48, 12);
    lv_obj_set_pos(s_media_value, 124, 12);
    lv_obj_clear_flag(s_media_value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_media_bar, LV_OBJ_FLAG_HIDDEN);

    lv_obj_center(s_media_obj);
    lv_obj_move_foreground(s_media_obj);
    lv_obj_clear_flag(s_media_obj, LV_OBJ_FLAG_HIDDEN);

    if (s_media_timer == NULL) {
        s_media_timer = lv_timer_create(media_timer_cb, MEDIA_OSD_SHOW_MS, NULL);
    }
    if (s_media_timer) {
        lv_timer_set_period(s_media_timer, MEDIA_OSD_SHOW_MS);
        lv_timer_reset(s_media_timer);
        lv_timer_resume(s_media_timer);
    }
}

static void show_mute_osd(bool muted)
{
    ensure_media_created();
    if (!s_media_obj || !s_media_icon || !s_media_title || !s_media_value || !s_media_bar) return;

    lv_label_set_text(s_media_icon, muted ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
    lv_label_set_text(s_media_title, muted ? "Muted" : "Sound On");
    lv_obj_set_size(s_media_icon, 60, 32);
    lv_obj_set_pos(s_media_icon, 65, 12);
    lv_obj_set_pos(s_media_title, 55, 50);
    lv_obj_add_flag(s_media_value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_media_bar, LV_OBJ_FLAG_HIDDEN);

    lv_obj_center(s_media_obj);
    lv_obj_move_foreground(s_media_obj);
    lv_obj_clear_flag(s_media_obj, LV_OBJ_FLAG_HIDDEN);

    if (s_media_timer == NULL) {
        s_media_timer = lv_timer_create(media_timer_cb, MEDIA_OSD_SHOW_MS, NULL);
    }
    if (s_media_timer) {
        lv_timer_set_period(s_media_timer, MEDIA_OSD_SHOW_MS);
        lv_timer_reset(s_media_timer);
        lv_timer_resume(s_media_timer);
    }
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

static void ensure_screenshot_dir(const char *scr_dir)
{
#ifdef _WIN32
    _mkdir(scr_dir);
#else
    /* Ensure dir exists with correct ownership (real uid/gid, not root). */
    mkdir(scr_dir, 0755);
    if (getuid() == 0) {
        /* Running as root via systemd — chown to the login user. */
        uid_t uid = 1000;
        gid_t gid = 1000;
        const char *sudo_uid = getenv("SUDO_UID");
        const char *sudo_gid = getenv("SUDO_GID");
        if (sudo_uid) uid = (uid_t)atoi(sudo_uid);
        if (sudo_gid) gid = (gid_t)atoi(sudo_gid);
        chown(scr_dir, uid, gid);
    }
#endif
}

namespace ui_global_hint {

void on_key(const struct key_item *elm)
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

    if (elm->key_state == KBD_KEY_PRESSED || elm->key_state == KBD_KEY_REPEATED) {
        switch (code) {
            case KEY_BRIGHTNESSUP:
            case KEY_BRIGHTNESSDOWN: {
                int pct = s_brightness_pct >= 0 ? s_brightness_pct : read_brightness_percent();
                pct += (code == KEY_BRIGHTNESSUP) ? MEDIA_OSD_STEP : -MEDIA_OSD_STEP;
                pct = write_brightness_percent(pct);
                show_media_bar("Brightness", LV_SYMBOL_TINT, pct);
                return;
            }

            case KEY_VOLUMEUP:
            case KEY_VOLUMEDOWN: {
                int pct = s_volume_pct >= 0 ? s_volume_pct : read_volume_percent();
                pct += (code == KEY_VOLUMEUP) ? MEDIA_OSD_STEP : -MEDIA_OSD_STEP;
                pct = write_volume_percent(pct);
                show_media_bar("Volume", pct == 0 ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX, pct);
                return;
            }

            case KEY_MUTE:
                if (elm->key_state == KBD_KEY_PRESSED) {
                    show_mute_osd(toggle_system_mute());
                }
                return;

            default:
                break;
        }
    }

    /* All non-media keys: only fire on the initial key-down edge. */
    if (elm->key_state != KBD_KEY_PRESSED) return;

    /* Ctrl+Alt+S: global screenshot → ~/Screenshots */
    if (code == KEY_S && (elm->mods & KBD_MOD_CTRL) && (elm->mods & KBD_MOD_ALT)) {
        const char *home = getenv("HOME");
        char scr_dir[256];
        snprintf(scr_dir, sizeof(scr_dir), "%s/Screenshots", home ? home : "/tmp");
        ensure_screenshot_dir(scr_dir);
        int ret = -1;
        cp0_signal_screenshot_api({"Save", scr_dir}, [&](int code, std::string) {
            ret = code;
        });
        show_hint(ret == 0 ? "Saved to ~/Screenshots" : "Screenshot failed");
        return;
    }

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

} // namespace ui_global_hint

extern "C" void ui_global_hint_on_key(const struct key_item *elm)
{
    ui_global_hint::on_key(elm);
}
