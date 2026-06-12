# 10 - Extension Development Guide

This chapter explains how to extend APPLaunch, focusing on four common change types: adding a built-in page, adding an external `.desktop` app, adding image/audio/font assets, and changing settings toggles. The core code is under `projects/APPLaunch/main/ui`; platform adaptation and path resolution live under `ext_components/cp0_lvgl`.

## 1. Entry Points to Understand Before Extending

| Entry point | Purpose |
| --- | --- |
| `projects/APPLaunch/main/ui/Launch.cpp` | Fixed app list, dynamic `.desktop` scanning, launching built-in pages or external processes |
| `projects/APPLaunch/main/ui/components/page_app/` | Built-in page implementation directory; pages are usually header-only `.hpp` files |
| `projects/APPLaunch/main/ui/components/ui_app_page.hpp` | Shared page capabilities such as `AppPage`, top bar, `img_path()`, and `audio_path()` |
| `projects/APPLaunch/main/ui/components/generate_page_app_includes.py` | Automatically generates `page_app.h` before build and includes every `page_app/*.hpp` file |
| `projects/APPLaunch/APPLaunch/` | Runtime asset tree; after packaging it maps to `/usr/share/APPLaunch/` on the device |
| `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_file.cpp` | Device-side `cp0_file_path()` path rules |
| `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_file.cpp` | SDL2 development-host `cp0_file_path()` path rules |
| `ext_components/cp0_lvgl/src/cp0/cp0_app_config.cpp` | Device-side settings persistence, saved to `/var/lib/applaunch/settings` |

APPLaunch has two kinds of app sources:

- **Built-in pages**: compiled into the APPLaunch process and registered with `app("NAME", icon, page_v<PageT>)`. When opened, APPLaunch creates a `PageT` object and switches to its screen.
- **External apps**: launched as independent processes through a fixed `Exec` value or a `.desktop` descriptor. For non-terminal apps, the Launcher pauses its LVGL timer, waits for the child process to exit, and then returns to the home page.

## 2. Adding a Built-in Page

Built-in pages are suitable for features that run in the same process as Launcher, use LVGL directly, and need to share the input group, top bar, or status bar. Examples include Settings, Music, Files, Camera, and LoRa pages.

### 2.1 Create the Page File

Create a new `.hpp` under `projects/APPLaunch/main/ui/components/page_app/`. The recommended naming style is `ui_app_xxx.hpp`. The page class should inherit from `AppPage`; set the title, create the UI, and bind key events in the constructor.

Minimal skeleton:

```cpp
#pragma once

#include "../ui_app_page.hpp"

class UIMyToolPage : public AppPage
{
public:
    UIMyToolPage() : AppPage()
    {
        set_page_title("MY TOOL");
        create_ui();
        event_handler_init();
    }

private:
    lv_obj_t *title_ = nullptr;

    void create_ui()
    {
        lv_obj_t *root = screen();
        lv_obj_set_style_bg_color(root, lv_color_hex(0x101820), LV_PART_MAIN | LV_STATE_DEFAULT);

        UIAppTopBar top("MY TOOL");
        top.create(root);

        title_ = lv_label_create(root);
        lv_label_set_text(title_, "Hello APPLaunch");
        lv_obj_center(title_);
    }

    void event_handler_init()
    {
        lv_obj_add_event_cb(screen(), &UIMyToolPage::key_event_cb, LV_EVENT_KEY, this);
    }

    static void key_event_cb(lv_event_t *e)
    {
        auto *self = static_cast<UIMyToolPage *>(lv_event_get_user_data(e));
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ESC && self->navigate_home) {
            self->navigate_home();
        }
    }
};
```

Notes:

- The page must inherit from `AppPage` so it can reuse mechanisms such as `screen()`, `input_group()`, and `navigate_home`.
- Prefer calling `navigate_home()` to return to the home page. Do not call `lv_disp_load_scr(ui_Screen1)` directly, or `LaunchImpl` will not be able to release the current page object correctly.
- If the page creates LVGL timers, file descriptors, threads, or peripheral handles, release them in the destructor.
- Use 320x170 as the baseline page size. A common layout is a 20 px top bar and a 320x150 body.
- Do not hard-code absolute asset paths. Use `img_path("xxx.png")` for images and `audio_path("xxx.wav")` for audio.

### 2.2 Confirm the Page Is Included

`projects/APPLaunch/main/SConstruct` runs this script before building:

```python
ui/components/generate_page_app_includes.py
```

The script scans `projects/APPLaunch/main/ui/components/page_app/*.hpp` and generates `projects/APPLaunch/main/ui/components/page_app.h`. In most cases, as long as the file suffix is `.hpp`, it will be included automatically during the build.

If you check manually, `page_app.h` should contain:

```cpp
#include "page_app/ui_app_my_tool.hpp"
```

### 2.3 Register the Page in the Home App List

Open `projects/APPLaunch/main/ui/Launch.cpp` and find `LaunchImpl::LaunchImpl()`. Register a built-in page like this:

```cpp
app_list.emplace_back("MYTOOL", img_path("mytool_100.png"), page_v<UIMyToolPage>);
```

It is recommended to place it inside the `APP_ENABLED` control section so the Settings page can later control whether it is shown:

```cpp
#define APP_ENABLED(key) (cp0_config_get_int("app_" key, 1) != 0)

if (APP_ENABLED("MyTool"))
    app_list.emplace_back("MYTOOL", img_path("mytool_100.png"), page_v<UIMyToolPage>);

#undef APP_ENABLED
```

Registration rules:

- The first argument is the display name in the home carousel. Keep it short to avoid truncation on the small screen.
- The second argument is the icon path, usually `img_path("xxx_100.png")`.
- The third argument, `page_v<PageT>`, means a built-in page is created when the app is clicked.
- If the page only supports device-side hardware, place it inside `#if defined(__linux__) && !defined(HAL_PLATFORM_SDL)` to avoid SDL2 build failures.

### 2.4 Add a Settings Page Toggle

If you want the `Launcher` menu in Settings to control whether the new page is shown, update `app_keys` and `app_labels` in `UISetupPage::menu_init()`.

Example:

```cpp
static const char *app_keys[] = {
    "Python", "Store", "CLI", "Game", "Setting",
    "Music", "Math", "MyTool"
};

static const char *app_labels[] = {
    "Python", "Store", "CLI", "Game", "Setting",
    "Music", "Math", "My Tool"
};
```

`save_app_toggle()` stores the switch as `app_<key>`, for example `app_MyTool=0`. Read the same key in `Launch.cpp`:

```cpp
cp0_config_get_int("app_MyTool", 1)
```

The device-side persistence file is:

```text
/var/lib/applaunch/settings
```

### 2.5 Build Verification

SDL2 local verification:

```bash
cd projects/APPLaunch
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j8 --implicit-deps-changed
./dist/M5CardputerZero-APPLaunch
```

Device cross-compile verification:

```bash
cd projects/APPLaunch
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8 --implicit-deps-changed
```

For device-only pages, run at least the cross build. If the page can also be displayed on the development host, use SDL2 first to quickly verify the UI and keys.

## 3. Adding an External `.desktop` App

External `.desktop` apps are suitable for independent executables, scripts, or terminal commands. They do not require changing the C++ app list; APPLaunch scans the `applications` directory and dynamically adds them to the home page.

### 3.1 Place the `.desktop` File

Development-tree path:

```text
projects/APPLaunch/APPLaunch/applications/
```

Installed device path:

```text
/usr/share/APPLaunch/applications/
```

Existing template:

```text
projects/APPLaunch/APPLaunch/applications/vim.desktop.temple
```

Note that the current scanning logic only handles filenames ending in `.desktop`. `.desktop.temple` is only a template and will not be loaded.

### 3.2 `.desktop` Field Format

Minimal example:

```ini
[Desktop Entry]
Name=Vim
Exec=vim
Terminal=true
Icon=share/images/email.png
Type=Application
```

Fields currently parsed by APPLaunch:

| Field | Required | Description |
| --- | --- | --- |
| `Name` | Yes | Display name on the home page |
| `Exec` | Yes | Command to execute; can be an absolute path or a shell command |
| `Icon` | No | Icon path; recommended format is `share/images/xxx.png` or any path readable by LVGL |
| `Terminal` | No | `true`/`True`/`1` means run in the built-in `UIConsolePage` |
| `Sysplause` | No | Terminal apps only; controls pause behavior after the terminal command ends, default true |
| `Type` | No | Kept for desktop-file convention compatibility; APPLaunch does not currently depend on it |
| `TryExec` | No | Not currently parsed by APPLaunch; can only serve as a descriptive field |

Example 1: launch a terminal command.

```ini
[Desktop Entry]
Name=TOP
Exec=top
Terminal=true
Sysplause=false
Icon=share/images/cli_100.png
Type=Application
```

Example 2: launch an independent program.

```ini
[Desktop Entry]
Name=MyApp
Exec=/usr/share/APPLaunch/bin/my_app
Terminal=false
Icon=share/images/my_app_100.png
Type=Application
```

Example 3: launch a script.

```ini
[Desktop Entry]
Name=NetInfo
Exec=/bin/sh /usr/share/APPLaunch/bin/netinfo.sh
Terminal=true
Icon=share/images/ip_panel_100.png
Type=Application
```

### 3.3 External App Launch Behavior

`Launch.cpp` supports two external-app launch modes:

- `Terminal=true`: creates `UIConsolePage`, displays a PTY terminal inside the APPLaunch process, and executes `Exec`.
- `Terminal=false`: calls `cp0_process_exec_blocking()` to start an external process. APPLaunch pauses the LVGL timer and input group, waits for the child process to exit, and then restores the home page.

Returning from non-terminal external apps depends on these behaviors:

- If the child process exits normally, APPLaunch restores `ui_Screen1`.
- On the device, holding ESC for about 3 seconds sends SIGTERM to the external app process group; if it still has not exited after another 3 seconds, SIGKILL is sent.
- `cp0_process_exec_blocking()` pauses the Launcher keyboard thread so the external program can read evdev input directly.

### 3.4 Dynamic Refresh

APPLaunch calls `applications_load()` at startup to scan `.desktop` files. After that, `inotify`/SDL directory watching checks the application directory every 3 seconds. After adding, deleting, or editing a `.desktop` file, the carousel normally refreshes automatically without restarting Launcher.

If it does not refresh:

```bash
# Device side
ls -l /usr/share/APPLaunch/applications
journalctl -u APPLaunch.service -f

# SDL2 development host: confirm APPLaunch/applications exists near the run directory
find projects/APPLaunch -path '*APPLaunch/applications*' -maxdepth 5 -type f
```

### 3.5 Deduplication Rules

Dynamic apps are deduplicated by `Exec`. If two `.desktop` files have exactly the same `Exec`, the later scanned file is skipped and this message is printed:

```text
applications_load: skip ... (duplicate Exec)
```

## 4. Adding Assets

Assets include images, audio, fonts, and external programs/scripts. In the development tree, place them under `projects/APPLaunch/APPLaunch/`. During build, `main/SConstruct` copies this tree into the output/install package through `STATIC_FILES += [ADir('../APPLaunch')]`.

### 4.1 Asset Directories

| Type | Development-tree path | Device path | Recommended access method |
| --- | --- | --- | --- |
| Images | `projects/APPLaunch/APPLaunch/share/images/` | `/usr/share/APPLaunch/share/images/` | `img_path("xxx.png")` or `.desktop` `Icon=share/images/xxx.png` |
| Audio | `projects/APPLaunch/APPLaunch/share/audio/` | `/usr/share/APPLaunch/share/audio/` | `audio_path("xxx.wav")` |
| Fonts | `projects/APPLaunch/APPLaunch/share/font/` | `/usr/share/APPLaunch/share/font/` | `launcher_fonts().get("xxx.ttf", size, style)` |
| External apps | `projects/APPLaunch/APPLaunch/bin/` | `/usr/share/APPLaunch/bin/` | `.desktop` `Exec=/usr/share/APPLaunch/bin/xxx` |
| `.desktop` | `projects/APPLaunch/APPLaunch/applications/` | `/usr/share/APPLaunch/applications/` | Automatically scanned |

If you add a `bin/` directory or scripts, make sure scripts have execute permission, or invoke them through `/bin/sh script.sh` in the `.desktop` file.

### 4.2 `cp0_file_path()` Path Rules

Key rules in the device-side `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_file.cpp`:

- `cp0_file_path("applications")` -> `/usr/share/APPLaunch/applications`
- `cp0_file_path("lock_file")` -> `/tmp/M5CardputerZero-APPLaunch_fcntl.lock`
- Image extensions `png/gif/jpg/jpeg/svg` -> `share/images/<file>`
- Audio extensions `wav/mp3/ogg` -> `/usr/share/APPLaunch/share/audio/<file>`
- Font extensions `ttf/otf` -> `/usr/share/APPLaunch/share/font/<file>`

On SDL2, `sdl_lvgl_file.cpp` infers the asset root from the executable directory, current working directory, and `APPLaunch/share`, then converts paths relative to the current working directory for convenient development-host runs.

### 4.3 Image Asset Recommendations

- Home carousel icons should have a 100 px version, such as `mytool_100.png`.
- For small icons inside pages, provide 80 px or smaller versions if needed.
- LVGL is sensitive to image paths and formats; reusing the repository's existing PNG naming and size style is the safest option.
- If `panel_set_icon()` prints `missing/unreadable`, first check whether the file exists in the runtime asset tree, not only in the source directory.

### 4.4 Audio Asset Recommendations

For key sounds in a page, refer to `UISetupPage`:

```cpp
std::string snd_enter_ = audio_path("key_enter.wav");
cp0_signal_audio_api({"PlayFile", snd_enter_}, nullptr);
```

Device-side audio normally uses `/usr/share/APPLaunch/share/audio/xxx.wav`; the SDL2 side is resolved by the path adaptation layer.

### 4.5 Font Asset Recommendations

Place fonts under `share/font/`. In pages, prefer the shared font cache to avoid repeated creation:

```cpp
lv_font_t *font = launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD);
lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
```

After adding a font, verify that FreeType is enabled in both SDL2 and device builds. The SDL2 and cross-build configurations both add FreeType-related include/link parameters for LVGL.

## 5. Changing Settings Toggles

The Settings page is centralized in `projects/APPLaunch/main/ui/components/page_app/ui_app_setup.hpp`. Current settings include Launcher app visibility toggles, Boot, Screen, WiFi, Speaker, Camera, Info, About, Help, ExtPort, and others.

### 5.1 Add a Launcher App Toggle

Steps:

1. Add an internal key such as `MyTool` to `app_keys` in `UISetupPage::menu_init()`.
2. Add a display label such as `My Tool` to `app_labels` in the same location.
3. Use the same key when registering the app in `Launch.cpp`: `APP_ENABLED("MyTool")`.
4. Open the Settings page, enter the `Launcher` menu, and toggle O/X.
5. If the list does not refresh after returning to the home page, restart APPLaunch. The current fixed/built-in list reads configuration when `LaunchImpl` is constructed.

### 5.2 Add a Regular Setting

Find the corresponding group in `menu_init()` and add an item to `sub_items`:

```cpp
{"My Option", true, cp0_config_get_int("my_option", 1) != 0, [this]() {
    bool en = cp0_config_get_int("my_option", 1) == 0;
    cp0_config_set_int("my_option", en ? 1 : 0);
    cp0_config_save();
}},
```

For second-level or third-level pages that choose values, refer to these existing implementations:

- `enter_brightness_adjust()`: brightness selection.
- `enter_darktime_adjust()`: screen-off timeout selection.
- `enter_volume_adjust()` and `apply_volume()`: volume saving and application.
- `enter_camera_resolution()`: camera resolution.
- `enter_startup_mode()`: startup mode.

### 5.3 Configuration Persistence Location

Device-side configuration implementation: `ext_components/cp0_lvgl/src/cp0/cp0_app_config.cpp`.

- Configuration directory: `/var/lib/applaunch`
- Configuration file: `/var/lib/applaunch/settings`
- Format: one `key=value` per line
- Maximum entries: `MAX_ENTRIES=32`

Common commands:

```bash
sudo cat /var/lib/applaunch/settings
sudo sed -i 's/^app_Music=.*/app_Music=1/' /var/lib/applaunch/settings
sudo systemctl restart APPLaunch.service
```

If you add many configuration items, remember that the current maximum is 32 entries. After that limit, `cp0_config_set_*` returns directly and the setting will not be saved.

## 6. Verification Checklist When Extending

| Check item | Method |
| --- | --- |
| Files are placed only in the correct directories | Built-in pages in `main/ui/components/page_app/`, assets in `APPLaunch/share/`, `.desktop` files in `APPLaunch/applications/` |
| SDL2 builds successfully | `CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j8 --implicit-deps-changed` |
| Device cross build succeeds | `CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8 --implicit-deps-changed` |
| Icons display correctly | Check logs for `set panel icon missing/unreadable` |
| Page can return home | Built-in page calls `navigate_home()` on ESC; external page exits itself or returns on long ESC press |
| `.desktop` is loaded | Filename ends in `.desktop` and contains `[Desktop Entry]`, `Name`, and `Exec` |
| Settings are saved | Check whether the corresponding key is written to `/var/lib/applaunch/settings` |
