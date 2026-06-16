# 04 - Application Model and Launch Mechanism

This chapter explains how APPLaunch unifies built-in pages, terminal commands, and external standalone programs into one application list, and how an application is launched after the user presses Enter. Key references are `projects/APPLaunch/main/ui/launch.cpp`, `projects/APPLaunch/main/ui/launch.h`, `projects/APPLaunch/main/ui/ui_launch_page.cpp`, and `projects/APPLaunch/main/ui/page_app/*`.

## 1. Application Model Overview

APPLaunch abstracts every home-screen entry as an `app`:

```text
app
├── Name  display title
├── Icon  icon path
├── Exec  external command; can be empty for built-in pages
└── launch(Launch*)  launch action
```

After this unification, the home carousel does not need to care about application type. It only displays `Name` and `Icon`; when Enter is pressed, it simply calls the current `app.launch()`.

```text
Home center card
  -> Launch::launch_app()
  -> Launch::launch_app()
  -> app.launch(this)
      ├── Built-in page: new PageT + lv_disp_load_scr()
      ├── Terminal app: UISTPage + PTY exec()
      └── External app: cp0_process_exec_blocking()
```

## 2. Key Source Paths

| Path | Description |
| --- | --- |
| `projects/APPLaunch/main/ui/launch.h` | Public `Launch` interface and app model declarations |
| `projects/APPLaunch/main/ui/launch.cpp` | `app`, `Launch`, application list, launch logic, `.desktop` scanning |
| `projects/APPLaunch/main/ui/ui_launch_page.cpp` | Forwards Enter / click events to `Launch::launch_app()` |
| `projects/APPLaunch/main/ui/page_app/ui_app_st.hpp` | Terminal page `UISTPage` |
| `projects/APPLaunch/main/ui/page_app/*.hpp` | Built-in pages such as settings, game, file, camera, and LoRa |
| `projects/APPLaunch/APPLaunch/applications/` | Runtime `.desktop` application descriptor directory |
| `ext_components/cp0_lvgl` | Lower-level capabilities such as process launch, PTY, directory watching, and path resolution |

## 3. `Launch` Runtime State

`launch.h` exposes the `Launch` class directly. Current code no longer has a separate `LaunchImpl` layer; the application list, directory watcher, current page holder, and carousel helpers all live in `Launch`.

Important private state includes:

| Field | Description |
| --- | --- |
| `launch_page_` | Weak reference to the home `UILaunchPage` |
| `current_app` | Application index corresponding to the current center card. Defaults to `2`, so the initial center card is CLI |
| `dir_watcher_` / `watch_timer_` | Watches the `applications/` directory and reloads dynamic apps |
| `fixed_count` | Number of built-in/fixed applications. Dynamic reload keeps the elements before this point |
| `app_list` | Built-in entries plus dynamic `.desktop` entries |
| `app_Page` | Lifetime holder for the current built-in page or terminal page |

`Launch::bind_ui()` builds the initial list, loads dynamic `.desktop` files, starts the directory watcher timer, and registers the app-registry change callback.

## 4. `app` Structure and Three Launch Modes

`app` is defined in `launch.cpp`:

```cpp
struct app
{
    std::string Name;
    std::string Icon;
    std::string Exec;

    std::function<void(Launch *)> launch;

    app(std::string name, std::string icon, std::string exec, bool terminal);
    app(std::string name, std::string icon, std::string exec, bool terminal, bool sysplause);
    app(std::string name, std::string icon, std::string exec, bool terminal, bool sysplause, bool run_as_root);

    template <class PageT>
    app(std::string name, std::string icon, page_t<PageT> tag);
};
```

Three application categories:

| Type | Construction | Launch function | Examples |
| --- | --- | --- | --- |
| Built-in page | `page_v<PageT>` / `append_page_app<PageT>` | Constructs a page and calls `lv_disp_load_scr()` | `CLI`, `GAME`, `SETTING`, `Compass` |
| Terminal command | `exec, terminal=true` | `launch_Exec_in_terminal()` creates `UISTPage` and calls `exec()` | `Python`, `Terminal=true` `.desktop` apps |
| External process | `exec, terminal=false` | `launch_Exec()` | AppStore, Calculator |

## 5. Fixed Application Registration

Built-in entries are declared in `launch.cpp` as `kBuiltinApps[]`. Each entry carries an `AppDescriptor` with the label, icon, config key, whether it is configurable in Settings, and whether it is always on.

Representative entries:

```cpp
constexpr BuiltinAppRegistration kBuiltinApps[] = {
    {{"Python", "python_100.png", "app_Python", false, true}, "python3", true, false, false, nullptr},
    {{"STORE", "store_100.png", "app_Store", false, true},
     "/usr/share/APPLaunch/bin/M5CardputerZero-AppStore", false, true, true, nullptr},
    {{"CLI", "cli_100.png", "app_CLI", false, true}, nullptr, false, true, false, append_page_app<UISTPage>},
    {{"GAME", "game_100.png", "app_Game", false, true}, nullptr, false, true, false, append_page_app<UIGamePage>},
    {{"SETTING", "setting_100.png", "app_Setting", false, true}, nullptr, false, true, false, append_page_app<UISetupPage>},
    {{"MATH", "math_100.png", "app_Math", true, false},
     "/usr/share/APPLaunch/bin/M5CardputerZero-Calculator", false, true, false, nullptr},
};
```

`Launch::rebuild_builtin_apps()` clears the list, appends enabled built-ins by calling `launcher_app_registry_is_enabled()`, and updates `fixed_count`. Settings changes are saved through `launcher_app_registry_set_enabled()` and then trigger `Launch::applications_reload()`.

The first five entries initialize the 5-slot home carousel:

```text
slot 0 far-left : Python
slot 1 left     : STORE
slot 2 center   : CLI
slot 3 right    : GAME
slot 4 far-right: SETTING
current_app     : 2
```

## 6. Built-in Page Launch Mechanism

Built-in pages are constructed through the template constructor:

```cpp
template <class PageT>
app::app(std::string name, std::string icon, page_t<PageT>)
    : Name(std::move(name)), Icon(std::move(icon))
{
    launch = [](Launch *self)
    {
        ui_loading_show("Loading...");
        lv_refr_now(NULL);

        auto p = std::make_shared<PageT>();
        self->app_Page = p;
        lv_disp_load_scr(p->screen());
        lv_indev_set_group(lv_indev_get_next(NULL), p->input_group());
        p->navigate_home = std::bind(&Launch::go_back_home, self);

        ui_loading_hide();
    };
}
```

Built-in pages must follow these conventions:

- The page class can be constructed without arguments.
- It provides `screen()` to return the page's root screen.
- It provides `input_group()` to return the page's own input group.
- It provides or inherits the `navigate_home` callback for returning to the home screen.

Launch sequence:

```text
Enter
  -> app.launch(Launch*)
  -> ui_loading_show("Loading...")
  -> lv_refr_now(NULL)
  -> make_shared<PageT>()
  -> app_Page = p keeps the lifetime
  -> lv_disp_load_scr(p->screen())
  -> Input device switches to p->input_group()
  -> p->navigate_home = Launch::go_back_home
  -> ui_loading_hide()
```

## 7. Terminal Application Launch Mechanism

Terminal applications use `UISTPage`, and the external command runs inside a terminal page in the APPLaunch process:

```cpp
void launch_Exec_in_terminal(const std::string &exec, bool sysplause = true)
{
    ui_loading_show("Loading...");
    lv_refr_now(NULL);

    auto p = std::make_shared<UISTPage>();
    app_Page = p;
    lv_disp_load_scr(p->screen());
    lv_indev_set_group(lv_indev_get_next(NULL), p->input_group());
    p->navigate_home = std::bind(&Launch::go_back_home, this);
    p->terminal_sysplause = sysplause;

    ui_loading_hide();
    p->exec(exec);
}
```

Typical entries:

```text
Python -> exec = "python3", terminal = true
CLI    -> built-in UISTPage, starts interactive bash internally
```

Compared with built-in pages, terminal applications add one extra step: `p->exec(exec)`. They usually interact with the command through a PTY. What the user sees is `UISTPage`, not a separate UI outside APPLaunch.

## 8. External Standalone Application Launch Mechanism

External applications use `cp0_process_exec_blocking()`:

```cpp
void launch_Exec(const std::string &exec, bool keep_root = false)
{
    ui_loading_show("Loading...");

    lv_disp_t *disp = lv_disp_get_default();
    lv_indev_t *indev = lv_indev_get_next(NULL);

    LVGL_RUN_FLAGE = 0;
    if (indev)
        lv_indev_set_group(indev, NULL);
    lv_timer_enable(false);
    lv_refr_now(disp);

    int ret = cp0_process_exec_blocking(exec.c_str(), &LVGL_HOME_KEY_FLAG,
                                        keep_root ? 1 : 0);

    lv_timer_enable(true);
    if (indev)
        lv_indev_set_group(indev, UILaunchPage::home_input_group());
    if (launch_page_)
        launch_page_->show_home_screen();
    ui_loading::hide();
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(disp);
    LVGL_RUN_FLAGE = 1;
}
```

Key points:

- Shows Loading and forces a refresh before launch, so the user gets immediate feedback.
- Clears the APPLaunch input group so the home screen does not keep processing keys while the external process is running.
- `lv_timer_enable(false)` pauses LVGL timers while the external program takes the foreground.
- `cp0_process_exec_blocking()` blocks until the external program exits.
- After the external program exits, it restores the timer, calls `launch_page_->show_home_screen()`, and restores `LVGL_RUN_FLAGE`.

Sequence text:

```text
Enter external app
  -> ui_loading_show()
  -> LVGL_RUN_FLAGE=0
  -> lv_indev_set_group(NULL)
  -> lv_timer_enable(false)
  -> lv_refr_now()
  -> cp0_process_exec_blocking()
      -> External program runs
      -> APPLaunch main rendering is paused
      -> Wait for the external program to exit
  -> lv_timer_enable(true)
  -> launch_page_->show_home_screen()
  -> ui_loading_hide()
  -> lv_refr_now()
  -> LVGL_RUN_FLAGE=1
```

`STORE` is a fixed entry that launches an external application:

```cpp
{{"STORE", "store_100.png", "app_Store", false, true},
 "/usr/share/APPLaunch/bin/M5CardputerZero-AppStore", false, true, true, nullptr},
```

Here `run_as_root=true` is passed to `launch_Exec(exec, run_as_root)`, and then converted to `keep_root ? 1 : 0`.

## 9. Return-to-Home Mechanism

Built-in pages and terminal pages return to the home screen through the `navigate_home` callback:

```cpp
void go_back_home()
{
    lv_async_call(lv_go_back_home, this);
}

static void lv_go_back_home(void *arg)
{
    auto self = (Launch *)arg;
    lv_timer_enable(true);
    if (self->launch_page_)
        self->launch_page_->show_home_screen();
    lv_refr_now(NULL);
    if (self->app_Page)
        self->app_Page.reset();
}
```

Why `lv_async_call()` is used:

- Returning home may be triggered by a page event or input callback.
- Running asynchronously avoids destroying the page object directly inside the current LVGL event stack.
- `app_Page.reset()` releases the current page, so the code must ensure that page object is no longer used.

External applications do not use `navigate_home`; instead, the home screen is restored after `cp0_process_exec_blocking()` returns.

## 10. `.desktop` Dynamic Application Scanning

Dynamic application directory:

```cpp
const std::string app_dir_path = cp0_file_path("applications");
```

After installation on a device, this usually maps to:

```text
/usr/share/APPLaunch/applications/
```

Example `.desktop` file:

```ini
[Desktop Entry]
Name=Vim
TryExec=vim
Exec=vim
Terminal=true
Icon=share/images/e-Mail_80.png
```

`applications_load()` only processes files with the `.desktop` extension, and reads fields from the `[Desktop Entry]` section:

| Field | Required | Description |
| --- | --- | --- |
| `Name` | Yes | Home-screen display title |
| `Exec` | Yes | Launch command |
| `Icon` | No | Icon path |
| `Terminal` | No | `true/True/1` means launch through `UISTPage` |
| `Sysplause` | No | Pause policy passed to the terminal page; defaults to true |

Registration logic:

```cpp
if (page_title.empty() || app_exec.empty())
    continue;

for (auto it : app_list) {
    if (it.Exec == app_exec) {
        in_list = true;
        break;
    }
}

if (!in_list)
    app_list.emplace_back(page_title, app_icon, app_exec,
                          app_terminal, app_sysplause);
```

Note: dynamic applications are deduplicated by `Exec`; if `Exec` matches a fixed app or another `.desktop` app, it is skipped.

## 11. Dynamic Application Directory Watching and Reloading

At the end of the `Launch` constructor:

```cpp
fixed_count = app_list.size();
applications_load();
inotify_init_watch();
watch_timer = lv_timer_create(app_dir_watch_cb, 3000, this);
```

Watch flow:

```text
Every 3 seconds via LVGL timer
  -> cp0_dir_watch_poll(dir_watcher)
  -> If applications/ changed
      -> applications_reload()
          -> Delete dynamic apps after fixed_count
          -> applications_load()
          -> refresh_ui_panels()
```

`refresh_ui_panels()` rewrites the 5 visible/hidden slots according to the current `current_app`:

```cpp
app_at(current_app - 2) -> far-left
app_at(current_app - 1) -> left
app_at(current_app)     -> center
app_at(current_app + 1) -> right
app_at(current_app + 2) -> far-right
```

This ensures that after dynamic applications are added or removed, the home screen does not need to rebuild LVGL objects; it only updates text and icons.

## 12. Icon Setting and Resource Paths

Icons are written by `panel_set_icon()`:

```cpp
static void panel_set_icon(lv_obj_t *panel, const char *src)
{
    lv_obj_t *img = lv_obj_get_child(panel, 0);
    if (!img || !lv_obj_check_type(img, &lv_image_class)) {
        img = lv_image_create(panel);
        lv_obj_set_size(img, LV_PCT(100), LV_PCT(100));
        lv_obj_set_align(img, LV_ALIGN_CENTER);
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_STRETCH);
    }
    lv_image_set_src(img, icon_src);
}
```

Characteristics:

- Each panel reuses the first child image and does not repeatedly create image objects.
- The image is stretched to the panel size.
- If the path is empty or unreadable, it writes a log but still calls `lv_image_set_src()`.

Fixed applications generally use `img_path("xxx.png")`. The `Icon` field of dynamic `.desktop` applications is currently passed directly as `app_icon`. When writing `.desktop` files, make sure the icon path can be read by LVGL.

## 13. Complete Flow from Key Press to Launch

```text
User releases ENTER
  -> LV_EVENT_KEYBOARD is delivered to UILaunchPage::screen()
  -> UILaunchPage::on_home_key()
      -> handle_home_key()
      -> code == KEY_ENTER and key_state == 0
      -> audio_play_enter()
  -> UILaunchPage::launch_selected_app()
      -> launch_->launch_app()
  -> Launch::launch_app()
      -> impl_->launch_app()
  -> Launch::launch_app()
      -> auto it = std::next(app_list.begin(), current_app)
      -> it->launch(this)
  -> Enter built-in page / terminal page / external process based on app type
```

## 14. Notes

- `Launch::bind_ui()` must be called before `Launch` is created. Otherwise, the home screen may be displayed, but application list updates, the status-bar timer, directory watching, and launch logic will not work.
- `current_app` defaults to `2`. The order of the first 5 fixed entries affects the initial center card; consider the initial home experience when changing this order.
- If built-in page construction can take a long time, keep `ui_loading_show()` + `lv_refr_now()` so the user sees immediate feedback.
- Launching an external application pauses APPLaunch LVGL timers and input group. The external program must exit normally or respond to the HOME logic, otherwise the user will feel stuck in the external UI.
- A dynamic `.desktop` application needs at least `Name` and `Exec`; `Terminal=true` is suitable for command-line programs, while graphical or exclusive-framebuffer programs should use `Terminal=false`.
- Dynamic applications are deduplicated by `Exec`, not by `Name`; if multiple entries use the same command, only the first one is kept.
- After modifying `applications/`, wait up to 3 seconds for the watcher to reload it. If the watcher is not initialized or the platform does not support it, restart APPLaunch to verify the change.
