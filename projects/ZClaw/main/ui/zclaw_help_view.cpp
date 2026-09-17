/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "zclaw_help_view.h"

#include "zclaw_fonts.hpp"
#include "zclaw_theme.h"
#include "zclaw_widgets.h"

namespace zclaw {
namespace {

constexpr lv_coord_t kPanelWidth = 300;
constexpr lv_coord_t kPanelHeight = 160;
constexpr lv_coord_t kContentWidth = 278;
constexpr lv_coord_t kContentHeight = 108;
constexpr lv_coord_t kScrollStep = 26;

void clear_container_style(lv_obj_t *object)
{
    lv_obj_remove_style_all(object);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
}

}  // namespace

HelpView::~HelpView()
{
    if (backdrop_)
        lv_obj_del(backdrop_);
    release();
}

void HelpView::create(lv_obj_t *parent, const FontManager *fonts)
{
    if (backdrop_ || !parent || !fonts)
        return;
    fonts_ = fonts;
    backdrop_ = lv_obj_create(parent);
    clear_container_style(backdrop_);
    lv_obj_set_size(backdrop_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(backdrop_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop_, LV_OPA_70, 0);
    lv_obj_add_flag(backdrop_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(backdrop_, backdrop_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(backdrop_, backdrop_deleted, LV_EVENT_DELETE, this);

    lv_obj_t *panel = lv_obj_create(backdrop_);
    lv_obj_set_size(panel, kPanelWidth, kPanelHeight);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(theme::kBar), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(theme::kPanelLine), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    const lv_coord_t title_height = lv_font_get_line_height(fonts_->font_12());
    widgets::label(panel, "Help", 11, 6, 120, title_height,
                   fonts_->font_12(), theme::kText);
    widgets::label(panel, "KEY_HELP", 190, 7, 98, title_height,
                   fonts_->font_10(), theme::kPurple, LV_TEXT_ALIGN_RIGHT);

    lv_obj_t *separator = widgets::box(panel, 11, 27, 278, 1, theme::kPanelLine);
    lv_obj_set_style_bg_opa(separator, LV_OPA_70, 0);

    content_ = lv_obj_create(panel);
    clear_container_style(content_);
    lv_obj_set_pos(content_, 11, 32);
    lv_obj_set_size(content_, kContentWidth, kContentHeight);
    lv_obj_add_flag(content_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(content_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLL_ELASTIC |
                                                             LV_OBJ_FLAG_SCROLL_MOMENTUM |
                                                             LV_OBJ_FLAG_SCROLL_CHAIN));
    lv_obj_set_scroll_dir(content_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(content_, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(content_, lv_color_hex(theme::kPurple), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(content_, LV_OPA_70, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content_, 7, 0);

    lv_obj_t *description = lv_label_create(content_);
    lv_obj_set_width(description, 270);
    lv_obj_set_style_text_font(description, fonts_->font_10(), 0);
    lv_obj_set_style_text_color(description, lv_color_hex(theme::kMuted), 0);
    lv_label_set_long_mode(description, LV_LABEL_LONG_WRAP);
    lv_label_set_text(description,
                      "A Personal AI Assistant based on ZeroClaw, supporting multiple "
                      "LLM providers.\n\n"
                      "Enter your API Key during setup, or complete setup first and edit "
                      "the configuration file later. See the M5Stack documentation for details.");

    lv_obj_t *controls = lv_label_create(content_);
    lv_obj_set_width(controls, 270);
    lv_obj_set_style_text_font(controls, fonts_->font_10(), 0);
    lv_obj_set_style_text_color(controls, lv_color_hex(theme::kText), 0);
    lv_label_set_long_mode(controls, LV_LABEL_LONG_WRAP);
    lv_label_set_text(controls,
                      "Enter: start typing a message\n"
                      "F / X: scroll up / down\n"
                      "Tab: settings");

    widgets::label(panel, "KEY_HELP / ESC  close", 11, 143, 278,
                   lv_font_get_line_height(fonts_->font_10()),
                   fonts_->font_10(), theme::kMuted);
    hide();
}

void HelpView::show()
{
    if (!backdrop_)
        return;
    lv_obj_scroll_to_y(content_, 0, LV_ANIM_OFF);
    lv_obj_move_foreground(backdrop_);
    lv_obj_clear_flag(backdrop_, LV_OBJ_FLAG_HIDDEN);
}

void HelpView::hide()
{
    if (backdrop_ && lv_obj_is_valid(backdrop_))
        lv_obj_add_flag(backdrop_, LV_OBJ_FLAG_HIDDEN);
}

bool HelpView::visible() const
{
    return backdrop_ && lv_obj_is_valid(backdrop_) &&
           !lv_obj_has_flag(backdrop_, LV_OBJ_FLAG_HIDDEN);
}

void HelpView::scroll(int32_t direction)
{
    if (content_ && direction != 0)
        lv_obj_scroll_by_bounded(content_, 0, -direction * kScrollStep, LV_ANIM_ON);
}

void HelpView::backdrop_clicked(lv_event_t *event)
{
    HelpView *view = static_cast<HelpView *>(lv_event_get_user_data(event));
    if (view && lv_event_get_target(event) == view->backdrop_)
        view->hide();
}

void HelpView::backdrop_deleted(lv_event_t *event)
{
    HelpView *view = static_cast<HelpView *>(lv_event_get_user_data(event));
    if (view)
        view->release();
}

void HelpView::release()
{
    fonts_ = nullptr;
    backdrop_ = nullptr;
    content_ = nullptr;
}

}  // namespace zclaw
