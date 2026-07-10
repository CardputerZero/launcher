#include "lvgl/lvgl.h"
#include "global_config.h"
#ifdef CONFIG_V9_5_LV_USE_SDL
#include "hal/hal_paths.h"
#endif
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "hal_lvgl_bsp.h"
#include "main.h"

namespace {

std::string executable_dir(const char *argv0)
{
    if (!argv0 || argv0[0] == '\0')
        return ".";
    std::string path(argv0);
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return ".";
    if (slash == 0)
        return "/";
    return path.substr(0, slash);
}

}  // namespace

int lvgl_main(int argc, char **argv)
{
    (void)argc;
#ifdef CONFIG_V9_5_LV_USE_SDL
    const std::string exe_dir = executable_dir(argv && argv[0] ? argv[0] : nullptr);
    hal_paths_init(exe_dir.c_str());
#endif

    lv_init();
    cp0_lvgl_init();
    lv_display_t *disp = lv_display_get_default();
    if (disp == nullptr) {
        fprintf(stderr, "HelloWorld: failed to create LVGL display\n");
        return 1;
    }

    printf("HelloWorld: display %dx%d\n",
           (int)lv_display_get_horizontal_resolution(disp),
           (int)lv_display_get_vertical_resolution(disp));

    ui_init();
    lv_obj_invalidate(lv_screen_active());

    while (!ui_should_quit()) {
        lv_timer_handler();
        usleep(10000);
    }
    return 0;
}
