#include "ui_low_battery.h"

#include "low_battery_flow.hpp"
#include "lvgl/lvgl.h"

#include <string>

namespace launcher_battery_ui {
namespace {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 240;
constexpr uint32_t kFlashMs = 500;

LowBatteryFlow flow;
LowBatteryWarning rendered_warning = LowBatteryWarning::None;
lv_obj_t *overlay = nullptr;
lv_obj_t *tint = nullptr;
lv_obj_t *countdown_label = nullptr;
lv_timer_t *warning_timer = nullptr;
uint32_t flash_tick = 0;
uint32_t rendered_seconds = 0;

void create_overlay()
{
    if (overlay || !lv_layer_top())
        return;

    overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_size(overlay, kScreenWidth, kScreenHeight);
    lv_obj_clear_flag(overlay, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);

    tint = lv_obj_create(overlay);
    lv_obj_remove_style_all(tint);
    lv_obj_set_size(tint, kScreenWidth, kScreenHeight);
    lv_obj_set_style_bg_color(tint, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(tint, LV_OPA_20, 0);
    lv_obj_clear_flag(tint, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

    lv_obj_t *panel = lv_obj_create(overlay);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, 18, 73);
    lv_obj_set_size(panel, 284, 94);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x160000), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_80, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xFF3030), 0);
    lv_obj_set_style_border_width(panel, 2, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "LOW BATTERY");
    lv_obj_set_width(title, 260);
    lv_obj_set_pos(title, 12, 10);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    countdown_label = lv_label_create(panel);
    lv_obj_set_width(countdown_label, 260);
    lv_obj_set_pos(countdown_label, 12, 36);
    lv_obj_set_style_text_align(countdown_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(countdown_label, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_text_font(countdown_label, &lv_font_montserrat_14, 0);

    lv_obj_t *message = lv_label_create(panel);
    lv_label_set_text(message, "Shut down now or connect a charger.");
    lv_obj_set_width(message, 260);
    lv_obj_set_pos(message, 12, 64);
    lv_label_set_long_mode(message, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(message, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(message, &lv_font_montserrat_10, 0);

    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
}

void render(uint32_t now, bool force = false)
{
    const LowBatteryWarning warning = flow.warning();
    if (warning == LowBatteryWarning::None) {
        if (overlay)
            lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        rendered_warning = warning;
        rendered_seconds = 0;
        return;
    }

    create_overlay();
    if (!overlay)
        return;
    lv_obj_move_foreground(overlay);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    const bool warning_changed = warning != rendered_warning;
    if (warning_changed) {
        flash_tick = now;
        lv_obj_set_style_bg_opa(tint, LV_OPA_20, 0);
    }
    const uint32_t seconds = flow.seconds_until_shutdown(now);
    if (force || warning_changed || seconds != rendered_seconds) {
        const std::string text = warning == LowBatteryWarning::ShutdownCountdown
            ? "Power off in " + std::to_string(seconds) + "s"
            : "Battery below 5%";
        lv_label_set_text(countdown_label, text.c_str());
        rendered_warning = warning;
        rendered_seconds = seconds;
    }
}

void timer_cb(lv_timer_t *)
{
    const uint32_t now = lv_tick_get();
    render(now);
    if (overlay && flow.warning() != LowBatteryWarning::None &&
        lv_tick_elaps(flash_tick) >= kFlashMs) {
        flash_tick = now;
        const lv_opa_t opacity = lv_obj_get_style_bg_opa(tint, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tint, opacity == LV_OPA_20 ? LV_OPA_50 : LV_OPA_20, 0);
    }
    if (!flow.shutdown_due(now))
        return;

    const cp0_battery_info_t confirmation = cp0_battery_read();
    flow.update(confirmation.valid != 0, confirmation.soc, (confirmation.flags & 1) != 0, now);
    render(now, true);
    if (flow.confirm_shutdown(confirmation.valid != 0, (confirmation.flags & 1) != 0, now))
        cp0_system_shutdown();
}

} // namespace

void init_warning()
{
    if (warning_timer)
        return;
    flow.reset();
    warning_timer = lv_timer_create(timer_cb, 250, nullptr);
    update_warning(cp0_battery_read());
}

void update_warning(const cp0_battery_info_t &info)
{
    const uint32_t now = lv_tick_get();
    flow.update(info.valid != 0, info.soc, (info.flags & 1) != 0, now);
    render(now, true);
}

} // namespace launcher_battery_ui
