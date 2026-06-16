/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_launcher_animation.h"

#include "animation.hpp"

#include <utility>

namespace {

constexpr int kLauncherAnimationTimeMs = 200;

struct LauncherSlot {
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
};

struct LauncherHomeAnimContext {
    lv_obj_t *items[10];
    bool to_right;
    launcher_home_animation::ReadyCallback ready_cb;
};

constexpr LauncherSlot kPanelSlots[] = {
    {-177, 4, 61, 61},
    {-99, -6, 80, 80},
    {0, -16, 100, 100},
    {99, -6, 80, 80},
    {177, 4, 61, 61},
};

constexpr LauncherSlot kLabelSlots[] = {
    {-177, 50, 0, 0},
    {-99, 50, 0, 0},
    {0, 50, 0, 0},
    {99, 50, 0, 0},
    {177, 50, 0, 0},
};

void apply_panel_slot(lv_obj_t *obj, const LauncherSlot &slot)
{
    if (!obj) {
        return;
    }

    lv_obj_set_x(obj, slot.x);
    lv_obj_set_y(obj, slot.y);
    lv_obj_set_width(obj, slot.w);
    lv_obj_set_height(obj, slot.h);
}

void apply_label_slot(lv_obj_t *obj, const LauncherSlot &slot)
{
    if (!obj) {
        return;
    }

    lv_obj_set_x(obj, slot.x);
    lv_obj_set_y(obj, slot.y);
}

void animate_panel(lv_obj_t *obj, const LauncherSlot &from, const LauncherSlot &to, LvglAnimation *anim)
{
    if (!obj) {
        return;
    }

    lv_obj_set_x(obj, anim->Animation_map(from.x, to.x));
    lv_obj_set_y(obj, anim->Animation_map(from.y, to.y));
    lv_obj_set_width(obj, anim->Animation_map(from.w, to.w));
    lv_obj_set_height(obj, anim->Animation_map(from.h, to.h));
}

void animate_label(lv_obj_t *obj, const LauncherSlot &from, const LauncherSlot &to, LvglAnimation *anim)
{
    if (!obj) {
        return;
    }

    lv_obj_set_x(obj, anim->Animation_map(from.x, to.x));
    lv_obj_set_y(obj, anim->Animation_map(from.y, to.y));
}

void animate_home(LauncherHomeAnimContext *ctx, LvglAnimation *anim)
{
    if (ctx->to_right) {
        for (int i = 0; i < 4; ++i) {
            animate_panel(ctx->items[i], kPanelSlots[i], kPanelSlots[i + 1], anim);
            animate_label(ctx->items[i + 5], kLabelSlots[i], kLabelSlots[i + 1], anim);
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            animate_panel(ctx->items[i], kPanelSlots[i], kPanelSlots[i - 1], anim);
            animate_label(ctx->items[i + 5], kLabelSlots[i], kLabelSlots[i - 1], anim);
        }
    }
}

void finish_home(LauncherHomeAnimContext *ctx)
{
    if (ctx->to_right) {
        for (int i = 0; i < 4; ++i) {
            apply_panel_slot(ctx->items[i], kPanelSlots[i + 1]);
            apply_label_slot(ctx->items[i + 5], kLabelSlots[i + 1]);
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            apply_panel_slot(ctx->items[i], kPanelSlots[i - 1]);
            apply_label_slot(ctx->items[i + 5], kLabelSlots[i - 1]);
        }
    }

    if (ctx->ready_cb) {
        ctx->ready_cb();
    }

    delete ctx;
}

void launcher_home_animate(lv_obj_t **items, bool to_right, launcher_home_animation::ReadyCallback ready_cb)
{
    auto *ctx = new LauncherHomeAnimContext{};
    ctx->to_right = to_right;
    ctx->ready_cb = ready_cb;

    for (int i = 0; i < 10; ++i) {
        ctx->items[i] = items[i];
    }

    LvglAnimation::start_raw(
        kLauncherAnimationTimeMs,
        [ctx](LvglAnimation *anim) {
            animate_home(ctx, anim);
        },
        [ctx](LvglAnimation *) {
            finish_home(ctx);
        });
}

} // namespace

namespace launcher_home_animation {

void animate_right(lv_obj_t **items, ReadyCallback ready_cb)
{
    launcher_home_animate(items, true, std::move(ready_cb));
}

void animate_left(lv_obj_t **items, ReadyCallback ready_cb)
{
    launcher_home_animate(items, false, std::move(ready_cb));
}

} // namespace launcher_home_animation
