# 02 - Runtime Framework and Boot Flow

This chapter explains the full path from the APPLaunch process entry point to the first frame of the home screen. Key references are `projects/APPLaunch/main/src/main.cpp`, `projects/APPLaunch/main/ui/ui.cpp`, `projects/APPLaunch/main/ui/launcher_ui_runtime.cpp`, and `projects/APPLaunch/main/ui/ui_launch_page.cpp`.

## 1. Runtime Framework Overview

APPLaunch is a single-process LVGL application. The main thread performs platform initialization, creates UI objects, refreshes the first frame, and then enters a loop driven by `lv_timer_handler()`.

```text
APPLaunch process
├── main.cpp
│   ├── lv_init()
│   ├── cp0_lvgl_init()
│   ├── lv_event_register_id()
│   ├── ui_init()
│   └── while (1)
│       ├── APPLaunch_lock()
│       ├── lv_timer_handler()
│       └── usleep(5000)
└── ui_init()
    └── LauncherUiRuntime()
        ├── create_display()
        ├── Create Launch / UILaunchPage bound objects
        └── build_launcher_home()
```

Core characteristics:

- LVGL initialization and platform adaptation initialization are executed only once in `main()`.
- The home UI is created under the control of `LauncherUiRuntime`; the actual objects are created in `UILaunchPage::create_screen()`.
- `Launch` / `Launch` is responsible for the application list, launch modes, status bar refresh, and dynamic application directory watching.
- Immediately after `ui_init()`, the first home frame is forced to refresh through `lv_obj_invalidate()` + `lv_refr_now(NULL)`, avoiding a black screen while waiting for the next natural refresh after startup.

## 2. Entry Files and Key Source Paths

| Path | Role |
| --- | --- |
| `projects/APPLaunch/main/src/main.cpp` | Process entry point, LVGL main loop, and external-application runtime lock detection |
| `projects/APPLaunch/main/ui/ui.cpp` | `ui_init()`, creates the global `LauncherUiRuntime home` |
| `projects/APPLaunch/main/ui/launcher_ui_runtime.cpp` | Sets the LVGL theme, creates the home screen, and creates Launch bound objects |
| `projects/APPLaunch/main/ui/ui_launch_page.cpp` | Home screen, startup GIF, home loading, and input group |
| `projects/APPLaunch/main/ui/launch.cpp` | Application manager; launches external/terminal/built-in pages and owns the status bar timer |
| `ext_components/cp0_lvgl` | Wrappers for `cp0_lvgl_init()`, file paths, input, processes, and system capabilities |

## 3. `main()` Boot Flow

The framework code for `main()` is as follows:

```cpp
int main(void)
{
    static const std::string default_lock_file = cp0_file_path("lock_file");
    lock_file = default_lock_file.c_str();

    lv_init();
    cp0_lvgl_init();

    if (LV_EVENT_KEYBOARD == 0)
        LV_EVENT_KEYBOARD = lv_event_register_id();

    ui_init();

    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);

    while (1) {
        APPLaunch_lock();
        lv_timer_handler();
        usleep(5000);
    }
}
```

### 3.1 Initialization Phase

1. `cp0_file_path("lock_file")` resolves the runtime lock file path.
2. `lv_init()` initializes LVGL core objects, memory, timers, and display/indev abstractions.
3. `cp0_lvgl_init()` initializes the platform layer: display, input, framebuffer/SDL, system signals, and other capabilities.
4. `lv_event_register_id()` registers the custom keyboard event `LV_EVENT_KEYBOARD`.
5. `ui_init()` enters APPLaunch's own UI construction flow.

### 3.2 First-Frame Refresh

After `ui_init()` returns, the code immediately executes:

```cpp
lv_obj_invalidate(lv_scr_act());
lv_refr_now(NULL);
```

The purpose of this step is not an ordinary refresh, but forcing the current active screen content to be flushed to the framebuffer/SDL window. When the home objects have just been created, relying only on later `lv_timer_handler()` calls may briefly show a black screen; forcing the first frame makes startup behavior more deterministic.

### 3.3 Main Loop

The main loop runs at a 5 ms cadence:

```text
Each loop iteration
  -> APPLaunch_lock()
  -> lv_timer_handler()
  -> sleep 5ms
```

- `APPLaunch_lock()` checks whether an external application has occupied the foreground.
- `lv_timer_handler()` drives LVGL timers, animations, input events, and redraws.
- `usleep(5000)` controls CPU usage and refresh cadence.

## 4. From `ui_init()` to Home Object Creation

`ui_init()` is located in `projects/APPLaunch/main/ui/ui.cpp`:

```cpp
std::unique_ptr<LauncherUiRuntime> home;

void ui_init(void)
{
    home = std::make_unique<LauncherUiRuntime>();
}
```

The `LauncherUiRuntime` constructor continues with:

```cpp
LauncherUiRuntime::LauncherUiRuntime()
{
    create_display();

    launch_ = std::make_shared<Launch>();
    launch_page_ = std::make_shared<UILaunchPage>(launch_);
    launch_->set_launch_page(launch_page_);

    build_launcher_home();
}
```

Pay attention to the order here:

1. `create_display()` first creates the font manager and sets the LVGL theme.
2. It constructs `Launch` and `UILaunchPage`, then establishes the two-way collaboration relationship through `Launch::set_launch_page()`.
3. `build_launcher_home()` creates the home screen, calls `Launch::bind_ui()` to build the application list, initializes the input group, and displays either the home screen or the startup GIF.

## 5. Display / Theme Initialization

The core code of `LauncherUiRuntime::create_display()`:

```cpp
void LauncherUiRuntime::create_display()
{
    fonts_ = std::make_shared<LauncherFonts>();

    dispp_ = lv_disp_get_default();
    theme_ = lv_theme_default_init(
        dispp_,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        false,
        LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp_, theme_);
}
```

Notes:

- `LauncherFonts` is the FreeType font cache shared by the home screen and pages. Its entry function is `launcher_fonts()`.
- `lv_disp_get_default()` depends on `cp0_lvgl_init()` having already registered the display device.
- The theme is only the base theme. Most home controls still have their sizes, colors, background images, and fonts set manually in `ui_launch_page.cpp`.

## 6. Home Creation and Display Flow

`LauncherUiRuntime::build_launcher_home()` is the main entry point for displaying the home screen:

```cpp
void LauncherUiRuntime::build_launcher_home()
{
    LV_EVENT_GET_COMP_CHILD = lv_event_register_id();

    launch_page_->create_screen();
    launch_->bind_ui();
    launch_page_->init_input_group();

#ifndef APPLAUNCH_STARTUP_ANIMATION
    launch_page_->load_home_screen();
#else
#ifdef HAL_PLATFORM_SDL
    launch_page_->load_home_screen();
#else
    const char *gif_path = cp0_file_path_c("logo_output.gif");
    FILE *gif_file = fopen(gif_path, "r");
    if (gif_file) {
        fclose(gif_file);
        launch_page_->start_startup_gif();
    } else {
        launch_page_->load_home_screen();
    }
#endif
#endif
}
```

### 6.1 Home Screen Creation

`UILaunchPage` inherits `home_base`, so the root screen, top status bar, content container, and input group are prepared by the shared page framework. `UILaunchPage::create_screen()` only fills the home content container and runs once:

```cpp
void UILaunchPage::create_screen()
{
    if (carousel_elements[kCardCenter])
        return;

    create_app_container(content_container());
}
```

It creates the home carousel area: 5 cards, 5 titles, 5 page dots, and left/right arrows. The top-left logo, WiFi indicator, time label, and battery bar are created by `home_base::creat_Top_UI()`.

### 6.2 Input Group Binding

The home input group comes from `AppPageRoot::input_group()`. `UILaunchPage::init_input_group()` stores it in the compatibility bridge and binds the active keyboard input device:

```cpp
void UILaunchPage::init_input_group()
{
    ::home_input_group = input_group();
    bind_home_input_group();
}
```

This allows keyboard events to be delivered to `screen()`, where the LVGL callback `on_home_key()` dispatches to `handle_home_key()` for left/right switching and Enter launch.

### 6.3 Startup GIF and Home Display

When `APPLAUNCH_STARTUP_ANIMATION` is enabled and the platform is not SDL:

```text
Check cp0_file_path_c("logo_output.gif")
  -> file exists: UILaunchPage::start_startup_gif()
  -> file does not exist: UILaunchPage::load_home_screen()
```

`start_startup_gif()` creates an independent GIF screen and binds the callback with `this`:

```cpp
startup_gif_ = lv_gif_create(NULL);
lv_gif_set_src(startup_gif_, startup_gif_path_.data());
lv_obj_center(startup_gif_);
lv_obj_add_event_cb(startup_gif_, on_startup_gif_event, LV_EVENT_ALL, this);
lv_disp_load_scr(startup_gif_);
```

When GIF playback finishes, it receives `LV_EVENT_READY`. `on_startup_gif_event()` returns to the owning `UILaunchPage` instance and `handle_startup_gif_event()` pauses the GIF and loads the home screen once:

```cpp
if (event_code == LV_EVENT_READY && !startup_gif_done_) {
    startup_gif_done_ = true;
    if (startup_gif_) lv_gif_pause(startup_gif_);
    load_home_screen();
}
```

Responsibilities of `load_home_screen()`:

```cpp
show_home_screen();
cp0_signal_audio_api_play_asset("startup.mp3");
```

## 7. Boot Sequence Text

```text
main()
  -> cp0_file_path("lock_file")
  -> lv_init()
  -> cp0_lvgl_init()
  -> register LV_EVENT_KEYBOARD
  -> ui_init()
      -> new LauncherUiRuntime
          -> create_display()
              -> new LauncherFonts
              -> lv_disp_get_default()
              -> lv_theme_default_init()
          -> new Launch
          -> new UILaunchPage(Launch)
          -> Launch::set_launch_page()
          -> build_launcher_home()
              -> register LV_EVENT_GET_COMP_CHILD
              -> launch_page_->create_screen()
                  -> home_base::creat_Top_UI()
                  -> create_app_container(content_container())
              -> launch_->bind_ui()
                  -> new Launch
                  -> Register fixed/dynamic applications and write them into home slots
                  -> Create status bar and application directory watch timers
              -> launch_page_->init_input_group()
              -> load_home_screen() or start_startup_gif()
  -> lv_obj_invalidate(lv_scr_act())
  -> lv_refr_now(NULL)
  -> while forever
      -> APPLaunch_lock()
      -> lv_timer_handler()
      -> usleep(5000)
```

## 8. External Application Runtime Lock `APPLaunch_lock()`

`APPLaunch_lock()` coordinates the foreground rendering relationship between APPLaunch and external independent processes.

```cpp
void APPLaunch_lock()
{
    int holder_pid = 0;
    cp0_process_check_lock(lock_file, &holder_pid);

    if (holder_pid == 0) {
        LVGL_RUN_FLAGE = 1;
        lv_obj_invalidate(lv_scr_act());
    } else {
        if (LVGL_HOME_KEY_FLAG) {
            // Kill the external application after HOME is held for 5 seconds.
            cp0_process_kill(holder_pid, 3000);
        }
        LVGL_RUN_FLAGE = 0;
    }
}
```

The actual code has several state variables:

- `lvgl_lock`: avoids repeatedly restoring LVGL refresh in every loop; it performs `invalidate` once after the lock is released.
- `home_back_status` / `start_time`: track how long the HOME key has been held.
- `holder_pid`: the PID of the external process currently holding the lock file.

Logic:

```text
No external application holds the lock
  -> APPLaunch restores LVGL_RUN_FLAGE=1
  -> If just recovered from the locked state, redraw the current screen

An external application holds the lock
  -> APPLaunch sets LVGL_RUN_FLAGE=0 and pauses its own rendering
  -> If the HOME key has been held for >= 5 seconds, try to kill the external application
```

## 9. Notes

- `ui_init()` already creates and may load the home screen internally. The later `lv_refr_now(NULL)` in `main()` is a first-frame safeguard and should not be removed casually.
- `cp0_lvgl_init()` must run before `ui_init()`, otherwise `lv_disp_get_default()`, input devices, paths, and system interfaces may not be ready.
- The SDL platform skips the startup GIF by default; only the device checks and plays `logo_output.gif`.
- Home input must be rebound through `UILaunchPage::bind_home_input_group()`; returning to the home screen from a built-in page or terminal page must also restore this group.
- While an external independent application is running, it sets `LVGL_RUN_FLAGE=0`; do not assume APPLaunch will continue refreshing the UI during this period.
- `APPLaunch_lock()` depends on cooperation between `cp0_process_exec_blocking()` and the lock file. If an external application exits abnormally but the lock is not released, the home screen may appear not to refresh; investigate the lock file and holder PID.
