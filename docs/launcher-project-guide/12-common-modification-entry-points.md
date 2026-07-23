# 12 - Common Modification Entry Points

Use this index to start from the current owner of each behavior.

| Change | Primary files | Verification |
| --- | --- | --- |
| Change shared process startup or LVGL loop behavior | `ext_components/cp0_lvgl`, `main/src/main.cpp` | Runner starts and teardown executes |
| Change launcher UI construction | `main/ui/launcher_ui_runtime.*`, `main/ui/ui.cpp` | Startup animation and home both load |
| Change carousel layout | `main/ui/ui_launch_page_carousel_view.cpp` | Five slots and five page dots remain stable |
| Change carousel navigation | `main/ui/ui_launch_page.cpp`, `main/ui/ui_launch_page_input.cpp`, `main/ui/model/launcher_navigation_model.hpp` | Keyboard, arrows, wraparound, and launch selection agree |
| Change startup animation or sound | `main/ui/ui_launch_page_startup.cpp` | Both enabled and skipped startup paths reach home |
| Add or reorder a fixed entry | `main/ui/builtin_app_registry.cpp` | Registry order matches the carousel order |
| Change application visibility | `main/ui/app_registry.*`, `main/ui/page_app/ui_app_setup*` | Toggle, reload, and restart preserve the setting |
| Change `.desktop` discovery | `main/ui/desktop_app_loader.*`, `main/ui/app_directory_watcher.*` | Add, edit, and remove a desktop file |
| Change internal page launch/return | `main/ui/launch.h`, `main/ui/launch.cpp` | Repeated Enter/ESC cannot overlap transitions |
| Change external process handoff | `main/ui/launch.cpp`, shared process service | Timers, input, home, and screen saver recover after exit |
| Add an internal page | `main/ui/page_app/`, `main/ui/builtin_app_registry.cpp` | Build, open, interact, and return home |
| Change standard page chrome | `ext_components/cp0_lvgl/include/ui_app_page.hpp`, `ext_components/cp0_lvgl/src/ui_app_page.cpp` | Title, status information, and input group remain correct |

## Fixed Registry

The fixed table is `BUILTIN_APPS` in `builtin_app_registry.cpp`. Current entries are Python, STORE, CLI, GAME, SETTING, MATH, LORA, plus IP_PANEL, SSH, and TANK on Linux device builds.

Use `append_page_app<PageT>` for internal pages. Use a null appender plus a command for terminal or standalone entries. A command beginning with `@` is resolved through `launcher_platform::path()`.

## Page Source Map

| Area | Files |
| --- | --- |
| Terminal | `page_app/ui_app_st.*`, parser, renderer, session, viewport modules |
| Game menu | `page_app/ui_app_game.*`, `ui_app_game_view.cpp` |
| Settings | `page_app/ui_app_setup.*`, input, layout, views modules |
| LoRa and mesh support | `page_app/ui_app_lora.*`, `page_app/ui_app_mesh.*` |
| IP panel | `page_app/ui_app_ip_panel.*` |
| SSH | `page_app/ui_app_ssh.*` |
| Tank game | `page_app/ui_app_tank_battle.*` |

## Runtime Checklist

When changing startup or page switching, verify:

1. `cp0_lvgl_run()` still owns shared initialization and the event loop.
2. `LauncherUiRuntime` owns both `Launch` and `UILaunchPage`.
3. Internal pages are retained until the asynchronous home callback completes.
4. External execution disables and restores timers, input, foreground state, and the home screen symmetrically.
5. Registry changes rebuild the list before refreshing the carousel.

## Resource Paths

Runtime assets live under `projects/APPLaunch/APPLaunch/` and are installed under `/usr/share/APPLaunch/` on the device. Fixed registry icons name files in `share/images/`; path resolution belongs to `launcher_platform`.

Do not edit generated headers under a build directory. Add page headers to the source layout expected by the generator, then rebuild.
