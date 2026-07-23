#include "cp0_lvgl_app_runner.hpp"
#include "global_config.h"
#include "main.h"

#ifdef CONFIG_V9_5_LV_USE_SDL
#include "hal/hal_paths.h"
#endif

#include <string>

namespace {

std::string executable_dir(const char *path)
{
    if (!path || !path[0])
        return ".";
    const std::string value(path);
    const size_t slash = value.find_last_of('/');
    if (slash == std::string::npos)
        return ".";
    return slash == 0 ? "/" : value.substr(0, slash);
}

} // namespace

int main(int argc, char *argv[])
{
#ifdef CONFIG_V9_5_LV_USE_SDL
    const std::string directory = executable_dir(argv && argv[0] ? argv[0] : nullptr);
    hal_paths_init(directory.c_str());
#else
    (void)argc;
    (void)argv;
#endif
    return run_zclaw_app();
}
