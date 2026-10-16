/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl/lvgl.h"

#include <functional>

namespace launcher_home_animation {

using ReadyCallback = std::function<void()>;
using AliveCallback = std::function<bool()>;

void animate_right(lv_obj_t **items, ReadyCallback ready_cb, AliveCallback alive_cb);
void animate_left(lv_obj_t **items, ReadyCallback ready_cb, AliveCallback alive_cb);

// Selected titles bounce (pause, left, pause, right). Others end in "...".
void apply_title_overflow(lv_obj_t *label, bool selected);

} // namespace launcher_home_animation
