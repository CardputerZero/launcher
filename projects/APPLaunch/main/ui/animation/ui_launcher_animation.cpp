/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_launcher_animation.h"

#include "animation.hpp"
#include "lvgl/src/misc/lv_text_private.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace {

constexpr int kLauncherAnimationTimeMs = 200;

struct LauncherPageSlideContext {
    lv_obj_t *container = nullptr;
    bool forward = true;
    launcher_home_animation::ReadyCallback ready_cb;
    launcher_home_animation::AliveCallback alive_cb;
};

// The container is a 320-wide object aligned CENTER, so x is an offset from
// the centred position; sliding it slides every grid cell with it.
constexpr lv_coord_t kPageSlideStartX = 340;

void page_slide_tick(LauncherPageSlideContext *ctx, LvglAnimation *anim)
{
    if (ctx->alive_cb && !ctx->alive_cb()) return;
    const lv_coord_t start_x = ctx->forward ? kPageSlideStartX : -kPageSlideStartX;
    lv_obj_set_x(ctx->container, anim->Animation_map(start_x, 0));
}

void page_slide_finish(const std::shared_ptr<LauncherPageSlideContext> &ctx)
{
    if (ctx->alive_cb && !ctx->alive_cb()) return;
    lv_obj_set_x(ctx->container, 0);
    if (ctx->ready_cb) ctx->ready_cb();
}

} // namespace

namespace launcher_home_animation {

void animate_page_slide(lv_obj_t *container, bool forward,
                        ReadyCallback ready_cb, AliveCallback alive_cb)
{
    if (!container) {
        if (ready_cb) ready_cb();
        return;
    }
    auto ctx = std::make_shared<LauncherPageSlideContext>();
    ctx->container = container;
    ctx->forward = forward;
    ctx->ready_cb = std::move(ready_cb);
    ctx->alive_cb = std::move(alive_cb);

    const lv_coord_t start_x = forward ? kPageSlideStartX : -kPageSlideStartX;
    lv_obj_set_x(container, start_x);
    if (!LvglAnimation::start_raw(
            kLauncherAnimationTimeMs,
            [ctx](LvglAnimation *anim) { page_slide_tick(ctx.get(), anim); },
            [ctx](LvglAnimation *) { page_slide_finish(ctx); })) {
        lv_obj_set_x(container, 0);
        if (ctx->ready_cb) ctx->ready_cb();
    }
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
