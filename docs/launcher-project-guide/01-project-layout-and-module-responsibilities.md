# 01 - Project Layout and Module Responsibilities

This chapter explains the overall repository structure and the internal structure of the APPLaunch project.

## 1. Overall Repository Structure

```text
launcher/
├── SDK/
├── ext_components/
├── projects/
├── docs/
├── README.md
└── README_ZH.md
```

### 1.1 `SDK/`

`SDK` is `M5Stack_Linux_Libs`, which provides the project with:

- The SCons/Kconfig build framework.
- LVGL components.
- Device drivers, utility functions, and example code.
- Build scripts and the component registration mechanism.

APPLaunch's `SConstruct` sets:

```python
os.environ["SDK_PATH"] = str(sdk_path)
```

Then it calls:

```python
env = SConscript(
    str(sdk_path / "tools" / "scons" / "project.py"),
    variant_dir=os.getcwd(),
    duplicate=0,
)
```

### 1.2 `ext_components/`

`ext_components` is the repository's extension component directory. APPLaunch includes it through `EXT_COMPONENTS_PATH`.

```text
ext_components/
├── cp0_lvgl/
├── Miniaudio/
├── RadioLib/
└── Sigslot/
```

| Component | Role |
| --- | --- |
| `cp0_lvgl` | CardputerZero platform adaptation; wraps LVGL initialization, file paths, input, processes, PTY, and system capabilities |
| `Miniaudio` | Dependency for audio playback and recording |
| `Sigslot` | Signal-slot mechanism |
| `RadioLib` | LoRa/SX126x wireless communication library component |

### 1.3 `projects/`

```text
projects/
├── APPLaunch/
├── AppStore/
├── Calculator/
├── CardputerZero-Emulator/
├── HelloWorld/
└── UserDemo/
```

| Project | Description |
| --- | --- |
| `APPLaunch` | Main launcher; the focus of this documentation |
| `AppStore` | Application store; can be launched by APPLaunch as an external application |
| `Calculator` | Calculator application; can be launched by APPLaunch |
| `CardputerZero-Emulator` | Device emulator |
| `HelloWorld` | Minimal example project for learning the build flow |
| `UserDemo` | User demo project |

### 1.4 `docs/`, `scripts/`, and Runtime Helpers

- `docs/`: developer-facing documentation and standalone packaging docs, including `APPLaunch-App-打包指南.md`.
- `scripts/`: repository-level helper tools, such as `firmware_manager.py` and `debian_packager.py`.
- `projects/APPLaunch/APPLaunch/bin/`: APPLaunch runtime helper scripts copied into `/usr/share/APPLaunch/bin/`, including `store_cache_sync.py`.

## 2. APPLaunch Top-Level Structure

```text
projects/APPLaunch/
├── APPLaunch/
├── main/
├── tools/
├── docs/
├── SConstruct
├── config_defaults.mk
├── linux_x86_sdl2_config_defaults.mk
├── linux_x86_cross_cp0_config_defaults.mk
├── mac_cross_cp0_config_defaults.mk
├── darwin_config_defaults.mk
└── setup.ini
```

### 2.1 Top-Level Build Files

| File | Description |
| --- | --- |
| `SConstruct` | Project entry point; selects the default configuration, SDK path, cross-compilation sysroot, and invokes the SDK build system |
| `config_defaults.mk` | Default on-device configuration; enables Linux framebuffer / evdev |
| `linux_x86_sdl2_config_defaults.mk` | Linux x86 SDL2 simulation configuration |
| `linux_x86_cross_cp0_config_defaults.mk` | Linux x86 cross-compilation configuration for AArch64 |
| `mac_cross_cp0_config_defaults.mk` | macOS cross-compilation configuration for AArch64 |
| `darwin_config_defaults.mk` | macOS SDL / Darwin-related configuration |

### 2.2 `APPLaunch/` Runtime Resource Tree

```text
projects/APPLaunch/APPLaunch/
├── applications/
│   └── vim.desktop.temple
├── bin/
│   └── store_cache_sync.py
├── lib/
│   └── nihao.so
└── share/
    ├── audio/
    ├── font/
    └── images/
```

This directory is copied into the runtime directory during build/package creation. After installation on the device, it maps to:

```text
/usr/share/APPLaunch/
```

Responsibilities of the resource tree:

- `applications/`: stores `.desktop` description files for external applications.
- `share/images/`: application icons, home carousel images, status bar images, and page images.
- `share/audio/`: startup sound, key sound, and switch sound.
- `share/font/`: TTF fonts.
- `lib/`: library files shipped with the package.

### 2.3 `main/` Main Source Directory

```text
projects/APPLaunch/main/
├── Kconfig
├── SConstruct
├── include/
├── src/
└── ui/
```

| Path | Description |
| --- | --- |
| `Kconfig` | Component configuration entry point |
| `SConstruct` | Registers the APPLaunch build target and dependencies |
| `include/` | APPLaunch private headers and compatibility headers |
| `src/main.cpp` | Process entry point; supplies launcher setup/teardown callbacks to the shared `cp0_lvgl_run()` runner |
| `ui/` | Implementations for all UI pages, the home screen, animations, Loading, and more |

### 2.4 `main/ui/` UI Directory

```text
main/ui/
├── ui.cpp / ui.h
├── launch.cpp / launch.h
├── builtin_app_registry.cpp / .hpp
├── desktop_app_loader.cpp / .hpp
├── launcher_ui_runtime.cpp / .h
├── ui_launch_page.cpp / .h
├── ui_launch_page_carousel_view.cpp
├── ui_launch_page_input.cpp
├── ui_launch_page_startup.cpp
├── launcher_ui_app_page.hpp
├── generate_page_app_includes.py
├── ui_loading.*
├── ui_global_hint.*
├── animation/
├── model/
└── page_app/
```

| File/Directory | Role |
| --- | --- |
| `ui.c` / `ui.cpp` / `ui.h` | UI initialization, global objects, and the C/C++ bridge |
| `launch.cpp` | Application manager; owns launch, return-home, list reload, and process foreground handoff |
| `builtin_app_registry.cpp` | Fixed application descriptors and typed page/command registration |
| `desktop_app_loader.cpp` | Dynamic `.desktop` discovery and parsing |
| `launcher_ui_runtime.cpp` | Owns and connects `Launch` and `UILaunchPage` |
| `ui_launch_page*.cpp` | Split home implementation: lifecycle, carousel view, input, and startup behavior |
| `ui_loading.cpp` | Loading overlay |
| `ui_global_hint.cpp` | Global hints |
| `animation/` | Home carousel animation implementation |
| `model/` | Testable page and launcher state models |
| `page_app/` | Internal page implementations |

### 2.5 `page_app/` Built-In Page Directory

```text
main/ui/page_app/
├── ui_app_st.*
├── ui_app_game.*
├── ui_app_lora.*
├── ui_app_mesh.*
├── ui_app_setup.*
├── ui_app_ssh.*
├── ui_app_tank_battle.*
└── ui_app_ip_panel.*
```

Pages use ordinary headers and implementation files. Larger pages split view, input, parser, session, or transport behavior into separate translation units. `generate_page_app_includes.py` produces the page include aggregate during the build.

## 3. Module Dependencies

Simplified dependency graph:

```text
main.cpp
  ├── ui/ui.h
  ├── cp0_lvgl_app.h
  ├── cp0_lvgl_file.hpp
  └── hal_lvgl_bsp.h

ui_init()
  ├── UILaunchPage
  ├── Launch
  ├── ui_loading
  └── page_app/*

Launch
  ├── UILaunchPage::panel()/label()
  ├── append_page_app<PageT> / page_v<PageT>
  ├── cp0_file_path()
  ├── cp0_process_*
  ├── cp0_dir_watch_*
  ├── cp0_wifi_*
  └── cp0_battery_*
```

## 4. Code Style Characteristics

APPLaunch currently has several clear code style characteristics:

- Mixed C and C++: LVGL-generated/compatibility code is often C, while most business pages are C++.
- LVGL callbacks remain C-style static functions, but page dispatch uses `lv_event_get_user_data()` to recover the owning C++ page instance.
- Page classes usually construct LVGL objects directly without using an additional UI framework.
- Hardware capabilities are preferably accessed through the unified interfaces wrapped by `cp0_lvgl`.
- Resource access should preferably use `cp0_file_path()` to avoid path differences between the device and SDL environments.
