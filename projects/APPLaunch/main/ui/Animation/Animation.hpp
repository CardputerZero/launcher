/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl.h"

#include <functional>

class LvglAnimation {
public:
    typedef std::function<void(void *, int)> callback_t;
    typedef std::function<void(LvglAnimation *)> raw_callback_t;
    typedef std::function<void(LvglAnimation *)> finished_callback_t;

    enum Type {
        linear,
        ease_in,
        ease_out,
        ease_in_out,
        overshoot,
        bounce,
    };

    static void start(void *obj,
                      int time,
                      int start_val,
                      int end_val,
                      callback_t callback,
                      finished_callback_t finished = nullptr,
                      Type type = overshoot)
    {
        LvglAnimation *self = new LvglAnimation(time, type);
        self->callback_ = std::move(callback);
        self->finished_callback_ = std::move(finished);
        self->launch(obj, obj, start_val, end_val, false);
    }

    static void start_raw(int time,
                          raw_callback_t callback,
                          finished_callback_t finished = nullptr,
                          Type type = overshoot)
    {
        LvglAnimation *self = new LvglAnimation(time, type);
        self->raw_callback_ = std::move(callback);
        self->finished_callback_ = std::move(finished);
        self->launch(nullptr, self, 0, LV_BEZIER_VAL_MAX, true);
    }

    int32_t Animation_map(int32_t start, int32_t end)
    {
        lv_anim_t tmp;
        lv_memzero(&tmp, sizeof(tmp));
        tmp.act_time = current_progress_;
        tmp.duration = LV_BEZIER_VAL_MAX;
        tmp.start_value = 0;
        tmp.end_value = LV_BEZIER_VAL_MAX;

        int32_t curved = path_cb_(&tmp);
        return start + (int32_t)(((int64_t)(end - start) * curved) >> LV_BEZIER_VAL_SHIFT);
    }

    int32_t progress() const
    {
        return current_progress_;
    }

private:
    callback_t callback_;
    raw_callback_t raw_callback_;
    finished_callback_t finished_callback_;
    int time_;
    Type type_;
    lv_anim_path_cb_t path_cb_;
    int32_t current_progress_ = 0;
    bool raw_ = false;

    LvglAnimation(int time, Type type)
        : time_(time),
          type_(type),
          path_cb_(get_path_cb(type))
    {
    }

    void launch(void *obj, void *var, int32_t start_val, int32_t end_val, bool raw)
    {
        (void)obj;
        raw_ = raw;

        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, var);
        lv_anim_set_duration(&anim, time_);
        lv_anim_set_values(&anim, start_val, end_val);
        lv_anim_set_user_data(&anim, this);
        lv_anim_set_deleted_cb(&anim, LvglAnimation::deleted_cb);

        if (raw_) {
            lv_anim_set_custom_exec_cb(&anim, LvglAnimation::exec_cb_raw);
            lv_anim_set_path_cb(&anim, lv_anim_path_linear);
        } else {
            lv_anim_set_custom_exec_cb(&anim, LvglAnimation::exec_cb);
            lv_anim_set_path_cb(&anim, path_cb_);
        }

        lv_anim_start(&anim);
    }

    static LvglAnimation *from_anim(lv_anim_t *anim)
    {
        return static_cast<LvglAnimation *>(lv_anim_get_user_data(anim));
    }

    static int32_t path_overshoot(const lv_anim_t *anim)
    {
        int32_t t;
        if (anim->act_time >= anim->duration) {
            t = LV_BEZIER_VAL_MAX;
        } else {
            t = lv_map(anim->act_time, 0, anim->duration, 0, LV_BEZIER_VAL_MAX);
        }

        int32_t step = lv_bezier3(t, 0, 600, 1300, LV_BEZIER_VAL_MAX);
        int64_t value = (int64_t)step * (anim->end_value - anim->start_value);
        value >>= LV_BEZIER_VAL_SHIFT;
        value += anim->start_value;
        return (int32_t)value;
    }

    static lv_anim_path_cb_t get_path_cb(Type type)
    {
        switch (type) {
        case linear:
            return lv_anim_path_linear;
        case ease_in:
            return lv_anim_path_ease_in;
        case ease_out:
            return lv_anim_path_ease_out;
        case ease_in_out:
            return lv_anim_path_ease_in_out;
        case overshoot:
            return LvglAnimation::path_overshoot;
        case bounce:
            return lv_anim_path_bounce;
        default:
            return LvglAnimation::path_overshoot;
        }
    }

    static void exec_cb(lv_anim_t *anim, int32_t value)
    {
        LvglAnimation *self = from_anim(anim);
        if (self && self->callback_) {
            self->callback_(anim->var, value);
        }
    }

    static void exec_cb_raw(lv_anim_t *anim, int32_t value)
    {
        LvglAnimation *self = from_anim(anim);
        if (!self) {
            return;
        }

        self->current_progress_ = value;
        if (self->raw_callback_) {
            self->raw_callback_(self);
        }
    }

    static void deleted_cb(lv_anim_t *anim)
    {
        LvglAnimation *self = from_anim(anim);
        if (!self) {
            return;
        }

        if (self->finished_callback_) {
            self->finished_callback_(self);
        }

        delete self;
    }
};
