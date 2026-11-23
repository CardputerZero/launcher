/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_launcher_animation.h"

#include "animation.hpp"
#include "launcher_carousel_layout.h"
#include "lvgl/src/misc/lv_text_private.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace {

constexpr int kLauncherAnimationTimeMs = 200;

struct LauncherHomeAnimContext {
    lv_obj_t *items[launcher_carousel_layout::kElementCount];
    bool to_right;
    launcher_home_animation::ReadyCallback ready_cb;
    launcher_home_animation::AliveCallback alive_cb;
};

lv_coord_t radius_for_slot(const launcher_carousel_layout::Slot &slot)
{
    return slot.width == 100 ? launcher_carousel_layout::kCenterCardRadius :
        launcher_carousel_layout::kSideCardRadius;
}

void sync_panel_radius(lv_obj_t *panel, lv_coord_t radius)
{
    if (!panel) return;
    const auto selector = LV_PART_MAIN | LV_STATE_DEFAULT;
    lv_obj_set_style_radius(panel, radius, selector);
    if (lv_obj_t *clip = lv_obj_get_child(panel, 0)) {
        const lv_coord_t border = lv_obj_get_style_border_width(panel, LV_PART_MAIN);
        lv_obj_set_style_radius(
            clip, std::max<lv_coord_t>(0, radius - border), selector);
    }
}

void compensate_image_scale(lv_obj_t *panel)
{
    if (!panel) return;
    lv_obj_t *clip = lv_obj_get_child(panel, 0);
    lv_obj_t *image = clip ? lv_obj_get_child(clip, 0) : nullptr;
    if (!image || !lv_obj_check_type(image, &lv_image_class)) return;

    // Percentage-sized children are resolved during layout.  Make sure the
    // scale is computed from the current animation frame's content size.
    lv_obj_update_layout(panel);
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
    lv_image_set_pivot(image, source_width / 2, source_height / 2);
    lv_image_set_scale_x(image, scale_x);
    lv_image_set_scale_y(image, scale_y);
}

void apply_panel_slot(lv_obj_t *obj, const launcher_carousel_layout::Slot &slot)
{
    if (!obj) {
        return;
    }

    lv_obj_set_x(obj, slot.x);
    lv_obj_set_y(obj, slot.y);
    lv_obj_set_width(obj, slot.width);
    lv_obj_set_height(obj, slot.height);
    compensate_image_scale(obj);
}

void apply_label_slot(lv_obj_t *obj, const launcher_carousel_layout::Slot &slot)
{
    if (!obj) {
        return;
    }

    lv_obj_set_x(obj, slot.x);
    lv_obj_set_y(obj, slot.y);
}

void animate_panel(lv_obj_t *obj, const launcher_carousel_layout::Slot &from,
                   const launcher_carousel_layout::Slot &to, LvglAnimation *anim)
{
    if (!obj) {
        return;
    }

    lv_obj_set_x(obj, anim->Animation_map(from.x, to.x));
    lv_obj_set_y(obj, anim->Animation_map(from.y, to.y));
    lv_obj_set_width(obj, anim->Animation_map(from.width, to.width));
    lv_obj_set_height(obj, anim->Animation_map(from.height, to.height));
    sync_panel_radius(obj, anim->Animation_map(radius_for_slot(from), radius_for_slot(to)));
    compensate_image_scale(obj);
}

void animate_label(lv_obj_t *obj, const launcher_carousel_layout::Slot &from,
                   const launcher_carousel_layout::Slot &to, LvglAnimation *anim)
{
    if (!obj) {
        return;
    }

    lv_obj_set_x(obj, anim->Animation_map(from.x, to.x));
    lv_obj_set_y(obj, anim->Animation_map(from.y, to.y));
}

void animate_home(LauncherHomeAnimContext *ctx, LvglAnimation *anim)
{
    if (ctx->alive_cb && !ctx->alive_cb())
        return;
    constexpr size_t title_offset = launcher_carousel_layout::kTitleOffset;
    const auto &slots = launcher_carousel_layout::kSlots;
    if (ctx->to_right) {
        for (size_t i = 0; i + 1 < launcher_carousel_layout::kPanelCount; ++i) {
            animate_panel(ctx->items[i], slots[i], slots[i + 1], anim);
            animate_label(ctx->items[i + title_offset], slots[i + title_offset],
                          slots[i + title_offset + 1], anim);
        }
    } else {
        for (size_t i = 1; i < launcher_carousel_layout::kPanelCount; ++i) {
            animate_panel(ctx->items[i], slots[i], slots[i - 1], anim);
            animate_label(ctx->items[i + title_offset], slots[i + title_offset],
                          slots[i + title_offset - 1], anim);
        }
    }
}

void finish_home(const std::shared_ptr<LauncherHomeAnimContext> &ctx)
{
    if (ctx->alive_cb && !ctx->alive_cb()) return;
    constexpr size_t title_offset = launcher_carousel_layout::kTitleOffset;
    const auto &slots = launcher_carousel_layout::kSlots;
    if (ctx->to_right) {
        for (size_t i = 0; i + 1 < launcher_carousel_layout::kPanelCount; ++i) {
            apply_panel_slot(ctx->items[i], slots[i + 1]);
            apply_label_slot(ctx->items[i + title_offset], slots[i + title_offset + 1]);
        }
    } else {
        for (size_t i = 1; i < launcher_carousel_layout::kPanelCount; ++i) {
            apply_panel_slot(ctx->items[i], slots[i - 1]);
            apply_label_slot(ctx->items[i + title_offset], slots[i + title_offset - 1]);
        }
    }

    if (ctx->ready_cb) {
        ctx->ready_cb();
    }

}

void launcher_home_animate(lv_obj_t **items, bool to_right,
                           launcher_home_animation::ReadyCallback ready_cb,
                           launcher_home_animation::AliveCallback alive_cb)
{
    auto ctx = std::make_shared<LauncherHomeAnimContext>();
    ctx->to_right = to_right;
    ctx->ready_cb = ready_cb;
    ctx->alive_cb = alive_cb;

    for (size_t i = 0; i < launcher_carousel_layout::kElementCount; ++i) {
        ctx->items[i] = items[i];
    }

    if (!LvglAnimation::start_raw(
        kLauncherAnimationTimeMs,
        [ctx](LvglAnimation *anim) {
            animate_home(ctx.get(), anim);
        },
        [ctx](LvglAnimation *) {
            finish_home(ctx);
        })) {
        finish_home(ctx);
    }
}

} // namespace

namespace launcher_home_animation {

void animate_right(lv_obj_t **items, ReadyCallback ready_cb, AliveCallback alive_cb)
{
    launcher_home_animate(items, true, std::move(ready_cb), std::move(alive_cb));
}

void animate_left(lv_obj_t **items, ReadyCallback ready_cb, AliveCallback alive_cb)
{
    launcher_home_animate(items, false, std::move(ready_cb), std::move(alive_cb));
}

namespace {

// Hold at each end before LONG_SCROLL moves left, then back right.
// LVGL only waits LV_LABEL_SCROLL_DELAY (300ms) unless this template is set.
constexpr uint32_t kTitleScrollPauseMs = 2000;

struct CarouselTitle {
    std::string full;
    std::string clipped;
    const lv_font_t *font = nullptr;
    int32_t width = 0;
    int32_t letter_space = 0;
    bool dirty = true;
};

std::string clip_title(const std::string &text, const lv_font_t *font,
                       int32_t width, int32_t letter_space)
{
    lv_text_attributes_t attributes{};
    attributes.letter_space = letter_space;
    attributes.max_width = width;
    attributes.text_flags = LV_TEXT_FLAG_BREAK_ALL;
    const size_t newline = text.find_first_of("\r\n");
    if (newline == std::string::npos &&
        lv_text_get_width(text.c_str(), text.size(), font, &attributes) <= width)
        return text;

    // Measure the actual suffix and its kerning against the preceding glyph.
    // LVGL returns and backs up to complete encoded characters.
    uint32_t prefix = lv_text_get_next_line(text.c_str(), text.size(),
                                            font, nullptr, &attributes);
    prefix = std::min<size_t>(prefix, newline);
    std::string clipped = text.substr(0, prefix) + "...";
    while (prefix && lv_text_get_width(clipped.c_str(), clipped.size(), font, &attributes) > width) {
        lv_text_encoded_prev(text.c_str(), &prefix);
        clipped.replace(prefix, std::string::npos, "...");
    }
    // Narrower than "...": keep the longest run of dots that still fits.
    while (!clipped.empty() &&
           lv_text_get_width(clipped.c_str(), clipped.size(), font, &attributes) > width)
        clipped.pop_back();
    return clipped;
}

const lv_anim_t *title_scroll_anim()
{
    static lv_anim_t anim;
    static bool ready = false;
    if (!ready) {
        lv_anim_init(&anim);
        lv_anim_set_delay(&anim, kTitleScrollPauseMs);
        lv_anim_set_reverse_delay(&anim, kTitleScrollPauseMs);
        lv_anim_set_repeat_delay(&anim, kTitleScrollPauseMs);
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
        ready = true;
    }
    return &anim;
}

CarouselTitle *carousel_title(lv_obj_t *label)
{
    auto *title = static_cast<CarouselTitle *>(lv_obj_get_user_data(label));
    if (!title) {
        title = new CarouselTitle;
        const char *current = lv_label_get_text(label);
        title->full = current ? current : "";
        lv_obj_set_user_data(label, title);
        lv_obj_add_event_cb(label, [](lv_event_t *event) {
            delete static_cast<CarouselTitle *>(lv_event_get_user_data(event));
        }, LV_EVENT_DELETE, title);
    }
    return title;
}

} // namespace

void set_title_text(lv_obj_t *label, const char *text)
{
    if (!label) return;
    if (!text) text = "";
    CarouselTitle *title = carousel_title(label);

    if (title->full != text) {
        title->full = text;
        title->dirty = true;
    }
    apply_title_overflow(label, lv_label_get_long_mode(label) == LV_LABEL_LONG_MODE_SCROLL);
}

void apply_title_overflow(lv_obj_t *label, bool selected)
{
    if (!label) return;
    CarouselTitle *title = carousel_title(label);
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    const int32_t line = font ? lv_font_get_line_height(font) : 0;
    if (line > 0 && lv_obj_get_style_height(label, LV_PART_MAIN) != line)
        lv_obj_set_height(label, line);

    // DOTS rewrites label->text in place, including while GET_SELF_SIZE runs
    // for DRAW_POST after the draw task holding that buffer was queued. Set a
    // precomputed string here instead (UI path only, never from DRAW_*).
    const auto mode = selected ? LV_LABEL_LONG_MODE_SCROLL : LV_LABEL_LONG_MODE_CLIP;
    if (selected && lv_obj_get_style_anim(label, LV_PART_MAIN) != title_scroll_anim())
        lv_obj_set_style_anim(label, title_scroll_anim(), LV_PART_MAIN);
    if (lv_label_get_long_mode(label) != mode)
        lv_label_set_long_mode(label, mode);

    if (!selected) {
        // Title slots set a pixel width before layout resolves their coordinates.
        const int32_t style_w = lv_obj_get_style_width(label, LV_PART_MAIN);
        const int32_t width = LV_COORD_IS_SPEC(style_w) ? -1 : style_w -
            lv_obj_get_style_space_left(label, LV_PART_MAIN) -
            lv_obj_get_style_space_right(label, LV_PART_MAIN);
        const int32_t letter_space = lv_obj_get_style_text_letter_space(label, LV_PART_MAIN);
        if (title->dirty || title->font != font || title->width != width ||
            title->letter_space != letter_space) {
            // No pixel width or font yet: show the name unclipped (CLIP mode).
            title->clipped = (font && width > 0)
                ? clip_title(title->full, font, width, letter_space)
                : title->full;
            title->font = font;
            title->width = width;
            title->letter_space = letter_space;
            title->dirty = false;
        }
    }

    const auto &visible = selected ? title->full : title->clipped;
    if (visible != lv_label_get_text(label))
        lv_label_set_text(label, visible.c_str());
}

} // namespace launcher_home_animation
