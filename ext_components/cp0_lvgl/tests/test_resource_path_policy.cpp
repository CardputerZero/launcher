#include "cp0_resource_path_policy.hpp"

#include <cassert>

int main()
{
    using cp0::filesystem::ResourceKind;
    using cp0::filesystem::classify_resource;

    assert(classify_resource("icon.PNG") == ResourceKind::image);
    assert(classify_resource("photo.JpEg") == ResourceKind::image);
    assert(classify_resource("share/audio/chime.OGG") == ResourceKind::audio);
    assert(classify_resource("font.otf") == ResourceKind::font);
    assert(classify_resource("dir.with.dot/file") == ResourceKind::none);
    assert(classify_resource("file.") == ResourceKind::none);

    assert(cp0::filesystem::has_lvgl_drive("A:/share/images/icon.png"));
    assert(cp0::filesystem::has_lvgl_drive("z:value"));
    assert(!cp0::filesystem::has_lvgl_drive("/tmp/a.png"));

    assert(cp0::filesystem::strip_app_root_prefix(
               "/usr/share/APPLaunch/share/images/icon.png") == "share/images/icon.png");
    assert(cp0::filesystem::strip_app_root_prefix("APPLaunch/share/font/a.ttf") ==
           "share/font/a.ttf");
    assert(cp0::filesystem::strip_app_root_prefix("other/a.ttf") == "other/a.ttf");

    assert(cp0::filesystem::has_parent_component("../icon.png"));
    assert(cp0::filesystem::has_parent_component("share/images/../icon.png"));
    assert(cp0::filesystem::has_parent_component("share\\..\\icon.png"));
    assert(!cp0::filesystem::has_parent_component("share/images/.../icon.png"));
    return 0;
}
