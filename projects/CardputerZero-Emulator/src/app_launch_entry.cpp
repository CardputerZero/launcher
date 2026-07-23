#include "lvgl/lvgl.h"
#include "ui_screensaver.h"

namespace launcher_ui {
void init();
void deinit();
}

extern "C" void ui_init(void)
{
    launcher_ui::init();
    ui_screensaver_init();
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
}

extern "C" void ui_deinit(void)
{
    launcher_ui::deinit();
}
