# 12 - Common Modification Entry Points

This chapter organizes common APPLaunch entry points by “what do I want to change?”. Before making changes, first check the current working-tree state to avoid overwriting changes from other agents:

```bash
cd /home/nihao/w2T/github/launcher
git status --short
```

## 1. High-frequency Task Entry Table

| Task | Main files/directories | Key points | Verification |
| --- | --- | --- | --- |
| Add a built-in page | `projects/APPLaunch/main/ui/page_app/` | Create `ui_app_xxx.hpp` and inherit from `AppPage` | Build with SDL2 and open the page |
| Register a built-in page on home | `projects/APPLaunch/main/ui/launch.cpp` | Add a `kBuiltinApps[]` entry with `append_page_app<PageT>` | Icon appears in the home carousel |
| Control built-in page visibility toggle | `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp`, `projects/APPLaunch/main/ui/launch.cpp` | Settings writes `AppDescriptor.config_key`; Launcher reads `launcher_app_registry_is_enabled()` | Toggle in Settings, then restart or refresh home |
| Add external `.desktop` app | `projects/APPLaunch/APPLaunch/applications/` | Filename must end in `.desktop` and include `Name` and `Exec` | No skip logs; app appears on home |
| Add icon | `projects/APPLaunch/APPLaunch/share/images/` | Built-in pages use `img_path()`, `.desktop` uses `Icon=share/images/xxx.png` | No `missing/unreadable` logs |
| Add sound effect | `projects/APPLaunch/APPLaunch/share/audio/` | Pages use `audio_path()` and `cp0_signal_audio_api()` | Sound plays on device |
| Add font | `projects/APPLaunch/APPLaunch/share/font/` | Use `launcher_fonts().get()` and confirm FreeType dependency | Page text uses the new font |
| Change home carousel layout | `projects/APPLaunch/main/ui/ui_launch_page.cpp`, `projects/APPLaunch/main/ui/ui_launch_page.h` | 5 slots, left/right switching, center card | Check animation and input in SDL2 |
| Change carousel animation | `projects/APPLaunch/main/ui/animation/ui_launcher_animation.cpp` | Card movement, scale, opacity, and other animations | Switch left/right repeatedly in SDL2 |
| Change home status bar | `projects/APPLaunch/main/ui/launch.cpp`, `projects/APPLaunch/main/ui/ui.cpp` | `update_home_status_bar()` refreshes WiFi/time/battery | Check `[HOME_STATUS]` logs |
| Change Settings menu | `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp` | Add `MenuItem`/`SubItem` in `menu_init()` | Enter the SETTING page and test |
| Change configuration saving logic | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_config.cpp` | Currently saves to `/var/lib/applaunch/settings`, max 32 entries | Inspect the settings file |
| Change asset path rules | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_file.cpp`, `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_file.cpp` | Consider device and SDL2 consistently | Check assets on both SDL2 and device |
| Change external app launch/return | `projects/APPLaunch/main/ui/launch.cpp`, `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_process.cpp` | `launch_Exec()`, `cp0_process_exec_blocking()` | External app starts, ESC returns |
| Change terminal apps | `projects/APPLaunch/main/ui/page_app/ui_app_st.hpp`, `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_pty.cpp` | PTY, command execution, input/output | Verify with a `Terminal=true` app |
| Change input mapping | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_keyboard.c`, `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_keyboard.c` | Device and SDL2 input differences | `evtest` + SDL2 keyboard |
| Change startup flow | `projects/APPLaunch/main/src/main.cpp` | `lv_init()`, `cp0_lvgl_init()`, `ui_init()`, main loop | Check `[BOOT]` logs |
| Change build dependencies | `projects/APPLaunch/main/SConstruct` | `SRCS`, `INCLUDE`, `REQUIREMENTS`, `STATIC_FILES` | scons build |
| Change build configuration | `projects/APPLaunch/*.mk` | Different configs for SDL2, device, and cross build | Build with a specific `CONFIG_DEFAULT_FILE` |
| Change package contents | `scripts/debian_packager.py`, `projects/APPLaunch/APPLaunch/` | Asset tree and install path | Check file list after building package |
| Change platform HAL | `ext_components/cp0_lvgl/src/cp0/`, `ext_components/cp0_lvgl/include/hal/` | framebuffer, audio, network, settings, process, etc. | Test on the device |

## 2. Source Directory Quick Reference

| Path | Purpose |
| --- | --- |
| `projects/APPLaunch/main/src/main.cpp` | APPLaunch process entry, initialization order, main loop, external app lock detection |
| `projects/APPLaunch/main/ui/ui.cpp` | Creates global LVGL UI objects; most `ui_*` globals originate here |
| `projects/APPLaunch/main/ui/ui.cpp` | C++ UI initialization bridge |
| `projects/APPLaunch/main/ui/ui.h` | UI global declarations and C/C++ shared interface |
| `projects/APPLaunch/main/ui/launch.cpp` | App model, app list, launch logic, dynamic `.desktop` loading, status bar refresh |
| `projects/APPLaunch/main/ui/launch.h` | Public wrapper class for `Launch` |
| `projects/APPLaunch/main/ui/ui_launch_page.cpp` | Home screen, carousel slots, input events, home-page behavior |
| `projects/APPLaunch/main/ui/ui_launch_page.h` | Home class interface, including panel/label/input group accessors |
| `projects/APPLaunch/main/ui/ui_loading.cpp` | Loading overlay show/hide |
| `projects/APPLaunch/main/ui/ui_global_hint.cpp` | Global hint overlay |
| `projects/APPLaunch/main/ui/launcher_ui_runtime.cpp` | LVGL OS/thread helpers |
| `projects/APPLaunch/main/ui/animation/` | Home carousel animation implementation |
| `projects/APPLaunch/main/ui/ui_app_page.hpp` | Built-in page base class, top bar, shared asset path helpers |
| `projects/APPLaunch/build/generated/include/generated/page_app.h` | Auto-generated built-in page include aggregate |
| `projects/APPLaunch/main/ui/page_app/` | Built-in page implementation directory |
| `ext_components/cp0_lvgl/include/` | Shared CP0/LVGL headers, including keyboard and compatibility input headers |

## 3. Built-in Page Entry Table

| Page/feature | File | Registered name or icon | Description |
| --- | --- | --- | --- |
| SETTING | `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp` | `SETTING` / `setting_100.png` | Settings page, including app toggles, brightness, volume, WiFi, camera, etc. |
| GAME | `projects/APPLaunch/main/ui/page_app/ui_app_game.hpp` | `GAME` / `game_100.png` | Built-in game entry |
| Compass | `projects/APPLaunch/main/ui/page_app/ui_app_compass.hpp` | `Compass` / `compass_needle_80.png` | Compass page |
| IP_PANEL | `projects/APPLaunch/main/ui/page_app/ui_app_ip_panel.hpp` | `IP_PANEL` / `ip_panel_100.png` | IP information panel, enabled on device |
| FILE | `projects/APPLaunch/main/ui/page_app/ui_app_file.hpp` | `FILE` / `file_100.png` | File page, enabled on device |
| SSH | `projects/APPLaunch/main/ui/page_app/ui_app_ssh.hpp` | `SSH` / `ssh_100.png` | SSH page, enabled on device |
| MESH | `projects/APPLaunch/main/ui/page_app/ui_app_mesh.hpp` | `MESH` / `mesh_100.png` | Mesh page, enabled on device |
| REC | `projects/APPLaunch/main/ui/page_app/ui_app_rec.hpp` | `REC` / `rec_100.png` | Recording page, enabled on device |
| CAMERA | `projects/APPLaunch/main/ui/page_app/ui_app_camera.hpp` | `CAMERA` / `camera_100.png` | Camera page, enabled on device |
| LORA | `projects/APPLaunch/main/ui/page_app/ui_app_lora.hpp` | `LORA` / `lora_100.png` | LoRa page, enabled on device |
| TANK | `projects/APPLaunch/main/ui/page_app/ui_app_tank_battle.hpp` | `TANK` / `tank_100.png` | Tank game, enabled on device |
| CLI/terminal | `projects/APPLaunch/main/ui/page_app/ui_app_st.hpp` | `CLI` / `cli_100.png` | `UISTPage`, used by bash, python, and `Terminal=true` apps |

Fixed entries are declared in `kBuiltinApps[]`:

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

## 4. External App Entry Table

| Item | Path/function | Description |
| --- | --- | --- |
| `.desktop` directory | `projects/APPLaunch/APPLaunch/applications/` | Development tree; packaged as `/usr/share/APPLaunch/applications/` |
| Template | `projects/APPLaunch/APPLaunch/applications/vim.desktop.temple` | Example template; not scanned because the suffix is not `.desktop` |
| Scan function | `Launch::applications_load()` in `projects/APPLaunch/main/ui/launch.cpp` | Parses `[Desktop Entry]`, `Name`, `Icon`, `Exec`, `Terminal`, and `Sysplause` |
| Directory watching | `Launch::inotify_init_watch()`, `app_dir_watch_cb()` | Watches application changes and refreshes the dynamic app list |
| Dynamic refresh | `Launch::applications_reload()` | Keeps fixed apps, deletes dynamic apps, then rescans |
| Terminal launch | `Launch::launch_Exec_in_terminal()` | Creates `UISTPage` and executes the command |
| Non-terminal launch | `Launch::launch_Exec()` | Pauses LVGL and calls `cp0_process_exec_blocking()` |
| Device-side process execution | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_process.cpp` | fork, privilege lowering, long ESC press to exit, keyboard restore |
| PTY execution | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_pty.cpp` | Terminal page command execution and user selection |

Minimal `.desktop` template:

```ini
[Desktop Entry]
Name=MyApp
Exec=/usr/share/APPLaunch/bin/my_app
Terminal=false
Icon=share/images/my_app_100.png
Type=Application
```

## 5. Asset Entry Table

| Asset | Development path | Access method | Common issue |
| --- | --- | --- | --- |
| Home/app icons | `projects/APPLaunch/APPLaunch/share/images/` | `img_path("xxx.png")` | Device-side relative paths depend on working directory; `.desktop` should use `share/images/xxx.png` |
| Page images | `projects/APPLaunch/APPLaunch/share/images/` | `img_path("xxx.png")` or `cp0_file_path_c("xxx.png")` | Filename case must match exactly |
| Audio | `projects/APPLaunch/APPLaunch/share/audio/` | `audio_path("xxx.wav")` | Device-side audio path is absolute `/usr/share/APPLaunch/share/audio/` |
| Fonts | `projects/APPLaunch/APPLaunch/share/font/` | `launcher_fonts().get("xxx.ttf", size, style)` | Requires FreeType; font objects should be cached and reused |
| External binaries/scripts | `projects/APPLaunch/APPLaunch/bin/` | `.desktop` `Exec=/usr/share/APPLaunch/bin/xxx` | Watch execute permissions and dynamic library dependencies |
| External app descriptors | `projects/APPLaunch/APPLaunch/applications/` | Automatically scans `.desktop` | `.desktop.temple` is not scanned |
| Packaged libraries | `projects/APPLaunch/APPLaunch/lib/` | Loaded by programs or scripts | Watch runtime `LD_LIBRARY_PATH` or rpath |

Path resolution code:

| Platform | File | Focus |
| --- | --- | --- |
| Device | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_file.cpp` | Asset root fixed to `/usr/share/APPLaunch`; images return relative `share/images/` paths |
| SDL2 | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_file.cpp` | Infers `APPLaunch/share` from executable directory, current directory, and parent directory |
| C interface | `cp0_file_path_c()` | Returns a thread-local cached `const char *`, suitable for LVGL style APIs |
| C++ interface | `cp0_file_path()` | Returns `std::string`; recommended inside pages |

## 6. Settings and Persistence Entry Table

| Setting item | UI entry | Configuration key | Implementation location |
| --- | --- | --- | --- |
| App visibility toggle | SETTING -> Launcher | `AppDescriptor.config_key` | `save_app_toggle()` in `ui_app_setup.hpp`, `launcher_app_registry_is_enabled()` in `launch.cpp` |
| Brightness | SETTING -> Screen -> Brightness | `brightness` | `ui_app_setup.hpp`, `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_settings.cpp` |
| Screen-off timeout | SETTING -> Screen -> DarkTime | `dark_time` | `ui_app_setup.hpp` |
| Volume | SETTING -> Speaker -> Volume | `volume` | `ui_app_setup.hpp`, `cp0_volume_read/write()` |
| Camera resolution | SETTING -> Camera -> Resolution | `cam_resolution` | `ui_app_setup.hpp`, read by the camera page |
| Startup mode | Related selection in Settings page | `startup_mode` | `ui_app_setup.hpp` |
| USB extension port | SETTING -> ExtPort | `extport_usb` | `ui_app_setup.hpp` |
| 5V output | SETTING -> ExtPort | `extport_5vout` | `ui_app_setup.hpp` |
| External app runtime user | Manual configuration | `run_as_user` | `cp0_lvgl_process.cpp`, `cp0_lvgl_pty.cpp` |

Configuration implementation:

| File | Description |
| --- | --- |
| `ext_components/cp0_lvgl/include/cp0_lvgl_app.h` | Declarations for `cp0_config_get_int/set_int/get_str/set_str/save` |
| `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_config.cpp` | Device-side configuration read/write, saved to `/var/lib/applaunch/settings` |
| `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_config.cpp` | SDL2 compatibility implementation |
| `ext_components/cp0_lvgl/src/commount.c` | Applies saved brightness and volume at startup |

## 7. Build Entry Table

| Scenario | Command/file | Description |
| --- | --- | --- |
| Default SDL2 build | `projects/APPLaunch/SConstruct` automatically selects `linux_x86_sdl2_config_defaults.mk` | Default configuration on x86_64 development hosts |
| Explicit SDL2 build | `CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j8 --implicit-deps-changed` | Recommended for local development verification |
| Cross build | `CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8 --implicit-deps-changed` | x86 Linux to AArch64 device |
| Native device build | `CONFIG_DEFAULT_FILE=config_defaults.mk scons -j4 --implicit-deps-changed` | Device-side framebuffer/evdev configuration |
| macOS cross build | `CONFIG_DEFAULT_FILE=mac_cross_cp0_config_defaults.mk scons ...` | macOS to device |
| macOS/Darwin | `darwin_config_defaults.mk` | Darwin/SDL-related configuration |
| Main build script | `projects/APPLaunch/SConstruct` | Sets SDK, EXT_COMPONENTS, sysroot download |
| Component build script | `projects/APPLaunch/main/SConstruct` | Sources, dependencies, static files, git commit macro |
| APPLaunch configuration | `projects/APPLaunch/main/Kconfig` | Main project Kconfig |
| cp0_lvgl configuration | `ext_components/cp0_lvgl/Kconfig` | Platform adaptation component configuration |

## 8. Platform Adaptation Entry Table

| Capability | Header | Device implementation | SDL2/compat implementation |
| --- | --- | --- | --- |
| LVGL initialization | `ext_components/cp0_lvgl/include/hal_lvgl_bsp.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl.c` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl.c` |
| framebuffer/display | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_freambuffer.c` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_display.c` |
| Keyboard input | `ext_components/cp0_lvgl/include/keyboard_input.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_keyboard.c` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_keyboard.c` |
| File paths | `ext_components/cp0_lvgl/include/cp0_lvgl_file.hpp` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_file.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_file.cpp` |
| Process | `ext_components/cp0_lvgl/include/hal/hal_process.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_process.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_process.cpp` |
| PTY | `ext_components/cp0_lvgl/include/hal/hal_pty.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_pty.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_pty.cpp` |
| Audio | `ext_components/cp0_lvgl/include/hal/hal_audio.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_audio.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_audio.cpp` |
| Settings/brightness/volume | `ext_components/cp0_lvgl/include/hal/hal_settings.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_settings.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_settings.cpp` |
| Network/WiFi | `ext_components/cp0_lvgl/include/hal/hal_network.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_network.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_network.cpp` |
| Screenshot | `ext_components/cp0_lvgl/include/hal/hal_screenshot.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_screenshot.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_screenshot.cpp` |
| Camera | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_camera.cpp` | Device camera | `ext_components/cp0_lvgl/src/sdl/cp0_lvgl_camera.cpp` |

## 9. Debugging Command Quick Reference

| Purpose | Command |
| --- | --- |
| View current changes | `git status --short` |
| SDL2 build | `cd projects/APPLaunch && CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j8 --implicit-deps-changed` |
| SDL2 run | `cd projects/APPLaunch && ./dist/M5CardputerZero-APPLaunch` |
| Cross build | `cd projects/APPLaunch && CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8 --implicit-deps-changed` |
| View systemd status | `systemctl --user status APPLaunch.service --no-pager` |
| Follow logs | `journalctl --user -u APPLaunch.service -f` |
| View boot logs | `journalctl --user -u APPLaunch.service -b --no-pager` |
| Check assets | `find /usr/share/APPLaunch -maxdepth 3 -type f | sort` |
| Check `.desktop` files | `find /usr/share/APPLaunch/applications -maxdepth 1 -type f -name '*.desktop' -print -exec sed -n '1,80p' {} \;` |
| Check settings | `sudo cat /var/lib/applaunch/settings` |
| Check input devices | `ls -l /dev/input/by-path/ && sudo evtest` |
| Check external app processes | `ps -eo pid,ppid,pgid,stat,cmd | grep -E 'APPLaunch|sh -c|M5CardputerZero'` |
| Check dynamic libraries | `ldd /usr/share/APPLaunch/bin/my_app` |
| Check icon logs | `journalctl --user -u APPLaunch.service -b --no-pager | grep 'set panel icon'` |

## 10. Pre-/Post-change Checklist

| Stage | Check item |
| --- | --- |
| Before change | Run `git status --short` and confirm which files already have changes from others |
| After adding a page | Confirm the `.hpp` file is in `page_app/`, and the class name matches the registration in `launch.cpp` |
| After adding assets | Confirm files can be found in both the source tree and device `/usr/share/APPLaunch` |
| After adding `.desktop` | File suffix is `.desktop`, with `[Desktop Entry]`, `Name`, and `Exec` |
| After changing settings | `/var/lib/applaunch/settings` contains the correct key and has not exceeded the configuration entry limit |
| After build | SDL2 or cross build passes, with no unexpected auto-generated diff |
| After running on device | `journalctl` has no missing, skip, segfault, or permission denied messages |
| After external app changes | The app exits normally or returns home via long ESC press |
