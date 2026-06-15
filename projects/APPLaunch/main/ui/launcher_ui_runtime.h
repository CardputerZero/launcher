/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl/lvgl.h"
#include <memory>

class Launch;
class UILaunchPage;
class LauncherFonts;

class LauncherUiRuntime
{
public:
    LauncherUiRuntime();
    ~LauncherUiRuntime();

    void start();

private:
    friend LauncherFonts &launcher_fonts();
    void create_display();
    void build_launcher_home();
    void show_initial_screen();

    lv_disp_t *dispp_ = nullptr;
    lv_theme_t *theme_ = nullptr;
    std::shared_ptr<UILaunchPage> launch_page_;
    std::shared_ptr<LauncherFonts> fonts_;
    std::unique_ptr<Launch> launch_;
};
