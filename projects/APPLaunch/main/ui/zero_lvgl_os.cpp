#include "zero_lvgl_os.h"

#include "Launch.h"
#include "UILaunchPage.h"

#include <cstdio>

void zero_lvgl_os::creat_display()
{
    fonts_ = std::make_shared<LauncherFonts>();

    dispp_ = lv_disp_get_default();
    theme_ = lv_theme_default_init(dispp_, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                   false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp_, theme_);
}

void zero_lvgl_os::create_launcher_home()
{
    LV_EVENT_GET_COMP_CHILD = lv_event_register_id();

    UILaunchPage::create_screen();
    ui_info_bind();
    UILaunchPage::init_input_group();

#ifndef APPLAUNCH_STARTUP_ANIMATION
    UILaunchPage::load_home_screen();
#else
#ifdef HAL_PLATFORM_SDL
    UILaunchPage::load_home_screen();
#else
    const char *gif_path = cp0_file_path_c("logo_output.gif");
    FILE *gif_file = fopen(gif_path, "r");
    if (gif_file) {
        fclose(gif_file);
        UILaunchPage::start_startup_gif();
    } else {
        UILaunchPage::load_home_screen();
    }
#endif
#endif
}

zero_lvgl_os::zero_lvgl_os()
{
    creat_display();
    create_launcher_home();

    launch_ = std::make_shared<Launch>();
    launch_page_ = std::make_shared<UILaunchPage>(launch_);
    launch_->set_launch_page(launch_page_);
}

zero_lvgl_os::~zero_lvgl_os() = default;
