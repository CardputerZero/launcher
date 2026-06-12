#include "zero_lvgl_os.h"

#include "Launch.h"
#include "UILaunchPage.h"

void zero_lvgl_os::creat_display()
{
    dispp_ = lv_disp_get_default();
    theme_ = lv_theme_default_init(dispp_, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                   false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp_, theme_);
}

zero_lvgl_os::zero_lvgl_os()
{
    creat_display();
    launch_ = std::make_shared<Launch>();
    launch_page_ = std::make_shared<UILaunchPage>(launch_);
    launch_->set_launch_page(launch_page_);
    
}

zero_lvgl_os::~zero_lvgl_os() = default;
