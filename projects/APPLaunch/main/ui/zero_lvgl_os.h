#pragma once

#include "lvgl/lvgl.h"
#include <memory>

class Launch;
class UILaunchPage;
class LauncherFonts;

class zero_lvgl_os
{
public:
    zero_lvgl_os();
    ~zero_lvgl_os();

    void start();

private:
    friend LauncherFonts &launcher_fonts();
    void creat_display();
    void create_launcher_home();

    lv_disp_t *dispp_ = nullptr;
    lv_theme_t *theme_ = nullptr;
    std::shared_ptr<UILaunchPage> launch_page_;
    std::shared_ptr<LauncherFonts> fonts_;
    std::shared_ptr<Launch> launch_;
};
