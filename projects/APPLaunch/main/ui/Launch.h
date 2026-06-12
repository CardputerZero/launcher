/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl/lvgl.h"
#include <memory>

class LaunchImpl;
class UILaunchPage;

class Launch
{
public:
    Launch();
    ~Launch();

    void bind_ui();
    void set_launch_page(std::shared_ptr<UILaunchPage> launch_page);
    void update_left_slot(lv_obj_t *panel, lv_obj_t *label);
    void update_right_slot(lv_obj_t *panel, lv_obj_t *label);
    void launch_app();

private:
    std::unique_ptr<LaunchImpl> impl_;
    std::shared_ptr<UILaunchPage> launch_page_;
};
