#include "ui_launcher_animation.h"

#include "Animation.hpp"

extern "C" int Animation_time;

namespace {

struct LauncherSlot {
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
};

struct LauncherHomeAnimContext {
    lv_obj_t *items[10];
    bool to_right;
    launcher_anim_ready_cb_t ready_cb;
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
        ctx->ready_cb(nullptr);
    }

    delete ctx;
}

void launcher_home_animate(lv_obj_t **items, bool to_right, launcher_anim_ready_cb_t ready_cb)
{
    auto *ctx = new LauncherHomeAnimContext{};
    ctx->to_right = to_right;
    ctx->ready_cb = ready_cb;

    for (int i = 0; i < 10; ++i) {
        ctx->items[i] = items[i];
    }

    LvglAnimation::start_raw(
        Animation_time,
        [ctx](LvglAnimation *anim) {
            animate_home(ctx, anim);
        },
        [ctx](LvglAnimation *) {
            finish_home(ctx);
        });
}

} // namespace

extern "C" void launcher_home_animate_right(lv_obj_t **items, launcher_anim_ready_cb_t ready_cb)
{
    launcher_home_animate(items, true, ready_cb);
}

extern "C" void launcher_home_animate_left(lv_obj_t **items, launcher_anim_ready_cb_t ready_cb)
{
    launcher_home_animate(items, false, ready_cb);
}
