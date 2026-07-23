/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_launch_page.h"

#include "hal_lvgl_bsp.h"
#include "input_keys.h"
#include "keyboard_input.h"
#include "sample_log.h"

#undef SLOGI
#define SLOGI(...) do { } while (0)

namespace {

// ============================================================
// audio
// ============================================================

static void audio_play_switch(void)
{
    cp0_signal_audio_api({"SystemSoundPlay", "1"}, nullptr);
}

static void audio_play_enter(void)
{
    cp0_signal_audio_api({"SystemSoundPlay", "2"}, nullptr);
}

static uint32_t fzxc_to_arrow(uint32_t key)
{
    switch (key)
    {
    case KEY_F:
        return KEY_UP;

    case KEY_X:
        return KEY_DOWN;

    case KEY_Z:
        return KEY_LEFT;

    case KEY_C:
        return KEY_RIGHT;

    default:
        return key;
    }
}

static UILaunchPage *page_from_event(lv_event_t *event)
{
    return event ? static_cast<UILaunchPage *>(lv_event_get_user_data(event)) : nullptr;
}

} // namespace

void UILaunchPage::handle_home_key(lv_event_t *event)
{
    if (!event)
        return;

    struct key_item *elm = static_cast<struct key_item *>(lv_event_get_param(event));
    if (!elm)
        return;

    uint32_t code = fzxc_to_arrow(elm->key_code);

    SLOGI("[LAUNCHER] main_key_switch raw=%u->code=%u state=%s sym=%s",
           elm->key_code,
           code,
           kbd_state_name(elm->key_state),
           elm->sym_name);

    if (elm->key_state)
    {
        switch (code)
        {
        case KEY_UP:
            break;

        case KEY_DOWN:
            break;

        case KEY_LEFT:
        {
            if (navigation_.keyboard_navigation_enabled())
            {
                audio_play_switch();
                switch_right();
            }
        }
        break;

        case KEY_RIGHT:
        {
            if (navigation_.keyboard_navigation_enabled())
            {
                audio_play_switch();
                switch_left();
            }
        }
        break;

        default:
            break;
        }
    }
    else if (code == KEY_ENTER)
    {
        audio_play_enter();
        launch_selected_app();
    }
    else if (code == KEY_F12)
    {
        if (!navigation_.diagnostic_overlay_visible())
        {
            lv_obj_t *overlay = lv_obj_create(lv_scr_act());
            if (!overlay) return;
            green_bg_ = overlay;
            lv_obj_add_event_cb(green_bg_, on_owned_obj_deleted, LV_EVENT_DELETE, this);
            lv_obj_set_size(green_bg_, 320, 170);
            lv_obj_align(green_bg_, LV_ALIGN_TOP_LEFT, 0, 0);

            lv_obj_set_style_bg_color(green_bg_, lv_color_hex(0x00FF00), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(green_bg_, LV_OPA_COVER, LV_PART_MAIN);

            lv_obj_set_style_border_width(green_bg_, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(green_bg_, 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(green_bg_, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(green_bg_, 0, LV_PART_MAIN);
            navigation_.set_diagnostic_overlay_visible(true);
        }
        else
        {
            navigation_.set_diagnostic_overlay_visible(false);
            if (green_bg_) {
                lv_obj_del(green_bg_);
                green_bg_ = nullptr;
            }
        }
    }
}

void UILaunchPage::on_left_arrow_clicked(lv_event_t *event) noexcept
{
    try {
        if (UILaunchPage *self = page_from_event(event)) self->switch_right();
    } catch (...) {
    }
}

void UILaunchPage::on_right_arrow_clicked(lv_event_t *event) noexcept
{
    try {
        if (UILaunchPage *self = page_from_event(event)) self->switch_left();
    } catch (...) {
    }
}

void UILaunchPage::on_app_clicked(lv_event_t *event) noexcept
{
    try {
        if (UILaunchPage *self = page_from_event(event)) self->launch_selected_app();
    } catch (...) {
    }
}

void UILaunchPage::on_home_key(lv_event_t *event) noexcept
{
    try {
        if (UILaunchPage *self = page_from_event(event)) self->handle_home_key(event);
    } catch (...) {
    }
}
