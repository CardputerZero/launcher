/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * ui_darkscreen.cpp — see ui_darkscreen.h for the feature description (#72).
 */

#include "ui_darkscreen.h"

#include "lvgl/lvgl.h"
#include "keyboard_input.h"
#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"

#include <string>

/* Cached state ----------------------------------------------------------- */
static uint32_t  s_last_activity_tick = 0;   /* lv_tick_get() of last key */
static bool      s_dark              = false; /* screen currently blanked */
static int       s_saved_backlight   = -1;    /* brightness before blanking */
static lv_obj_t *s_black             = nullptr;

/* The key that woke the screen is swallowed (press + repeats + its release)
 * so it does not also trigger a UI action. */
static uint32_t  s_swallow_code   = 0;
static bool      s_swallow_active = false;

/* Tracks LVGL_RUN_FLAGE edges so we can reset the idle timer when the
 * launcher regains the foreground after a sub-app exits. */
static int       s_prev_run = 1;

static int config_get_int(const char *key, int default_val)
{
    int val = default_val;
    cp0_signal_config_api({"GetInt", key ? std::string(key) : std::string(), std::to_string(default_val)},
                          [&](int code, std::string data) {
                              if (code == 0) val = std::atoi(data.c_str());
                          });
    return val;
}

/* Configured idle timeout in seconds (0 = Never). Defaults to 30s. */
static int darkscreen_timeout_secs()
{
    return config_get_int("dark_time", 30);
}

static void ensure_black_overlay()
{
    if (s_black != nullptr) return;
    lv_obj_t *parent = lv_layer_top();
    if (parent == nullptr) return;

    s_black = lv_obj_create(parent);
    lv_obj_remove_style_all(s_black);

    lv_display_t *disp = lv_display_get_default();
    int w = disp ? lv_display_get_horizontal_resolution(disp) : 320;
    int h = disp ? lv_display_get_vertical_resolution(disp) : 170;
    lv_obj_set_size(s_black, w, h);
    lv_obj_set_pos(s_black, 0, 0);

    lv_obj_set_style_bg_color(s_black, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_black, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_black, 0, 0);
    lv_obj_set_style_radius(s_black, 0, 0);
    lv_obj_set_style_pad_all(s_black, 0, 0);

    lv_obj_clear_flag(s_black, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_black, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_black, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(s_black, LV_OBJ_FLAG_HIDDEN);
}

static void darkscreen_go_dark()
{
    if (s_dark) return;

    s_saved_backlight = cp0_backlight_read();
    if (s_saved_backlight <= 0)
        s_saved_backlight = config_get_int("brightness", -1);
    if (s_saved_backlight <= 0)
        s_saved_backlight = cp0_backlight_max();

    ensure_black_overlay();
    if (s_black) {
        lv_obj_move_foreground(s_black);
        lv_obj_clear_flag(s_black, LV_OBJ_FLAG_HIDDEN);
    }
    /* Paint the black frame before cutting the backlight so no stale UI flashes. */
    lv_refr_now(NULL);
    cp0_backlight_write(0);
    s_dark = true;
}

static void darkscreen_wake()
{
    if (!s_dark) return;

    int b = s_saved_backlight;
    if (b <= 0) b = config_get_int("brightness", -1);
    if (b <= 0) b = cp0_backlight_max();
    if (b <= 0) b = 50;
    cp0_backlight_write(b);

    if (s_black)
        lv_obj_add_flag(s_black, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(lv_screen_active());

    s_dark = false;
    s_last_activity_tick = lv_tick_get();
}

extern "C" int ui_darkscreen_filter_key(const struct key_item *elm)
{
    if (elm == nullptr) return 0;

    s_last_activity_tick = lv_tick_get();

    /* Mid-swallow: keep eating the waking key's events until it is released. */
    if (s_swallow_active) {
        if (elm->key_code == s_swallow_code) {
            if (elm->key_state == KBD_KEY_RELEASED)
                s_swallow_active = false;
            return 1;
        }
        s_swallow_active = false; /* a different key: stop swallowing */
    }

    if (s_dark) {
        darkscreen_wake();
        s_swallow_code   = elm->key_code;
        s_swallow_active = (elm->key_state != KBD_KEY_RELEASED);
        return 1;
    }
    return 0;
}

extern "C" void ui_darkscreen_tick(void)
{
    int run = LVGL_RUN_FLAGE;

    /* Just regained the foreground (a sub-app exited): restart the idle clock
     * so we don't immediately blank, and drop any pending swallow state. */
    if (run == 1 && s_prev_run != 1) {
        s_last_activity_tick = lv_tick_get();
        s_swallow_active = false;
    }
    s_prev_run = run;

    /* Only the foreground launcher blanks; leave sub-apps untouched. */
    if (run != 1) return;

    int secs = darkscreen_timeout_secs();
    if (secs <= 0) {            /* Never */
        if (s_dark) darkscreen_wake();
        return;
    }
    if (s_dark) return;

    if (s_last_activity_tick == 0)
        s_last_activity_tick = lv_tick_get();

    if (lv_tick_elaps(s_last_activity_tick) >= (uint32_t)secs * 1000u)
        darkscreen_go_dark();
}
