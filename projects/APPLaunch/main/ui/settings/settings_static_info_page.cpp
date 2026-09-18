/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "settings_static_info_page.hpp"
#include "settings_fonts.hpp"
#include "settings_storage_model.hpp"

#include "cp0_font_service.hpp"
#include "input_keys.h"
#include "keyboard_input.h"

#include <cstddef>
#include <cstdio>
#include <utility>

namespace {

#define SETTINGS_T12B_STRINGIFY_IMPL(value) #value
#define SETTINGS_T12B_STRINGIFY(value) SETTINGS_T12B_STRINGIFY_IMPL(value)

const char *build_version()
{
#ifdef LAUNCHER_VERSION_RAW
    return SETTINGS_T12B_STRINGIFY(LAUNCHER_VERSION_RAW);
#else
    return "unknown";
#endif
}

const char *build_date()
{
#ifdef LAUNCHER_BUILD_DATE_RAW
    return SETTINGS_T12B_STRINGIFY(LAUNCHER_BUILD_DATE_RAW);
#else
    return "unknown";
#endif
}

const char *build_channel()
{
#ifdef LAUNCHER_CHANNEL_RAW
    return SETTINGS_T12B_STRINGIFY(LAUNCHER_CHANNEL_RAW);
#else
    return "unknown";
#endif
}

const char *build_commit()
{
#ifdef LAUNCHER_GIT_COMMIT_RAW
    return SETTINGS_T12B_STRINGIFY(LAUNCHER_GIT_COMMIT_RAW);
#else
    return "unknown";
#endif
}

#undef SETTINGS_T12B_STRINGIFY
#undef SETTINGS_T12B_STRINGIFY_IMPL

} // namespace

LvSettingStaticInfoPage3::LvSettingStaticInfoPage3() = default;

LvSettingStaticInfoPage3::LvSettingStaticInfoPage3(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> back_callback,
    settings_t12b::about_help::Content content)
    : page_node_(page_node), content_(std::move(content))
{
    LeaveSelfPage = std::move(back_callback);
    create_ui(parent);
}

LvSettingStaticInfoPage3::~LvSettingStaticInfoPage3()
{
    if (keyboard_root_ && keyboard_event_dsc_) {
        lv_obj_remove_event_dsc(keyboard_root_, keyboard_event_dsc_);
        keyboard_event_dsc_ = nullptr;
    }
    if (lines_timer_) {
        lv_timer_delete(lines_timer_);
        lines_timer_ = nullptr;
    }
    lines_provider_ = nullptr;
    line_labels_.clear();
    if (ComponensObj) {
        lv_obj_delete(ComponensObj);
        ComponensObj = nullptr;
    }
    scroll_body_ = nullptr;
}

void LvSettingStaticInfoPage3::set_lines_provider(
    std::function<bool(std::vector<std::string> &)> provider)
{
    lines_provider_ = std::move(provider);
    if (!lines_provider_ || lines_timer_ || !ComponensObj) return;
    lines_timer_ = lv_timer_create(&LvSettingStaticInfoPage3::refresh_lines_cb, 1000, this);
}

void LvSettingStaticInfoPage3::refresh_lines_cb(lv_timer_t *timer)
{
    auto *self = static_cast<LvSettingStaticInfoPage3 *>(lv_timer_get_user_data(timer));
    if (!self || !self->lines_provider_) return;

    std::vector<std::string> lines = self->content_.lines;
    if (!self->lines_provider_(lines) || lines.size() != self->line_labels_.size()) return;

    for (std::size_t index = 0; index < lines.size(); ++index) {
        lv_obj_t *label = self->line_labels_[index];
        if (!label || lines[index] == lv_label_get_text(label)) continue;
        lv_label_set_text(label, lines[index].c_str());
    }
}

void LvSettingStaticInfoPage3::AnimateNextIn(std::function<void()> callback)
{
    if (callback) callback();
}

void LvSettingStaticInfoPage3::AnimateNextOut(std::function<void()> callback)
{
    if (callback) callback();
}

void LvSettingStaticInfoPage3::LoadNextPage() {}

void LvSettingStaticInfoPage3::LeaveNextPage()
{
    if (LeaveSelfPage) LeaveSelfPage();
}

void LvSettingStaticInfoPage3::create_ui(lv_obj_t *parent)
{
    if (!parent) return;

    ComponensObj = lv_obj_create(parent);
    if (!ComponensObj) return;

    lv_obj_set_size(ComponensObj,
                    metric(LayoutMetric::ScreenW),
                    metric(LayoutMetric::ScreenH));
    lv_obj_set_pos(ComponensObj, 0, 0);
    lv_obj_set_style_bg_color(ComponensObj, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ComponensObj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ComponensObj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ComponensObj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(ComponensObj, 0, LV_PART_MAIN);
    lv_obj_remove_flag(ComponensObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ComponensObj, LV_OBJ_FLAG_SCROLLABLE);

    add_label(content_.title,
              metric(LayoutMetric::ContentX),
              metric(LayoutMetric::TitleY),
              0x58A6FF,
              settings_fonts::sans(16, LV_FREETYPE_FONT_STYLE_BOLD),
              false);

    // The license list is much longer than the fixed display.  Keep the
    // title and footer fixed while allowing the body to scroll with the
    // directional keys.
    if (content_.lines.size() > 8) {
        scroll_body_ = lv_obj_create(ComponensObj);
        if (scroll_body_) {
            lv_obj_set_pos(scroll_body_,
                           metric(LayoutMetric::ContentX),
                           metric(LayoutMetric::LinesY) - 2);
            lv_obj_set_size(scroll_body_,
                            metric(LayoutMetric::ContentW),
                            metric(LayoutMetric::LinesBottomY) - metric(LayoutMetric::LinesY) + 2);
            lv_obj_set_style_bg_opa(scroll_body_, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(scroll_body_, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(scroll_body_, 0, LV_PART_MAIN);
            lv_obj_add_flag(scroll_body_, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_scroll_dir(scroll_body_, LV_DIR_VER);
            lv_obj_set_scrollbar_mode(scroll_body_, LV_SCROLLBAR_MODE_OFF);
            lv_obj_remove_flag(scroll_body_, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(scroll_body_,
                                &LvSettingStaticInfoPage3::scroll_event_cb,
                                LV_EVENT_SCROLL_BEGIN,
                                this);
            lv_obj_add_event_cb(scroll_body_,
                                &LvSettingStaticInfoPage3::scroll_event_cb,
                                LV_EVENT_SCROLL_END,
                                this);

            std::string body_text;
            for (std::size_t index = 0; index < content_.lines.size(); ++index) {
                if (index != 0) body_text.push_back('\n');
                body_text += content_.lines[index];
            }
            lv_obj_t *body_label = lv_label_create(scroll_body_);
            if (body_label) {
                lv_obj_set_pos(body_label, 0, 0);
                lv_obj_set_width(body_label, metric(LayoutMetric::ContentW));
                lv_label_set_long_mode(body_label, LV_LABEL_LONG_WRAP);
                lv_label_set_text(body_label, body_text.c_str());
                lv_obj_set_style_text_color(body_label, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
                lv_obj_set_style_text_font(body_label, settings_fonts::cjk_sans(13), LV_PART_MAIN);
                lv_obj_update_layout(body_label);
                lv_obj_update_layout(scroll_body_);
            }
        }
    } else {
        int y = metric(LayoutMetric::LinesY);
        for (const std::string &line : content_.lines) {
            if (y >= metric(LayoutMetric::LinesBottomY)) break;
            lv_obj_t *label = add_label(line,
                                        metric(LayoutMetric::ContentX),
                                        y,
                                        0xE0E0E0,
                                        settings_fonts::cjk_sans(12),
                                        true);
            if (!label) break;
            line_labels_.push_back(label);
            lv_obj_update_layout(label);
            y += lv_obj_get_height(label) + metric(LayoutMetric::LineGap);
        }
    }

    const lv_font_t *hint_font = settings_fonts::sans(12, LV_FREETYPE_FONT_STYLE_BOLD);
    add_label(scroll_body_ ? "UP/DOWN: scroll   ESC: back" : "ESC: back",
              metric(LayoutMetric::ContentX),
              metric(LayoutMetric::FooterY),
              0x46DC87,
              hint_font ? hint_font : settings_fonts::sans(12),
              false);
    DComponens::lvgl_bind_event(
        ComponensObj, LV_EVENT_KEY, nullptr,
        [this](lv_event_t *event) { handle_key_event(event); });
    keyboard_root_ = lv_screen_active();
    if (keyboard_root_ && LV_EVENT_KEYBOARD != 0) {
        keyboard_event_dsc_ = lv_obj_add_event_cb(
            keyboard_root_,
            &LvSettingStaticInfoPage3::keyboard_event_cb,
            static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD),
            this);
    }
}

lv_obj_t *LvSettingStaticInfoPage3::add_label(const std::string &text,
                                              int x,
                                              int y,
                                              uint32_t color,
                                              const lv_font_t *font,
                                              bool wrap)
{
    lv_obj_t *label = lv_label_create(ComponensObj);
    if (!label) return nullptr;
    lv_label_set_text(label, text.c_str());
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    if (font) lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    if (wrap) {
        lv_obj_set_width(label, metric(LayoutMetric::ContentW));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    }
    return label;
}

void LvSettingStaticInfoPage3::handle_key_event(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_KEY) return;
    const uint32_t key = lv_event_get_key(event);
    const bool page_up = key == LV_KEY_PREV || key == KEY_PAGEUP;
    const bool page_down = key == LV_KEY_NEXT || key == KEY_PAGEDOWN;
    // LVGL's positive scroll delta moves the child down, revealing earlier
    // text; therefore its sign is opposite to the key direction.
    if (key == LV_KEY_ESC || key == LV_KEY_LEFT) {
        if (LeaveSelfPage) LeaveSelfPage();
        lv_event_stop_processing(event);
    } else if (scroll_body_ && scroll_animation_active_ &&
               (key == LV_KEY_UP || key == LV_KEY_DOWN || page_up || page_down ||
                key == LV_KEY_HOME || key == LV_KEY_END)) {
        // Keep a second navigation event from replacing the current scroll
        // animation.  The scroll-end event releases this lock.
        lv_event_stop_processing(event);
    } else if (scroll_body_ && key == LV_KEY_UP) {
        lv_obj_scroll_by_bounded(scroll_body_, 0, 72, LV_ANIM_ON);
        lv_event_stop_processing(event);
    } else if (scroll_body_ && key == LV_KEY_DOWN) {
        lv_obj_scroll_by_bounded(scroll_body_, 0, -72, LV_ANIM_ON);
        lv_event_stop_processing(event);
    } else if (scroll_body_ && page_up) {
        lv_obj_scroll_by_bounded(scroll_body_, 0, lv_obj_get_height(scroll_body_), LV_ANIM_ON);
        lv_event_stop_processing(event);
    } else if (scroll_body_ && page_down) {
        lv_obj_scroll_by_bounded(scroll_body_, 0, -lv_obj_get_height(scroll_body_), LV_ANIM_ON);
        lv_event_stop_processing(event);
    } else if (scroll_body_ && key == LV_KEY_HOME) {
        lv_obj_scroll_to_y(scroll_body_, 0, LV_ANIM_ON);
        lv_event_stop_processing(event);
    } else if (scroll_body_ && key == LV_KEY_END) {
        lv_obj_scroll_to_y(scroll_body_, lv_obj_get_scroll_bottom(scroll_body_), LV_ANIM_ON);
        lv_event_stop_processing(event);
    }
}

void LvSettingStaticInfoPage3::keyboard_event_cb(lv_event_t *event)
{
    if (!event) return;

    auto *self = static_cast<LvSettingStaticInfoPage3 *>(lv_event_get_user_data(event));
    auto *item = static_cast<const key_item *>(lv_event_get_param(event));
    if (!self || !item || item->key_state != KBD_KEY_REPEATED) return;
    // The shared Cardputer keyboard reports the physical F/X keys for
    // navigation, so repeated navigation is matched by key_code.
    switch (item->key_code) {
    case KEY_F:
    case KEY_UP:
        self->handle_repeated_key(KEY_UP);
        break;
    case KEY_X:
    case KEY_DOWN:
        self->handle_repeated_key(KEY_DOWN);
        break;
    case KEY_PAGEUP:
    case KEY_PREVIOUS:
        self->handle_repeated_key(KEY_PAGEUP);
        break;
    case KEY_PAGEDOWN:
    case KEY_NEXT:
        self->handle_repeated_key(KEY_PAGEDOWN);
        break;
    default:
        return;
    }
    if (self->scroll_body_) {
        lv_event_stop_processing(event);
    }
}

void LvSettingStaticInfoPage3::scroll_event_cb(lv_event_t *event)
{
    if (!event) return;
    auto *self = static_cast<LvSettingStaticInfoPage3 *>(lv_event_get_user_data(event));
    if (!self) return;
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_SCROLL_BEGIN) {
        self->scroll_animation_active_ = true;
    } else if (code == LV_EVENT_SCROLL_END) {
        self->scroll_animation_active_ = false;
    }
}

void LvSettingStaticInfoPage3::handle_repeated_key(uint32_t key)
{
    if (!scroll_body_) return;
    // A tap starts an LVGL scroll animation.  Do not let a repeat event
    // cancel and replace that animation while it is still running.
    if (scroll_animation_active_) return;
    if (key == KEY_UP) {
        lv_obj_scroll_by_bounded(scroll_body_, 0, 72, LV_ANIM_ON);
    } else if (key == KEY_DOWN) {
        lv_obj_scroll_by_bounded(scroll_body_, 0, -72, LV_ANIM_ON);
    } else if (key == KEY_PAGEUP || key == KEY_PREVIOUS) {
        lv_obj_scroll_by_bounded(scroll_body_, 0, lv_obj_get_height(scroll_body_), LV_ANIM_ON);
    } else if (key == KEY_PAGEDOWN || key == KEY_NEXT) {
        lv_obj_scroll_by_bounded(scroll_body_, 0, -lv_obj_get_height(scroll_body_), LV_ANIM_ON);
    } else {
        return;
    }
}

std::unique_ptr<DComponens::LvglComponensBase> settings_t12b_about_page_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> on_back)
{
    return std::make_unique<LvSettingStaticInfoPage3>(
        parent,
        page_node,
        std::move(on_back),
        settings_t12b::about_help::about(
            build_version(), build_date(), build_channel(), build_commit()));
}

std::unique_ptr<DComponens::LvglComponensBase> settings_storage_page_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> on_back)
{
    const SettingsStorageInfo storage = SettingsStorageModel::read();
    settings_t12b::about_help::Content content{"Storage", {}};
    if (storage.valid) {
        content.lines = {
            "SD card",
            "Total: " + SettingsStorageModel::format_bytes(storage.total_bytes),
            "Available: " + SettingsStorageModel::format_bytes(storage.available_bytes),
        };
    } else {
        content.lines = {"SD card information unavailable."};
    }
    return std::make_unique<LvSettingStaticInfoPage3>(
        parent, page_node, std::move(on_back), std::move(content));
}

std::unique_ptr<DComponens::LvglComponensBase> settings_credit_page_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> on_back)
{
    return std::make_unique<LvSettingStaticInfoPage3>(
        parent, page_node, std::move(on_back), settings_t12b::about_help::credit());
}
