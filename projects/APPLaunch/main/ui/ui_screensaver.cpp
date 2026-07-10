/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_screensaver.h"

#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "keyboard_input.h"
#include "lvgl/lvgl.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace {

constexpr int kBlockSize = 20;
constexpr uint32_t kIdleCheckMs = 500;
constexpr uint32_t kAnimationFrameMs = 33;
constexpr int kVelocityX = 90;
constexpr int kVelocityY = 70;

constexpr uint32_t kColors[] = {
    0x00E5FF, 0xFFEA00, 0xFF3D71, 0x69F0AE,
    0xFF9100, 0xD500F9, 0x76FF03, 0xFFFFFF,
};

lv_obj_t *s_overlay = nullptr;
lv_obj_t *s_block = nullptr;
lv_timer_t *s_timer = nullptr;
uint32_t s_last_activity_tick = 0;
uint32_t s_last_frame_tick = 0;
uint32_t s_swallow_code = 0;
int32_t s_x_milli = 0;
int32_t s_y_milli = 0;
int s_velocity_x = kVelocityX;
int s_velocity_y = kVelocityY;
size_t s_color_index = 0;
bool s_active = false;
bool s_foreground = true;
bool s_swallow_active = false;

int config_get_int(const char *key, int default_value)
{
    int value = default_value;
    cp0_signal_config_api({"GetInt", key, std::to_string(default_value)},
                          [&](int code, std::string data) {
                              if (code == 0)
                                  value = std::atoi(data.c_str());
                          });
    return value;
}

int timeout_seconds()
{
    const int value = config_get_int("dark_time", 30);
    switch (value) {
    case 0:
    case 10:
    case 30:
    case 60:
    case 300:
        return value;
    default:
        return 30;
    }
}

void create_objects()
{
    if (s_overlay)
        return;

    lv_display_t *display = lv_display_get_default();
    if (!display)
        return;

    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_size(s_overlay,
                    lv_display_get_horizontal_resolution(display),
                    lv_display_get_vertical_resolution(display));
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);

    s_block = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_block);
    lv_obj_set_size(s_block, kBlockSize, kBlockSize);
    lv_obj_set_style_bg_color(s_block, lv_color_hex(kColors[0]), 0);
    lv_obj_set_style_bg_opa(s_block, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_block, 0, 0);
    lv_obj_clear_flag(s_block, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_block, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void stop_screensaver()
{
    if (s_overlay)
        lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_active)
        lv_obj_invalidate(lv_screen_active());

    s_active = false;
    s_last_frame_tick = 0;
    if (s_timer)
        lv_timer_set_period(s_timer, kIdleCheckMs);
}

void start_screensaver()
{
    create_objects();
    if (!s_overlay || !s_block)
        return;

    lv_display_t *display = lv_display_get_default();
    const int width = lv_display_get_horizontal_resolution(display);
    const int height = lv_display_get_vertical_resolution(display);
    lv_obj_set_size(s_overlay, width, height);

    s_x_milli = std::max(0, (width - kBlockSize) / 4) * 1000;
    s_y_milli = std::max(0, (height - kBlockSize) / 3) * 1000;
    s_velocity_x = kVelocityX;
    s_velocity_y = kVelocityY;
    s_color_index = 0;
    lv_obj_set_pos(s_block, s_x_milli / 1000, s_y_milli / 1000);
    lv_obj_set_style_bg_color(s_block, lv_color_hex(kColors[s_color_index]), 0);

    lv_obj_move_foreground(s_overlay);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    s_active = true;
    s_last_frame_tick = lv_tick_get();
    lv_timer_set_period(s_timer, kAnimationFrameMs);
    lv_timer_reset(s_timer);
}

void animate(uint32_t now)
{
    lv_display_t *display = lv_display_get_default();
    if (!display || !s_block)
        return;

    uint32_t elapsed = lv_tick_elaps(s_last_frame_tick);
    s_last_frame_tick = now;
    elapsed = std::min<uint32_t>(elapsed, 100);

    const int32_t max_x = std::max(0, lv_display_get_horizontal_resolution(display) - kBlockSize) * 1000;
    const int32_t max_y = std::max(0, lv_display_get_vertical_resolution(display) - kBlockSize) * 1000;
    s_x_milli += s_velocity_x * static_cast<int32_t>(elapsed);
    s_y_milli += s_velocity_y * static_cast<int32_t>(elapsed);

    bool collided = false;
    if (s_x_milli <= 0 || s_x_milli >= max_x) {
        s_x_milli = std::clamp<int32_t>(s_x_milli, 0, max_x);
        s_velocity_x = -s_velocity_x;
        collided = true;
    }
    if (s_y_milli <= 0 || s_y_milli >= max_y) {
        s_y_milli = std::clamp<int32_t>(s_y_milli, 0, max_y);
        s_velocity_y = -s_velocity_y;
        collided = true;
    }

    if (collided) {
        s_color_index = (s_color_index + 1) % (sizeof(kColors) / sizeof(kColors[0]));
        lv_obj_set_style_bg_color(s_block, lv_color_hex(kColors[s_color_index]), 0);
    }
    lv_obj_set_pos(s_block, s_x_milli / 1000, s_y_milli / 1000);
}

void timer_cb(lv_timer_t *)
{
    if (!s_foreground || LVGL_RUN_FLAGE != 1)
        return;

    const uint32_t now = lv_tick_get();
    if (s_active) {
        animate(now);
        return;
    }

    const int seconds = timeout_seconds();
    if (seconds > 0 && lv_tick_elaps(s_last_activity_tick) >= static_cast<uint32_t>(seconds) * 1000u)
        start_screensaver();
}

} // namespace

extern "C" void ui_screensaver_init(void)
{
    if (s_timer)
        return;
    create_objects();
    s_last_activity_tick = lv_tick_get();
    s_timer = lv_timer_create(timer_cb, kIdleCheckMs, nullptr);
}

extern "C" int ui_screensaver_filter_key(const struct key_item *item)
{
    if (!item)
        return 0;

    s_last_activity_tick = lv_tick_get();
    if (s_swallow_active) {
        if (item->key_code == s_swallow_code) {
            if (item->key_state == KBD_KEY_RELEASED)
                s_swallow_active = false;
            return 1;
        }
        s_swallow_active = false;
    }

    if (!s_active)
        return 0;

    stop_screensaver();
    s_swallow_code = item->key_code;
    s_swallow_active = item->key_state != KBD_KEY_RELEASED;
    return 1;
}

extern "C" void ui_screensaver_set_foreground(int foreground)
{
    s_foreground = foreground != 0;
    s_swallow_active = false;
    stop_screensaver();
    s_last_activity_tick = lv_tick_get();
}
