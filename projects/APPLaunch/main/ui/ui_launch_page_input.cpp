/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_launch_page.h"

#include "animation/launcher_grid_layout.h"
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

    /* Text editors own printable keys, including F/X/Z/C.  This callback is
     * installed on the active screen, so it must not let those events leak
     * into launcher navigation while an editor has selected text context. */
    if (elm->input_context == KBD_INPUT_CONTEXT_TEXT ||
        cp0_keyboard_get_input_context() == KBD_INPUT_CONTEXT_TEXT) {
        return;
    }

    uint32_t code = elm->semantic_key ? elm->semantic_key : fzxc_to_arrow(elm->key_code);

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
        {
            if (navigation_ready())
            {
                audio_play_switch();
                move_grid_up();
            }
        }
        break;

        case KEY_DOWN:
        {
            if (navigation_ready())
            {
                audio_play_switch();
                move_grid_down();
            }
        }
        break;

        case KEY_LEFT:
        {
            if (navigation_ready())
            {
                audio_play_switch();
                move_grid_previous();
            }
        }
        break;

        case KEY_RIGHT:
        {
            if (navigation_ready())
            {
                audio_play_switch();
                move_grid_next();
            }
        }
        break;

        case KEY_I:
        {
            if (navigation_ready())
            {
                audio_play_switch();
                move_grid_page_previous();
            }
        }
        break;

        case KEY_O:
        {
            if (navigation_ready())
            {
                audio_play_switch();
                move_grid_page_next();
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
}

void UILaunchPage::on_app_clicked(lv_event_t *event) noexcept
{
    try {
        UILaunchPage *self = page_from_event(event);
        if (!self) return;
        lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(event));
        for (size_t cell = 0; cell < launcher_grid_layout::kCells; ++cell) {
            if (self->grid_elements_[cell] != target) continue;
            self->select_grid_cell(cell);
            self->launch_selected_app();
            return;
        }
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
