/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_power_menu.h"

#include "ui.h"
#include "ui_screensaver.h"
#include "launcher_toast.h"
#include "input_keys.h"
#include "keyboard_input.h"
#include "model/power_menu_model.hpp"
#include "sample_log.h"

#include <set>

namespace ui_power_menu {
namespace {

constexpr lv_coord_t kPanelWidth = 200;
constexpr lv_coord_t kPanelHeight = 158;
constexpr lv_coord_t kPanelPad = 8;
constexpr lv_coord_t kRowWidth = kPanelWidth - 2 * kPanelPad;
constexpr lv_coord_t kRowHeight = 28;
constexpr lv_coord_t kRowTop = 24;
constexpr lv_coord_t kRowPitch = 30;
constexpr lv_coord_t kOkTop = 58;

constexpr uint32_t kPanelColor = 0x171717;
constexpr uint32_t kPanelBorderColor = 0x3A3A3A;
constexpr uint32_t kSelectedColor = 0x2E6FD8;
constexpr uint32_t kTextColor = 0xFFFFFF;
constexpr uint32_t kDimTextColor = 0xB0B0B0;

/* If the system is still running this long after a reboot/shutdown request
 * (the simulator backends only log it), give the screen back. */
constexpr uint32_t kPowerActionFallbackMs = 10000;

constexpr const char *kFontFile = "AlibabaPuHuiTi-3-55-Regular.ttf";

PowerMenuModel model;
lv_obj_t *overlay = nullptr;
lv_obj_t *panel = nullptr;
lv_obj_t *title_label = nullptr;
lv_obj_t *rows[PowerMenuModel::kItemCount] = {};
lv_obj_t *ok_row = nullptr;
lv_obj_t *hint_label = nullptr;
lv_timer_t *fallback_timer = nullptr;
PowerMenuItem pending_power_action = PowerMenuItem::Cancel;
bool halting = false;

bool filter_installed = false;
cp0_keyboard_key_filter_t previous_key_filter = nullptr;
/* Keys pressed while the menu owned input; their repeats and releases are
 * swallowed even after the menu closes, so e.g. the release of the Enter that
 * chose Cancel cannot launch the selected home app. */
std::set<uint32_t> swallowed_keys;

void play_switch_sound() noexcept
{
    try {
        cp0_signal_audio_api({"SystemSoundPlay", "1"}, nullptr);
    } catch (...) {
    }
}

void play_enter_sound() noexcept
{
    try {
        cp0_signal_audio_api({"SystemSoundPlay", "2"}, nullptr);
    } catch (...) {
    }
}

void forget_objects()
{
    overlay = nullptr;
    panel = nullptr;
    title_label = nullptr;
    for (lv_obj_t *&row : rows) row = nullptr;
    ok_row = nullptr;
    hint_label = nullptr;
}

void overlay_delete_cb(lv_event_t *event) noexcept
{
    try {
        if (lv_event_get_target(event) != overlay) return;
    } catch (...) {
    }
    forget_objects();
    model.close();
}

lv_obj_t *create_label(lv_obj_t *parent, uint16_t size, lv_freetype_font_style_t style)
{
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return nullptr;
    lv_obj_set_style_text_color(label, lv_color_hex(kTextColor), 0);
    lv_obj_set_style_text_font(label, launcher_fonts().get(kFontFile, size, style), 0);
    return label;
}

lv_obj_t *create_row(lv_obj_t *parent, lv_coord_t y, const char *text)
{
    lv_obj_t *row = lv_obj_create(parent);
    if (!row) return nullptr;
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, kRowWidth, kRowHeight);
    lv_obj_set_pos(row, 0, y);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(kSelectedColor), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = create_label(row, 14, LV_FREETYPE_FONT_STYLE_NORMAL);
    if (label) {
        lv_label_set_text(label, text);
        lv_obj_center(label);
    }
    return row;
}

void set_row_selected(lv_obj_t *row, bool selected)
{
    if (!row) return;
    lv_obj_set_style_bg_opa(row, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_t *label = lv_obj_get_child(row, 0);
    if (label)
        lv_obj_set_style_text_color(label, lv_color_hex(selected ? kTextColor : kDimTextColor), 0);
}

bool ensure_created()
{
    if (overlay) return true;
    lv_obj_t *parent = lv_layer_top();
    if (!parent) return false;

    overlay = lv_obj_create(parent);
    if (!overlay) return false;
    lv_obj_add_event_cb(overlay, overlay_delete_cb, LV_EVENT_DELETE, nullptr);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);  // block touches to the carousel
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);

    panel = lv_obj_create(overlay);
    if (!panel) {
        lv_obj_delete(overlay);
        return false;
    }
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, kPanelWidth, kPanelHeight);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(kPanelColor), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(kPanelBorderColor), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, kPanelPad, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    title_label = create_label(panel, 16, LV_FREETYPE_FONT_STYLE_BOLD);
    if (title_label) {
        lv_obj_set_width(title_label, kRowWidth);
        lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
        lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);
    }

    for (size_t i = 0; i < PowerMenuModel::kItemCount; ++i)
        rows[i] = create_row(panel, kRowTop + static_cast<lv_coord_t>(i) * kRowPitch,
                             PowerMenuModel::label(PowerMenuModel::item_at(i)));

    ok_row = create_row(panel, kOkTop, "OK");

    hint_label = create_label(panel, 12, LV_FREETYPE_FONT_STYLE_NORMAL);
    if (hint_label) {
        lv_obj_set_style_text_color(hint_label, lv_color_hex(kDimTextColor), 0);
        lv_label_set_text(hint_label, "ESC to go back");
        lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    return true;
}

void hide_overlay()
{
    if (overlay) lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
}

void render()
{
    if (!overlay) return;
    const bool confirming = model.stage() == PowerMenuStage::Confirming;
    if (title_label)
        lv_label_set_text(title_label, confirming ? PowerMenuModel::confirm_title(model.selected()) : "Power");
    for (size_t i = 0; i < PowerMenuModel::kItemCount; ++i) {
        if (!rows[i]) continue;
        if (confirming) lv_obj_add_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        set_row_selected(rows[i], i == model.selected_index());
    }
    if (ok_row) {
        if (confirming) lv_obj_clear_flag(ok_row, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(ok_row, LV_OBJ_FLAG_HIDDEN);
        set_row_selected(ok_row, true);
    }
    if (hint_label) {
        if (confirming) lv_obj_clear_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Restores the normal dialog look after a blank/"Rebooting" screen. */
void reset_overlay_look()
{
    if (!overlay) return;
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    if (panel) lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

void fallback_timer_cb(lv_timer_t *timer) noexcept
{
    try {
        if (timer != fallback_timer) return;
        lv_timer_delete(fallback_timer);
        fallback_timer = nullptr;
        if (!halting) return;
        halting = false;
        hide_overlay();
        reset_overlay_look();
        SLOGW("[POWER] %s did not take effect", PowerMenuModel::label(pending_power_action));
        launcher_toast().show(pending_power_action == PowerMenuItem::Reboot ? "Reboot failed"
                                                                            : "Shutdown failed");
    } catch (...) {
        halting = false;
        hide_overlay();
    }
}

void start_fallback_timer()
{
    if (fallback_timer) lv_timer_delete(fallback_timer);
    fallback_timer = lv_timer_create(fallback_timer_cb, kPowerActionFallbackMs, nullptr);
}

void run_power_action(PowerMenuItem action)
{
    pending_power_action = action;
    switch (action) {
        case PowerMenuItem::Lock:
            hide_overlay();
            SLOGI("[POWER] lock requested");
            ui_screensaver_lock_now();
            return;
        case PowerMenuItem::Reboot:
            if (!overlay) return;
            halting = true;
            lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
            if (title_label) lv_label_set_text(title_label, "Rebooting...");
            for (lv_obj_t *row : rows)
                if (row) lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
            if (ok_row) lv_obj_add_flag(ok_row, LV_OBJ_FLAG_HIDDEN);
            if (hint_label) lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
            lv_refr_now(nullptr);
            SLOGI("[POWER] reboot requested");
            start_fallback_timer();
            cp0_system_reboot();
            return;
        case PowerMenuItem::Shutdown:
            if (!overlay) return;
            halting = true;
            /* Blank the whole display first so the panel goes dark cleanly. */
            lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
            if (panel) lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
            lv_refr_now(nullptr);
            SLOGI("[POWER] shutdown requested");
            start_fallback_timer();
            cp0_system_shutdown();
            return;
        case PowerMenuItem::Cancel:
        default:
            hide_overlay();
            return;
    }
}

PowerMenuKey menu_key_for(const key_item *item)
{
    const uint32_t code = item->semantic_key ? item->semantic_key : item->key_code;
    switch (code) {
        case KEY_UP:
        case KEY_LEFT:
        case KEY_F:
        case KEY_Z:
            return PowerMenuKey::Previous;
        case KEY_DOWN:
        case KEY_RIGHT:
        case KEY_X:
        case KEY_C:
            return PowerMenuKey::Next;
        case KEY_ENTER:
        case KEY_KPENTER:
            return PowerMenuKey::Enter;
        case KEY_ESC:
            return PowerMenuKey::Back;
        default:
            return PowerMenuKey::Other;
    }
}

void handle_menu_key(const key_item *item)
{
    const PowerMenuKey key = menu_key_for(item);
    const bool repeated = item->key_state == KBD_KEY_REPEATED;
    /* Only navigation auto-repeats; Enter/ESC act once per press. */
    if (repeated && key != PowerMenuKey::Previous && key != PowerMenuKey::Next) return;

    const PowerMenuDecision decision = model.handle(key);
    if (decision.navigated) play_switch_sound();
    else if (key == PowerMenuKey::Enter && (decision.redraw || decision.close)) play_enter_sound();

    if (decision.execute) {
        run_power_action(decision.action);
        return;
    }
    if (decision.close) {
        hide_overlay();
        return;
    }
    if (decision.redraw) render();
}

int key_filter(const key_item *item) noexcept
{
    try {
        if (!item) return 0;
        const auto held = swallowed_keys.find(item->key_code);
        if (held != swallowed_keys.end()) {
            if (item->key_state == KBD_KEY_RELEASED) {
                swallowed_keys.erase(held);
                return 1;
            }
            if (item->key_state != KBD_KEY_PRESSED) {
                if (!model.is_open()) return 1;
                handle_menu_key(item);  // navigation auto-repeat
                return 1;
            }
            swallowed_keys.erase(held);  // stale entry: its release went elsewhere
        }
        if (halting) return 1;  // the blank/"Rebooting" screen owns every key
        if (!model.is_open())
            return previous_key_filter ? previous_key_filter(item) : 0;

        if (item->key_state != KBD_KEY_RELEASED) swallowed_keys.insert(item->key_code);
        if (item->key_state == KBD_KEY_PRESSED) handle_menu_key(item);
        return 1;
    } catch (...) {
        model.close();
        hide_overlay();
        return 1;
    }
}

} // namespace

void init()
{
    if (filter_installed) return;
    previous_key_filter = cp0_keyboard_get_key_filter();
    cp0_keyboard_set_key_filter(key_filter);
    filter_installed = true;
}

void shutdown()
{
    if (filter_installed && cp0_keyboard_get_key_filter() == key_filter)
        cp0_keyboard_set_key_filter(previous_key_filter);
    filter_installed = false;
    previous_key_filter = nullptr;
    swallowed_keys.clear();
    if (fallback_timer) {
        lv_timer_delete(fallback_timer);
        fallback_timer = nullptr;
    }
    halting = false;
    model.close();
    if (overlay) lv_obj_delete(overlay);
    forget_objects();
}

bool open()
{
    try {
        if (!filter_installed || halting || model.is_open()) return false;
        if (!ensure_created()) return false;
        model.open();
        reset_overlay_look();
        render();
        /* The ESC that opened the menu is still down: swallow its repeats and
         * release so they neither close the menu nor reach the home screen. */
        swallowed_keys.insert(KEY_ESC);
        lv_obj_move_foreground(overlay);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        play_enter_sound();
        SLOGI("[POWER] menu opened");
        return true;
    } catch (...) {
        model.close();
        hide_overlay();
        return false;
    }
}

bool is_open()
{
    return model.is_open() || halting;
}

} // namespace ui_power_menu
