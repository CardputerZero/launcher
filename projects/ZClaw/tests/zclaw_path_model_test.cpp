#include "zclaw_path_model.h"

#include <cassert>

int main()
{
    assert(zclaw::select_home_directory("/env/home", "/account/home") ==
           "/env/home");
    assert(zclaw::select_home_directory("", "/account/home") ==
           "/account/home");
    assert(zclaw::select_home_directory("", "") == ".");

    zclaw::ZClawPaths paths = zclaw::make_zclaw_paths("/home/test");
    assert(paths.home == "/home/test");
    assert(paths.zeroclaw_directory == "/home/test/.zeroclaw");
    assert(paths.providers_config ==
           "/home/test/.zeroclaw/zclaw_providers.tsv");
    assert(paths.ui_config == "/home/test/.zeroclaw/zclaw_ui.tsv");
    assert(paths.zeroclaw_config == "/home/test/.zeroclaw/config.toml");
    assert(paths.zeroclaw_binary == "/home/test/.zeroclaw/bin/zeroclaw");

    paths = zclaw::make_zclaw_paths("/home/test/");
    assert(paths.zeroclaw_directory == "/home/test/.zeroclaw");
    paths = zclaw::make_zclaw_paths("/");
    assert(paths.zeroclaw_directory == "/.zeroclaw");
    paths = zclaw::make_zclaw_paths("");
    assert(paths.home == ".");
    assert(paths.zeroclaw_directory == "./.zeroclaw");
    return 0;
}
