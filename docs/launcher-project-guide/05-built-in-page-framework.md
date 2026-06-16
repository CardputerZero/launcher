# 05 - Built-in Page Framework

This chapter explains the class hierarchy, lifecycle, page list, page registration method, and conventions for adding built-in APPLaunch pages. Key source files are `projects/APPLaunch/main/ui/ui_app_page.hpp`, `projects/APPLaunch/main/ui/page_app/*.hpp`, `projects/APPLaunch/main/ui/launch.cpp`, and `projects/APPLaunch/main/ui/ui_launch_page.cpp`.

## 1. What a Built-in Page Is

A built-in page is an LVGL page class compiled into the APPLaunch process. It is different from an external `.desktop` application:

- A built-in page directly creates an `lv_obj_t *root_screen_` and switches to its own screen through `lv_disp_load_scr(page->screen())`.
- The page object is stored in `Launch::app_Page`, and is released asynchronously by the `navigate_home` callback when exiting.
- The page shares the APPLaunch process, LVGL main loop, input thread, resource resolution, and `cp0_lvgl_app.h` system interfaces with the home screen.
- Pages are usually header-only and placed under `projects/APPLaunch/main/ui/page_app/`, then aggregated by `build/generated/include/generated/page_app.h`.

Simplified relationship:

```text
UILaunchPage home carousel
        |
        v
Launch::launch_app()
        |
        +-- External command: cp0_process_exec_blocking()
        +-- Terminal command: UISTPage + PTY
        +-- Built-in page: std::make_shared<PageT>()
                         |
                         v
                    lv_disp_load_scr(page->screen())
```

## 2. Page Base-Class Hierarchy

### 2.1 `AppPageRoot`

`AppPageRoot` is the root base class for all built-in pages. It is located in `projects/APPLaunch/main/ui/ui_app_page.hpp`. It creates an independent screen and an LVGL input group.

```cpp
class AppPageRoot
{
public:
    std::string page_title_ = "APP";
    lv_group_t *input_group_;
    lv_obj_t *root_screen_;
    std::function<void(void)> navigate_home;
    bool has_bottom_bar_ = false;
    int top_bar_height_px_ = 20;

    AppPageRoot()
    {
        creat_base_UI();
        creat_input_group();
    }

    virtual ~AppPageRoot()
    {
        lv_obj_del(root_screen_);
        lv_group_delete(input_group_);
    }
};
```

Key points:

- `root_screen_` is the page's own top-level screen, not a child of the home `UILaunchPage::screen()`.
- By default, `input_group_` only contains `root_screen_`. When the page is launched, it is bound to the current `lv_indev_t`.
- `navigate_home` is injected by `Launch`; a page calls it to return home after ESC or after finishing a task.
- The destructor deletes `root_screen_` and `input_group_`, so LVGL child objects created inside the page are released with the screen.

### 2.2 Top Bar, Content Area, and Bottom Bar Regions

`ui_app_page.hpp` splits a page into several reusable regions:

| Class | Responsibility | Default size |
| --- | --- | --- |
| `AppTopBarRegion` | Creates the status top bar, showing title, WiFi, time, and battery | Height `20px` |
| `AppContentRegion` | Creates the `ui_APP_Container` content area | Height `150px`, or `130px` when a bottom bar exists |
| `AppBottomBarRegion` | Creates the `ui_BOTTOM_Container` bottom bar | Height `20px` |
| `AppPageLayout` | Top bar + content area | `20+150` within `320x170` |
| `AppPageWithBottomBarLayout` | Top bar + content area + bottom bar | `20+130+20` |
| `home_base` | Home-only base class, not exactly equivalent to AppPage | Home status bar + carousel container |

A typical page directly inherits `AppPage`:

```cpp
class UIIpPanelPage : public AppPage
{
public:
    UIIpPanelPage() : AppPage()
    {
        set_page_title("IP INFO");
        creat_UI();
        event_handler_init();
    }
};
```

A few games or full-screen pages inherit `AppPageRoot`, occupy the full `320x170` area themselves, and do not use the default top bar. Examples include `UIGamePage`, `UICompassPage`, and `UITankBattlePage`.

## 3. Top Bar and Status Refresh

The common top bar is implemented by `UIAppTopBar` and contains:

- Left title: `set_page_title()` ultimately updates `top_bar_.set_title()`.
- WiFi signal: `cp0_wifi_get_status()`; the WiFi panel is hidden when not connected.
- Time: `cp0_time_str()`, refreshed every 5 seconds by default.
- Battery: responds to `LV_EVENT_BATTERY`, using `cp0_battery_info_t` to update the percentage and bar.

Key source paths:

- `projects/APPLaunch/main/ui/ui_app_page.hpp`: `UIAppTopBar`, `AppTopBarRegion`.
- `ext_components/cp0_lvgl/include/cp0_lvgl_app.h`: declarations for interfaces such as `cp0_wifi_get_status()`, `cp0_time_str()`, and `cp0_battery_read()`.

Top-bar resources use `cp0_file_path_c()`:

```cpp
lv_obj_set_style_bg_img_src(time_panel_,
    cp0_file_path_c("status_time_background.png"),
    LV_PART_MAIN | LV_STATE_DEFAULT);
```

Note: ordinary built-in pages have their own status refresh timer. A page must release timers it creates itself in its destructor; `AppTopBarRegion` already releases the top-bar status timer.

## 4. Page Lifecycle

### 4.1 Launching a Built-in Page from Home

`launch.cpp` constructs a built-in page app descriptor through a template:

```cpp
template <class PageT>
app::app(std::string name, std::string icon, page_t<PageT>)
    : Name(std::move(name)), Icon(std::move(icon))
{
    launch = [](Launch *ctx) {
        auto p = std::make_shared<PageT>();
        ctx->app_Page = p;
        p->navigate_home = std::bind(&Launch::go_back_home, ctx);
        lv_disp_load_scr(p->screen());
        lv_indev_set_group(lv_indev_get_next(NULL), p->input_group());
    };
}
```

In the actual code in `projects/APPLaunch/main/ui/launch.cpp`, the core flow is:

1. After the user releases ENTER on the home screen, `UILaunchPage::handle_home_key()` calls `launch_selected_app()`.
2. `UILaunchPage::launch_selected_app()` forwards to `Launch::launch_app()`.
3. `Launch::launch_app()` finds the current app and executes that app's `launch` function.
4. The built-in page object is created, the screen is loaded, and the input group is switched.
5. The page calls `navigate_home()` after ESC or after completing its business logic.
6. `Launch::go_back_home()` uses `lv_async_call()` to return to the home screen, rebinds the home input group, and resets `app_Page`.

### 4.2 Returning Home

All returns to home should go through `navigate_home`; do not directly delete the page from inside the page.

```cpp
if (navigate_home)
    navigate_home();
```

`Launch::lv_go_back_home()` will:

- `lv_timer_enable(true)` to restore LVGL timers.
- `UILaunchPage::bind_home_input_group()` to bind the home input group.
- `launch_page_->show_home_screen()` to load the home screen and bind the home input group.
- `app_Page.reset()` to release the current page object.

Notes:

- A page destructor must stop any `lv_timer_t`, background thread, file watcher, PTY, or audio resource that the page created.
- Do not directly `delete this` from a keyboard event callback stack; use `navigate_home` and let `Launch` handle it asynchronously.
- If a page temporarily switches to a child or nested page, it must restore the correct input group.

## 5. Current Built-in Page List

Page implementations are concentrated in `projects/APPLaunch/main/ui/page_app/`.

| Page class | File | Launcher name | Inheritance | Description |
| --- | --- | --- | --- | --- |
| `UISTPage` | `ui_app_st.hpp` | `CLI` or terminal external command | `AppPage` | Terminal emulator, PTY read/write, supports ANSI/VT sequences and keyboard escape sequences |
| `UIGamePage` | `ui_app_game.hpp` | `GAME` | `AppPageRoot` | Snake game, full-screen custom drawing, driven by an LVGL timer |
| `UISetupPage` | `ui_app_setup.hpp` | `SETTING` | `AppPage` | System settings, application toggles, brightness, volume, WiFi, camera resolution, and more |
| `UICompassPage` | `ui_app_compass.hpp` | `Compass` | `AppPageRoot` | Compass page, sensor thread + UI timer |
| `UIIpPanelPage` | `ui_app_ip_panel.hpp` | `IP_PANEL` | `AppPage` | Network interface/IP information list, refreshed every second |
| `UIFilePage` | `ui_app_file.hpp` | `FILE` | `AppPage` | File browser, directory list and enter/back navigation |
| `UISSHPage` | `ui_app_ssh.hpp` | `SSH` | `AppPage` | SSH parameter input, embeds `UISTPage` after connection |
| `UIMeshPage` | `ui_app_mesh.hpp` | `MESH` | `AppPage` | Mesh message list, input overlay, send/refresh |
| `UIRecPage` | `ui_app_rec.hpp` | `REC` | Custom `rec_page` | Recording/playback/file list with asynchronous resource management |
| `UICameraPage` | `ui_app_camera.hpp` | `CAMERA` | `AppPage` | Camera preview, gallery, capture, status page |
| `UILoraPage` | `ui_app_lora.hpp` | `LORA` | `AppPage` | LoRa business page, also contains C-style create/destroy interfaces internally |
| `UITankBattlePage` | `ui_app_tank_battle.hpp` | `TANK` | `AppPageRoot` | Tank mini-game, full-screen, fixed key mapping |

`Python`, `STORE`, and `MATH` are not built-in pages: they are launched through commands or external processes.

## 6. Page Registration and Display Order

Built-in entries are declared in `kBuiltinApps[]` in `launch.cpp`. The first 5 enabled entries initialize the 5 home carousel slots first:

```cpp
constexpr BuiltinAppRegistration kBuiltinApps[] = {
    {{"Python", "python_100.png", "app_Python", false, true}, "python3", true, false, false, nullptr},
    {{"STORE", "store_100.png", "app_Store", false, true},
     "/usr/share/APPLaunch/bin/M5CardputerZero-AppStore", false, true, true, nullptr},
    {{"CLI", "cli_100.png", "app_CLI", false, true}, nullptr, false, true, false, append_page_app<UISTPage>},
    {{"GAME", "game_100.png", "app_Game", false, true}, nullptr, false, true, false, append_page_app<UIGamePage>},
    {{"SETTING", "setting_100.png", "app_Setting", false, true}, nullptr, false, true, false, append_page_app<UISetupPage>},
};
```

Built-in page visibility is now driven by `kBuiltinApps[]` and `AppDescriptor.config_key`. `Launch::rebuild_builtin_apps()` calls `launcher_app_registry_is_enabled()` before appending each descriptor, and Settings changes call `launcher_app_registry_set_enabled()` followed by `Launch::applications_reload()`.

Conventions:

- `Store`, `CLI`, `Game`, and `Setting` are always-on in the settings page and cannot be disabled.
- `Compass` is currently added unconditionally in `launch.cpp` and is not controlled by the Launcher toggle list in `UISetupPage`.
- Pages such as `IP_PANEL`, `FILE`, `SSH`, `MESH`, `REC`, `CAMERA`, `LORA`, and `TANK` are added only in Linux device builds; SDL builds are limited by `#if defined(__linux__) && !defined(HAL_PLATFORM_SDL)`.
- Dynamic `.desktop` applications are scanned and added after built-in pages. Directory changes are checked by a watcher every 3 seconds.

## 7. Page Code Skeleton

New ordinary pages should usually inherit `AppPage`:

```cpp
#pragma once
#include "../ui_app_page.hpp"
#include "compat/input_keys.h"

class UINewPage : public AppPage
{
public:
    UINewPage() : AppPage()
    {
        set_page_title("NEW");
        create_ui();
        event_handler_init();
    }

    ~UINewPage()
    {
        if (timer_) {
            lv_timer_delete(timer_);
            timer_ = nullptr;
        }
    }

private:
    lv_timer_t *timer_ = nullptr;

    void create_ui()
    {
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, 320, 150);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    }

    void event_handler_init()
    {
        lv_obj_add_event_cb(root_screen_, &UINewPage::event_cb, LV_EVENT_ALL, this);
    }

    static void event_cb(lv_event_t *e)
    {
        auto *self = static_cast<UINewPage *>(lv_event_get_user_data(e));
        if (!self || !IS_KEY_RELEASED(e))
            return;

        uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
        if (key == KEY_ESC && self->navigate_home)
            self->navigate_home();
    }
};
```

A new full-screen page may inherit `AppPageRoot`, but it must handle the `320x170` layout, status hints, and return key by itself.

## 8. Page UI Conventions

- Design for a `320x170` resolution. The common page content area is `320x150`; the top `20px` is occupied by the top bar.
- Page objects are usually stored in `std::unordered_map<std::string, lv_obj_t *> ui_obj_` to make repainting/deletion easier.
- For list pages, prefer fixed row height + virtual scrolling instead of free LVGL container scrolling, to avoid focus confusion on a small screen.
- For frequently refreshed pages, use `lv_timer_create()` and call `lv_timer_delete()` in the destructor.
- For background threads or asynchronous callbacks, use an `std::atomic<bool>` alive flag and stop the thread in the destructor to avoid callbacks touching a freed page.
- Do not hard-code relative paths for images, audio, or fonts; use `img_path()`, `audio_path()`, or `cp0_file_path_c()`.

## 9. Nested Pages and Special Pages

`UISSHPage` is a typical nested page: while entering SSH parameters, the keyboard is handled by `UISSHPage`; after connection, it creates `UISTPage` and switches the screen and input group.

```cpp
terminal_page_ = std::make_shared<UISTPage>();
terminal_page_->navigate_home = [this]() {
    terminal_page_.reset();
    view_state_ = ViewState::INPUT;
    lv_disp_load_scr(this->screen());
    lv_indev_set_group(lv_indev_get_next(NULL), this->input_group());
};

lv_disp_load_scr(terminal_page_->screen());
lv_indev_set_group(lv_indev_get_next(NULL), terminal_page_->input_group());
```

Special care is required for this type of page:

- Exiting a child page does not necessarily mean returning home; it may only return to the parent page.
- The input group must switch with the current screen, otherwise keys will be delivered to an invisible page.
- The parent page destructor must release child page objects first.

## 10. Relationship with the Home Carousel

The home carousel itself is managed by `ui_launch_page.cpp`:

- `carousel_elements` stores 5 cards, 5 titles, and 5 page dots.
- When switching left/right, `switch_left()` / `switch_right()` are called. After the animation finishes, the array is rotated and `Launch` updates the far-side slot content.
- ENTER triggers `UILaunchPage::launch_selected_app()`, which ultimately calls the current app's `launch()`.

Built-in pages do not directly manipulate the home carousel. After returning home, the carousel state is preserved by `Launch`.

## 11. Common Notes

- Do not perform long blocking operations in a page constructor; display the page or loading state first, then start the task.
- Do not assume `lv_indev_get_next(NULL)` is always non-null; preferably check before switching the input group.
- Do not directly access home global objects from a page unless it is clearly a home-screen feature.
- For page titles, call `set_page_title()` instead of modifying the internal top-bar label directly.
- Every page that can exit should support `KEY_ESC` and call `navigate_home` or return to the previous view.
- Page toggle keys must stay consistent between `AppDescriptor.config_key`, `UISetupPage::save_app_toggle()`, and `launcher_app_registry_is_enabled()` in `launch.cpp`.
