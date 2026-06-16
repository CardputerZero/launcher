#include "lvgl/lvgl.h"

namespace launcher_ui {
void init();
}

extern "C" void ui_init(void)
{
    launcher_ui::init();
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
}
