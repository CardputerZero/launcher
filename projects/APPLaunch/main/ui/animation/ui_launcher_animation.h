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

// Slide the whole grid container in from the left/right edge (a page flip).
// forward=true enters from the right edge, false from the left.  The
// container must be alive for the whole animation; alive_cb guards that.
void animate_page_slide(lv_obj_t *container, bool forward,
                        ReadyCallback ready_cb, AliveCallback alive_cb);

// Selected titles bounce (pause, left, pause, right). Others end in "...".
void set_title_text(lv_obj_t *label, const char *text);
void apply_title_overflow(lv_obj_t *label, bool selected);

} // namespace launcher_home_animation
