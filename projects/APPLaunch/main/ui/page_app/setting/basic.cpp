#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"

namespace setting {

void Launcher::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Launcher";
    std::size_t app_count = 0;
    const AppDescriptor *apps = launcher_app_registry_entries(&app_count);
    for (std::size_t i = 0; i < app_count; ++i) {
        const AppDescriptor &desc = apps[i];
        if (!desc.configurable)
            continue;
        bool enabled = launcher_app_registry_is_enabled(desc);
        m.sub_items.push_back({desc.label, true, enabled,
            [page, key = std::string(desc.config_key)]() { Launcher::save_app_toggle(*page, key); }});
    }
    menu.push_back(m);
}

void Launcher::save_app_toggle(UISetupPage &page, const std::string &config_key)
{
    int launcher_idx = page.find_menu("Launcher");
    if (launcher_idx < 0)
        return;
    MenuItem &launcher_menu = page.menu_items_[launcher_idx];

    std::size_t app_count = 0;
    const AppDescriptor *apps = launcher_app_registry_entries(&app_count);
    int visible_idx = 0;
    for (std::size_t i = 0; i < app_count; ++i) {
        const AppDescriptor &desc = apps[i];
        if (!desc.configurable)
            continue;
        if (config_key == desc.config_key) {
            if (visible_idx >= (int)launcher_menu.sub_items.size())
                return;
            bool enabled = launcher_menu.sub_items[visible_idx].toggle_state;
            launcher_app_registry_set_enabled(desc, enabled);
            UISetupPage::config_save();
            launcher_app_registry_notify_changed();
            return;
        }
        ++visible_idx;
    }
}

void Boot::factory_reset()
{
    remove("/var/lib/applaunch/settings");
    cp0_system_reboot();
}

void Boot::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Boot";
    m.sub_items = {
        {"Reboot", false, false, [page]() {
            page->enter_confirm_action("Reboot?", [page](){ cp0_system_reboot(); });
        }},
        {"Shutdown", false, false, [page]() {
            page->enter_confirm_action("Shutdown?", [page](){ cp0_system_shutdown(); });
        }},
    };
    menu.push_back(m);
}

void Boot::rearm_oobe_and_reboot()
{
#ifndef _WIN32
    mkdir("/var/lib/applaunch", 0755);
#endif
    FILE *f = fopen("/var/lib/applaunch/run-oobe", "w");
    if (f) fclose(f);
    cp0_system_reboot();
}

void Screen::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Screen";
    m.sub_items = {
        {"Brightness", false, false, [page]() { page->screen_.enter_brightness_adjust(*page); }},
        {"DarkTime", false, false, [page]() { page->screen_.enter_darktime_adjust(*page); }},
    };
    menu.push_back(m);
}

int Screen::backlight_read()
{
    int value = -1;
    cp0_signal_settings_api({"BacklightRead"}, [&](int code, std::string data) {
        if (code == 0) value = std::atoi(data.c_str());
    });
    return value;
}

int Screen::backlight_max()
{
    int value = 100;
    cp0_signal_settings_api({"BacklightMax"}, [&](int code, std::string data) {
        if (code == 0) value = std::atoi(data.c_str());
    });
    return value;
}

void Screen::enter_brightness_adjust(UISetupPage &page)
{
    page.val_title_ = "Brightness";
    page.val_options_ = {"100%", "75%", "50%", "25%"};
    bright_val_ = backlight_read();
    int mx = backlight_max();
    int pct = mx > 0 ? bright_val_ * 100 / mx : 100;
    if (pct >= 87) page.val_sel_idx_ = 0;
    else if (pct >= 62) page.val_sel_idx_ = 1;
    else if (pct >= 37) page.val_sel_idx_ = 2;
    else page.val_sel_idx_ = 3;
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void Screen::apply_value(UISetupPage &page)
{
    if (page.val_title_ == "DarkTime") {
        static const int times[] = {0, 10, 30, 60, 300};
        UISetupPage::config_set_int("dark_time", times[page.val_sel_idx_]);
        UISetupPage::config_save();
        return;
    }

    int mx = backlight_max();
    int pcts[] = {100, 75, 50, 25};
    int new_val = mx * pcts[page.val_sel_idx_] / 100;
    if (new_val < 1) new_val = 1;
    cp0_backlight_write(new_val);
    UISetupPage::config_set_int("brightness", new_val);
    UISetupPage::config_save();
}

void Screen::enter_darktime_adjust(UISetupPage &page)
{
    static const int times[] = {0, 10, 30, 60, 300};
    page.val_title_ = "DarkTime";
    page.val_options_ = {"Never", "10S", "30S", "60S", "300S"};
    const int saved = UISetupPage::config_get_int("dark_time", 30);
    page.val_sel_idx_ = 2;
    for (size_t i = 0; i < sizeof(times) / sizeof(times[0]); ++i) {
        if (times[i] == saved) {
            page.val_sel_idx_ = static_cast<int>(i);
            break;
        }
    }
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void Speaker::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Speaker";
    m.sub_items = {{"Volume", false, false, [page]() { page->speaker_.enter_volume_adjust(*page); }}};
    menu.push_back(m);
}

void Speaker::enter_volume_adjust(UISetupPage &page)
{
    page.val_title_ = "Volume";
    page.val_options_ = {"100%", "75%", "50%", "25%", "0%"};
    vol_val_ = UISetupPage::config_get_int("volume", UISetupPage::audio_volume_read());
    int pct = vol_val_;
    if (pct >= 87) page.val_sel_idx_ = 0;
    else if (pct >= 62) page.val_sel_idx_ = 1;
    else if (pct >= 37) page.val_sel_idx_ = 2;
    else if (pct >= 12) page.val_sel_idx_ = 3;
    else page.val_sel_idx_ = 4;
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void Speaker::apply_value(UISetupPage &page)
{
    int pcts[] = {100, 75, 50, 25, 0};
    int new_val = pcts[page.val_sel_idx_];
    UISetupPage::audio_volume_write(new_val);
    UISetupPage::config_set_int("volume", new_val);
    UISetupPage::config_save();
}

void Camera::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Camera";
    m.sub_items = {{"Resolution", false, false, [page]() { page->camera_.enter_resolution(*page); }}};
    menu.push_back(m);
}

void Camera::enter_resolution(UISetupPage &page)
{
    page.val_title_ = "Resolution";
    page.val_options_ = {"1280x720", "640x480"};
    page.val_sel_idx_ = (UISetupPage::config_get_int("camera.resolution.width", 1280) == 640) ? 1 : 0;
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void Camera::apply_value(UISetupPage &page)
{
    int width = 1280, height = 720;
    if (page.val_sel_idx_ == 1) { width = 640; height = 480; }
    UISetupPage::config_set_int("camera.resolution.width", width);
    UISetupPage::config_set_int("camera.resolution.height", height);
    UISetupPage::config_save();
}

} // namespace setting
