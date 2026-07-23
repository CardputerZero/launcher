/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_launcher_animation.h"

#include "animation.hpp"
#include "launcher_carousel_layout.h"

#include <memory>
#include <utility>

namespace {

constexpr int kLauncherAnimationTimeMs = 200;

struct LauncherHomeAnimContext {
    lv_obj_t *items[launcher_carousel_layout::kElementCount];
    bool to_right;
    launcher_home_animation::ReadyCallback ready_cb;
    launcher_home_animation::AliveCallback alive_cb;
};

void apply_panel_slot(lv_obj_t *obj, const launcher_carousel_layout::Slot &slot)
{
    if (!obj) {
        return;
    }

    lv_obj_set_x(obj, slot.x);
    lv_obj_set_y(obj, slot.y);
    lv_obj_set_width(obj, slot.width);
    lv_obj_set_height(obj, slot.height);
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

} // namespace launcher_home_animation
