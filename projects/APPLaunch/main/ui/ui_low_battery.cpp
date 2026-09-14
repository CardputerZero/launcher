/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_low_battery.h"
#include "ui_screensaver.h"

#include "low_battery_flow.hpp"
#include "keyboard_input.h"
#include "input_keys.h"
#include "lvgl/lvgl.h"

#include <set>
#include <string>

namespace launcher_battery_ui {
namespace {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 240;
constexpr uint32_t kFlashMs = 500;
constexpr uint32_t kBatteryRefreshMs = 3000;

LowBatteryFlow flow;
LowBatteryWarning rendered_warning = LowBatteryWarning::None;
lv_obj_t *overlay = nullptr;
lv_obj_t *tint = nullptr;
lv_obj_t *panel = nullptr;
lv_obj_t *countdown_label = nullptr;
lv_obj_t *message_label = nullptr;
lv_timer_t *warning_timer = nullptr;
uint32_t flash_tick = 0;
uint32_t rendered_seconds = 0;
uint32_t battery_refresh_tick = 0;
cp0_keyboard_key_filter_t previous_key_filter = nullptr;
std::set<uint32_t> swallowed_keys;

void render(uint32_t now, bool force = false);

int reminder_key_filter(const struct key_item *item) noexcept
{
    try {
        if (!item) return 0;
        // Consume the whole key gesture, even after Enter has hidden the popup.
        const auto held = swallowed_keys.find(item->key_code);
        if (held != swallowed_keys.end()) {
            if (item->key_state == KBD_KEY_RELEASED) {
                swallowed_keys.erase(held);
                return 1;
            }
            if (item->key_state != KBD_KEY_PRESSED) return 1;
            swallowed_keys.erase(held);
        }
        if (flow.warning() == LowBatteryWarning::Reminder && overlay &&
            !lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN)) {
            if (item->key_state != KBD_KEY_RELEASED)
                swallowed_keys.insert(item->key_code);
            if (item->key_state == KBD_KEY_PRESSED &&
                (item->key_code == KEY_ENTER || item->key_code == KEY_KPENTER)) {
                flow.dismiss_reminder();
                render(lv_tick_get());
            }
            return 1;
        }
        return previous_key_filter ? previous_key_filter(item) : 0;
    } catch (...) {
        return 1;
    }
}

void restore_key_filter()
{
    if (cp0_keyboard_get_key_filter() == reminder_key_filter)
        cp0_keyboard_set_key_filter(previous_key_filter);
    previous_key_filter = nullptr;
    swallowed_keys.clear();
}

void overlay_deleted_cb(lv_event_t *event) noexcept
{
    try {
        if (lv_event_get_code(event) != LV_EVENT_DELETE ||
            lv_event_get_target(event) != lv_event_get_current_target(event) ||
            lv_event_get_target(event) != overlay)
            return;
        overlay = nullptr;
        tint = nullptr;
        panel = nullptr;
        countdown_label = nullptr;
        message_label = nullptr;
        rendered_warning = LowBatteryWarning::None;
        rendered_seconds = 0;
    } catch (...) {
        overlay = nullptr;
        tint = nullptr;
        panel = nullptr;
        countdown_label = nullptr;
        message_label = nullptr;
        rendered_warning = LowBatteryWarning::None;
        rendered_seconds = 0;
    }
}

void critical_child_deleted_cb(lv_event_t *event) noexcept
{
    try {
        if (lv_event_get_code(event) != LV_EVENT_DELETE ||
            lv_event_get_target(event) != lv_event_get_current_target(event)) return;
        lv_obj_t *deleted = static_cast<lv_obj_t *>(lv_event_get_target(event));
        if (deleted != tint && deleted != panel && deleted != countdown_label &&
            deleted != message_label) return;
        if (deleted == tint) tint = nullptr;
        if (deleted == panel) panel = nullptr;
        if (deleted == countdown_label) countdown_label = nullptr;
        if (deleted == message_label) message_label = nullptr;
        if (overlay)
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        rendered_warning = LowBatteryWarning::None;
        rendered_seconds = 0;
    } catch (...) {
        tint = nullptr;
        panel = nullptr;
        countdown_label = nullptr;
        message_label = nullptr;
        rendered_warning = LowBatteryWarning::None;
        rendered_seconds = 0;
    }
}

void create_overlay()
{
    if (overlay && tint && panel && countdown_label && message_label) return;
    if (overlay) lv_obj_delete(overlay);
    if (!lv_layer_top()) return;

    overlay = lv_obj_create(lv_layer_top());
    if (!overlay)
        return;
    lv_obj_add_event_cb(overlay, overlay_deleted_cb, LV_EVENT_DELETE, nullptr);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, kScreenWidth, kScreenHeight);
    lv_obj_clear_flag(overlay, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);

    tint = lv_obj_create(overlay);
    if (!tint) {
        lv_obj_delete(overlay);
        return;
    }
    lv_obj_add_event_cb(tint, critical_child_deleted_cb, LV_EVENT_DELETE, nullptr);
    lv_obj_remove_style_all(tint);
    lv_obj_set_size(tint, kScreenWidth, kScreenHeight);
    lv_obj_set_style_bg_color(tint, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(tint, LV_OPA_20, 0);
    lv_obj_clear_flag(tint, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

    panel = lv_obj_create(overlay);
    if (!panel) {
        lv_obj_delete(overlay);
        return;
    }
    lv_obj_add_event_cb(panel, critical_child_deleted_cb, LV_EVENT_DELETE, nullptr);
    lv_obj_remove_style_all(panel);
    lv_obj_clear_flag(panel, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_pos(panel, 18, 73);
    lv_obj_set_size(panel, 284, 94);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x160000), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_80, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xFF3030), 0);
    lv_obj_set_style_border_width(panel, 2, 0);

    lv_obj_t *title = lv_label_create(panel);
    if (!title) {
        lv_obj_delete(overlay);
        return;
    }
    lv_label_set_text(title, "LOW BATTERY");
    lv_obj_set_width(title, 260);
    lv_obj_set_pos(title, 12, 10);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    countdown_label = lv_label_create(panel);
    if (!countdown_label) {
        lv_obj_delete(overlay);
        return;
    }
    lv_obj_add_event_cb(
        countdown_label, critical_child_deleted_cb, LV_EVENT_DELETE, nullptr);
    lv_obj_set_width(countdown_label, 260);
    lv_obj_set_pos(countdown_label, 12, 36);
    lv_obj_set_style_text_align(countdown_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(countdown_label, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_text_font(countdown_label, &lv_font_montserrat_14, 0);

    message_label = lv_label_create(panel);
    if (!message_label) {
        lv_obj_delete(overlay);
        return;
    }
    lv_obj_add_event_cb(message_label, critical_child_deleted_cb, LV_EVENT_DELETE, nullptr);
    lv_obj_set_width(message_label, 260);
    lv_obj_set_pos(message_label, 12, 64);
    lv_label_set_long_mode(message_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(message_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(message_label, &lv_font_montserrat_10, 0);

    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
}

void render(uint32_t now, bool force)
{
    const LowBatteryWarning warning = flow.warning();
    if (ui_screensaver_is_active()) {
        if (overlay)
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        rendered_warning = LowBatteryWarning::None;
        rendered_seconds = 0;
        return;
    }
    if (warning == LowBatteryWarning::None || warning == LowBatteryWarning::Undefined) {
        if (overlay)
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        rendered_warning = warning;
        rendered_seconds = 0;
        return;
    }

    create_overlay();
    if (!overlay || !tint || !panel || !countdown_label || !message_label)
        return;
    lv_obj_move_foreground(overlay);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    const bool warning_changed = warning != rendered_warning;
    if (warning_changed) {
        const bool reminder = warning == LowBatteryWarning::Reminder;
        if (reminder) lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
        else lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
        flash_tick = now;
        lv_obj_set_style_bg_opa(tint, reminder ? LV_OPA_TRANSP : LV_OPA_20, 0);
        lv_obj_set_style_bg_color(panel, lv_color_hex(reminder ? 0x171717 : 0x160000), 0);
        lv_obj_set_style_bg_opa(panel, reminder ? LV_OPA_COVER : LV_OPA_80, 0);
        lv_obj_set_style_border_color(panel, lv_color_hex(reminder ? 0xFFAA00 : 0xFF3030), 0);
        lv_obj_set_style_text_color(countdown_label,
                                    lv_color_hex(reminder ? 0xFFAA00 : 0xFF4444), 0);
        lv_label_set_text(message_label, reminder ? "Connect a charger. Press Enter to close."
                                                 : "Shut down now or connect a charger.");
    }
    const uint32_t seconds = flow.seconds_until_shutdown(now);
    if (force || warning_changed || seconds != rendered_seconds) {
        const std::string text = warning == LowBatteryWarning::ShutdownCountdown
            ? "Power off in " + std::to_string(seconds) + "s"
            : warning == LowBatteryWarning::Reminder
            ? "Battery: " + std::to_string(flow.reminder_soc()) + "%"
            : "Battery below 5%";
        lv_label_set_text(countdown_label, text.c_str());
        rendered_warning = warning;
        rendered_seconds = seconds;
    }
}

void timer_cb(lv_timer_t *timer) noexcept
{
    try {
    if (timer != warning_timer) return;
    const uint32_t now = lv_tick_get();
    // Recheck the latest snapshot before committing an expired countdown.
    const bool refresh_due = flow.shutdown_due(now) ||
        static_cast<uint32_t>(now - battery_refresh_tick) >= kBatteryRefreshMs;
    if (refresh_due) {
        battery_refresh_tick = now;
        const cp0_battery_info_t info = cp0_battery_read();
        flow.update(info.valid != 0, info.soc, (info.flags & 1) != 0, now);
    }
    if (flow.take_shutdown_due(now)) {
        cp0_system_shutdown();
        return;
    }
    render(now, refresh_due);
    if (overlay && tint && !lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN) &&
        (flow.warning() == LowBatteryWarning::Low ||
         flow.warning() == LowBatteryWarning::ShutdownCountdown) &&
        lv_tick_elaps(flash_tick) >= kFlashMs) {
        flash_tick = now;
        const lv_opa_t opacity = lv_obj_get_style_bg_opa(tint, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tint, opacity == LV_OPA_20 ? LV_OPA_50 : LV_OPA_20, 0);
    }
    } catch (...) {
        if (overlay)
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

} // namespace

void init_warning()
{
    try {
    if (warning_timer)
        return;
    flow.reset();
    battery_refresh_tick = lv_tick_get();
    warning_timer = lv_timer_create(timer_cb, 250, nullptr);
    if (!warning_timer) return;
    previous_key_filter = cp0_keyboard_get_key_filter();
    cp0_keyboard_set_key_filter(reminder_key_filter);
    update_warning(cp0_battery_read());
    } catch (...) {
        restore_key_filter();
        if (warning_timer) {
            lv_timer_delete(warning_timer);
            warning_timer = nullptr;
        }
        if (overlay)
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        flow.reset();
    }
}

void update_warning(const cp0_battery_info_t &info)
{
    try {
    const uint32_t now = lv_tick_get();
    flow.update(info.valid != 0, info.soc, (info.flags & 1) != 0, now);
    render(now, true);
    } catch (...) {
        if (overlay)
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

void refresh_visibility() noexcept
{
    try {
        if (warning_timer)
            render(lv_tick_get(), true);
    } catch (...) {
        if (overlay)
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

void shutdown_warning()
{
    restore_key_filter();
    if (warning_timer) {
        lv_timer_delete(warning_timer);
        warning_timer = nullptr;
    }
    if (overlay)
        lv_obj_delete(overlay);
    overlay = nullptr;
    tint = nullptr;
    panel = nullptr;
    countdown_label = nullptr;
    message_label = nullptr;
    flow.reset();
    rendered_warning = LowBatteryWarning::None;
    rendered_seconds = 0;
    battery_refresh_tick = 0;
}

} // namespace launcher_battery_ui
