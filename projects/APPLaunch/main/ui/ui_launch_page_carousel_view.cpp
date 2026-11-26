/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_launch_page.h"

#include "animation/launcher_grid_layout.h"
#include "animation/ui_launcher_animation.h"
#include "cp0_enum_cast.h"
#include "launcher_platform.hpp"
#include "ui.h"

#include <algorithm>
#include <cstdint>
#include <string>

extern "C" {
LV_FONT_DECLARE(launcher_montserrat_bold_latin_16)
}

const lv_font_t *launcher_ui::grid_title_font()
{
    return launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD,
                                LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                &launcher_montserrat_bold_latin_16);
}

namespace {

using launcher_grid_layout::Cell;
using launcher_grid_layout::kCellLayout;

static_assert(UILaunchPage::kLauncherGridElementCount ==
              launcher_grid_layout::kCells * 2);

// LVGL's software scaler limits the transformed image to
// ((source_size - 1) * scale) >> 8.  The usual floor(source/target) scale
// can therefore leave the final destination row/column untouched.  Round up
// against the last source pixel; the clip object trims the resulting edge.
void compensate_image_scale(lv_obj_t *image)
{
    if (!image) return;
    lv_obj_update_layout(image);
    const int32_t source_width = lv_image_get_src_width(image);
    const int32_t source_height = lv_image_get_src_height(image);
    const int32_t target_width = lv_obj_get_width(image);
    const int32_t target_height = lv_obj_get_height(image);
    if (source_width <= 1 || source_height <= 1 || target_width <= 1 || target_height <= 1)
        return;

    constexpr uint32_t scale_unit = LV_SCALE_NONE;
    const uint32_t scale_x = static_cast<uint32_t>(
        (static_cast<uint64_t>(target_width - 1) * scale_unit + source_width - 2) /
        (source_width - 1));
    const uint32_t scale_y = static_cast<uint32_t>(
        (static_cast<uint64_t>(target_height - 1) * scale_unit + source_height - 2) /
        (source_height - 1));
    // Keep manual compensation centred on the source so resampling stays
    // symmetric while the page slide moves the grid.
    lv_image_set_pivot(image, source_width / 2, source_height / 2);
    lv_image_set_scale_x(image, scale_x);
    lv_image_set_scale_y(image, scale_y);
}

lv_obj_t *create_grid_card(lv_obj_t *parent, const Cell &cell, uint32_t border_color,
                           bool clickable)
{
    lv_obj_t *card = lv_obj_create(parent);
    if (!card) return nullptr;
    lv_obj_set_size(card, launcher_grid_layout::kCardSize, launcher_grid_layout::kCardSize);
    lv_obj_set_pos(card, cell.card_x, cell.card_y);
    lv_obj_set_align(card, LV_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    if (!clickable)
        lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(card, launcher_grid_layout::kCardRadius,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(card, lv_color_hex(border_color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_post(card, true, LV_PART_MAIN | LV_STATE_DEFAULT);
    return card;
}

lv_obj_t *create_grid_title(lv_obj_t *parent, const Cell &cell, const char *text)
{
    lv_obj_t *title = lv_label_create(parent);
    if (!title) return nullptr;
    lv_obj_set_width(title, launcher_grid_layout::kTitleWidth);
    lv_obj_set_pos(title, cell.title_x, cell.title_y);
    lv_obj_set_align(title, LV_ALIGN_CENTER);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, launcher_ui::grid_title_font(),
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    launcher_home_animation::set_title_text(title, text);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(title, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    return title;
}

lv_obj_t *create_grid_arrow(lv_obj_t *parent, lv_coord_t x, const char *asset)
{
    lv_obj_t *button = lv_btn_create(parent);
    if (!button) return nullptr;
    lv_obj_set_size(button, 17, 23);
    lv_obj_set_pos(button, x, -4);
    lv_obj_set_align(button, LV_ALIGN_CENTER);
    lv_obj_add_flag(button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(button, launcher_platform::path_c(asset), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(button, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(button, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    return button;
}

} // namespace

void UILaunchPage::set_panel_icon(lv_obj_t *panel, const std::string &src)
{
    if (!panel)
        return;

    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *clip = lv_obj_get_child(panel, 0);
    lv_obj_t *image = clip ? lv_obj_get_child(clip, 0) : nullptr;
    if (src.empty()) {
        if (image && lv_obj_check_type(image, &lv_image_class))
            lv_image_set_src(image, nullptr);
        return;
    }
    if (!clip || !image || !lv_obj_check_type(image, &lv_image_class)) {
        clip = lv_obj_create(panel);
        if (!clip) return;
        lv_obj_set_size(clip, LV_PCT(100), LV_PCT(100));
        lv_obj_set_align(clip, LV_ALIGN_CENTER);
        lv_obj_clear_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
        image = lv_image_create(clip);
        if (!image) return;
        lv_obj_set_size(image, LV_PCT(100), LV_PCT(100));
        lv_obj_set_align(image, LV_ALIGN_CENTER);
        // The pool already provides a buffer sized for the card content. Use
        // CENTER so the scale is supplied by compensate_image_scale rather
        // than LVGL's floor-based STRETCH transform.
        lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CENTER);
    }

    // Clip the image in a child container. LVGL applies clip_corner to an
    // object's children, not to the object's own draw operation.
    const auto selector = LV_PART_MAIN | LV_STATE_DEFAULT;
    const lv_coord_t card_radius = lv_obj_get_style_radius(panel, LV_PART_MAIN);
    const lv_coord_t card_border = lv_obj_get_style_border_width(panel, LV_PART_MAIN);
    lv_obj_set_style_radius(clip, std::max<lv_coord_t>(0, card_radius - card_border), selector);
    lv_obj_set_style_clip_corner(clip, true, selector);
    lv_obj_set_style_bg_opa(clip, LV_OPA_TRANSP, selector);
    lv_obj_set_style_border_width(clip, 0, selector);
    lv_obj_set_style_pad_all(clip, 0, selector);

    const uint32_t image_size = static_cast<uint32_t>(std::max<lv_coord_t>(
        1, launcher_grid_layout::kCardSize - 2 * card_border));
    lv_image_set_src(image, home_icon_pool_.find(src, image_size));
    compensate_image_scale(image);
}

void UILaunchPage::update_grid_cell(size_t cell, const char *title, const std::string &icon)
{
    update_grid_item(panel(cell), label(cell), title, icon);
}

void UILaunchPage::update_grid_item(lv_obj_t *panel, lv_obj_t *label,
                                    const char *title, const std::string &icon)
{
    launcher_home_animation::set_title_text(label, title ? title : "");
    set_panel_icon(panel, icon);
}

void UILaunchPage::create_screen()
{
    const bool complete = grid_container_ && left_arrow_button_ && right_arrow_button_ &&
        std::all_of(grid_elements_.begin(), grid_elements_.end(),
                    [](lv_obj_t *element) { return element != nullptr; });
    if (!complete)
        create_app_container(content_container());
}

void UILaunchPage::create_app_container(lv_obj_t *parent)
{
    if (!parent)
        return;

    lv_obj_t *container = lv_obj_create(parent);
    if (!container) return;
    lv_obj_set_size(container, 320, 150);
    lv_obj_set_align(container, LV_ALIGN_CENTER);
    lv_obj_clear_flag(container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);

    std::array<lv_obj_t *, kLauncherGridElementCount> elements = {};

    for (size_t cell = 0; cell < launcher_grid_layout::kCells; ++cell) {
        const Cell &layout = kCellLayout[cell];
        elements[kTitle0 + cell] = create_grid_title(container, layout, "");
        elements[kCard0 + cell] = create_grid_card(
            container, layout, BORDER_COLOR_SIDE, true);
    }

    lv_obj_t *left_arrow = create_grid_arrow(container, -151, "carousel_left_arrow.png");
    lv_obj_t *right_arrow = create_grid_arrow(container, 150, "carousel_right_arrow.png");

    const bool complete = left_arrow && right_arrow &&
        std::all_of(elements.begin(), elements.end(), [](lv_obj_t *element) { return element != nullptr; });
    if (!complete) {
        lv_obj_delete(container);
        return;
    }
    *grid_alive_ = false;
    if (grid_container_) lv_obj_delete(grid_container_);
    grid_alive_ = std::make_shared<bool>(true);
    grid_container_ = container;
    grid_elements_ = elements;
    left_arrow_button_ = left_arrow;
    right_arrow_button_ = right_arrow;

    for (size_t cell = 0; cell < launcher_grid_layout::kCells; ++cell)
        lv_obj_add_event_cb(grid_elements_[kCard0 + cell], on_app_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(left_arrow_button_, on_left_arrow_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(right_arrow_button_, on_right_arrow_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(grid_container_, on_owned_obj_deleted, LV_EVENT_DELETE, this);
    for (lv_obj_t *element : grid_elements_)
        lv_obj_add_event_cb(element, on_owned_obj_deleted, LV_EVENT_DELETE, this);
    lv_obj_add_event_cb(left_arrow_button_, on_owned_obj_deleted, LV_EVENT_DELETE, this);
    lv_obj_add_event_cb(right_arrow_button_, on_owned_obj_deleted, LV_EVENT_DELETE, this);
    if (!home_key_registered_ && screen()) {
        lv_obj_add_event_cb(screen(), on_home_key, (lv_event_code_t)LV_EVENT_KEYBOARD, this);
        home_key_registered_ = true;
    }
    apply_grid_highlight();
}
