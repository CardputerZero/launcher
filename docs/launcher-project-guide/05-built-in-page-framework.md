# 05 - Built-in Page Framework

Built-in pages are LVGL page objects compiled into APPLaunch. They use the shared `cp0_lvgl` page runner and return to the launcher through a callback owned by `Launch`.

## 1. Page Base Classes

`ext_components/cp0_lvgl/include/ui_app_page.hpp` provides the shared page abstraction. `AppPage` supplies the standard layout and top bar; `AppPageRoot` is available for pages that own the full screen. APPLaunch adds its home compatibility shell in `projects/APPLaunch/main/ui/launcher_ui_app_page.hpp`.

The launcher home also uses the page shell through `home_base` in `launcher_ui_app_page.hpp`. `UILaunchPage` extends it with carousel, startup, and launcher-specific input behavior.

## 2. Starting and Stopping a Page

The typed application callback in `launch.h` constructs the page, stores it in `Launch::app_Page`, assigns `navigate_home`, and calls:

```cpp
cp0_lvgl_start_app_page(*page);
```

Page switching, input-group activation, and screen loading are shared runner responsibilities. A page requests return through `navigate_home`; it must not destroy itself or directly reconstruct the launcher home.

Timers, worker threads, callbacks, and external handles created by a page must be stopped in its destructor. Event callbacks should verify that their page and root objects are still active before updating LVGL state.

## 3. Current Internal Pages

| Class | Implementation | Registry label | Notes |
| --- | --- | --- | --- |
| `UISTPage` | `ui_app_st.*` plus parser, renderer, session, and viewport modules | CLI | Interactive terminal; also hosts terminal-command entries |
| `UIGamePage` | `ui_app_game.*` and `ui_app_game_view.cpp` | GAME | Game menu |
| `UISetupPage` | `ui_app_setup.*`, input, layout, and views modules | SETTING | Settings and application visibility |
| `UILoraPage` | `ui_app_lora.*` and view module | LORA | LoRa UI |
| `UIIpPanelPage` | `ui_app_ip_panel.*` and view module | IP_PANEL | Device-only network panel |
| `UISSHPage` | `ui_app_ssh.*` and view module | SSH | Device-only SSH page |
| `UITankBattlePage` | `ui_app_tank_battle.*` and view module | TANK | Device-only full-screen game |

`ui_app_mesh.*` remains a source module used by LoRa-related UI, but it is not a separately registered home entry.

Python is a terminal command. STORE and MATH are standalone processes. They are fixed launcher entries, but they are not built-in page classes.

## 4. Registration

`builtin_app_registry.cpp` owns `BUILTIN_APPS`. Internal registrations use `append_page_app<PageT>`; command registrations leave the appender null. `launcher_append_enabled_builtin_apps()` filters descriptors through the Settings registry and appends enabled entries.

The first five visible carousel objects are view slots, not a fixed five-entry application table. They are continuously remapped around `Launch::current_app`, so adding or hiding an application requires only rebuilding the application list and refreshing the carousel.

## 5. Source Organization

Pages are implemented as normal headers and source files under `main/ui/page_app/`. Larger pages are split by responsibility:

- `*_view.cpp` creates and updates LVGL objects.
- input modules translate LVGL events into model operations.
- parser, session, or transport modules own non-visual behavior.
- `main/ui/model/` contains logic that can be tested without an LVGL display.

The build generates `generated/page_app.h` as an include aggregate. Do not document or edit a path under `build/generated`; that directory is build output and may not exist before configuration.

## 6. Adding a Page

1. Add the page header and source files under `main/ui/page_app/`.
2. Derive from the appropriate page base and create the page contents through its lifecycle hooks.
3. Stop page-owned timers, callbacks, threads, and handles during destruction.
4. Add an icon under `APPLaunch/share/images/`.
5. Add an `AppDescriptor` and `append_page_app<PageT>` entry in `builtin_app_registry.cpp`.
6. Add model tests for navigation or state transitions and build the SDL target for visual verification.

## 7. Home Transition Rules

- Call `navigate_home` to leave the page.
- Do not release the active page from inside its own LVGL event callback.
- Do not bind the home input group directly; `Launch::lv_go_back_home()` and `UILaunchPage::show_home_screen()` restore it.
- Use the shared loading overlay before expensive synchronous construction.
- Keep LVGL calls on the UI thread; marshal worker results before touching objects.
