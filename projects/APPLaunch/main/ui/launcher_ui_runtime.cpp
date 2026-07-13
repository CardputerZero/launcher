/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "launcher_ui_runtime.h"

#include "launch.h"
#include "ui_launch_page.h"

#include "hal_lvgl_bsp.h"
#include "launcher_platform.hpp"

void LauncherUiRuntime::create_display()
{
    fonts_ = std::make_shared<LauncherFonts>();

    dispp_ = lv_disp_get_default();
    theme_ = lv_theme_default_init(dispp_, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                   false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp_, theme_);
}

void LauncherUiRuntime::build_launcher_home()
{
    launch_page_->create_screen();
    launch_->bind_ui();
    launch_page_->init_input_group();
}

void LauncherUiRuntime::show_initial_screen()
{
#ifndef APPLAUNCH_STARTUP_ANIMATION
    launch_page_->load_home_screen();
#else
#ifdef HAL_PLATFORM_SDL
    launch_page_->load_home_screen();
#else
    bool gif_exists = false;
    cp0_signal_filesystem_api({"Exists", launcher_platform::path("logo_output.gif")}, [&](int code, std::string data) {
        gif_exists = code == 0 && data == "1";
    });
    if (gif_exists) {
        launch_page_->start_startup_gif();
    } else {
        launch_page_->load_home_screen();
    }
#endif
#endif
}

LauncherUiRuntime::LauncherUiRuntime()
{
    create_display();

    launch_ = std::make_unique<Launch>();
    launch_page_ = std::make_shared<UILaunchPage>(launch_.get());
    launch_->set_launch_page(launch_page_);
}

LauncherUiRuntime::~LauncherUiRuntime() = default;

void LauncherUiRuntime::start()
{
    build_launcher_home();
    show_initial_screen();
}
