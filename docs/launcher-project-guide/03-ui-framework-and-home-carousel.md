# 03 - UI Framework and Home Carousel

This chapter explains how the APPLaunch home UI is organized, how data flows through the carousel cards, and how key events are handled. Key references are `projects/APPLaunch/main/ui/ui_launch_page.cpp`, `projects/APPLaunch/main/ui/ui_launch_page.h`, `projects/APPLaunch/main/ui/animation/ui_launcher_animation.cpp`, and `projects/APPLaunch/main/ui/launch.cpp`.

## 1. UI Framework Overview

APPLaunch does not use a traditional desktop framework. Instead, it builds the home page from the shared LVGL page base plus a carousel content area:

```text
UILaunchPage : home_base
├── home_base/AppPageRoot root screen
│   ├── home_base::creat_Top_UI()
│   │   ├── ZERO / logo
│   │   ├── WiFi signal bars
│   │   ├── Time panel
│   │   └── Battery panel
│   └── content_container()
└── Home carousel inside content_container()
    ├── 5 carousel card panels
    ├── 5 title labels
    ├── Left/right arrow buttons
    └── 5 page dots
```

Home uses the common `home_base` / `AppPageRoot` page framework for the root screen, status bar, and input group. `ui_launch_page.cpp` fills the inherited content container with the carousel and wires the LVGL callbacks.

## 2. Key Source Paths

| Path | Description |
| --- | --- |
| `projects/APPLaunch/main/ui/ui_launch_page.h` | Home class definition, carousel element enum, and `carousel_elements` array |
| `projects/APPLaunch/main/ui/ui_launch_page.cpp` | Home screen creation, carousel switching, keyboard events, startup GIF, and font cache |
| `projects/APPLaunch/main/ui/animation/ui_launcher_animation.cpp` | Carousel left/right switch animation |
| `projects/APPLaunch/main/ui/launch.cpp` | Fills new card content after switching, launches the current application, and refreshes the status bar |
| `projects/APPLaunch/main/ui/ui.h` | Home layout constants such as `LABEL_Y_CENTER` and `BORDER_COLOR_CENTER` |

## 3. Responsibilities of `UILaunchPage`

`UILaunchPage` is the facade class for the home UI:

```cpp
class UILaunchPage : public home_base
{
public:
    explicit UILaunchPage(std::shared_ptr<Launch> launch);
    ~UILaunchPage();

    void show_home_screen();
    void load_home_screen();
    void start_startup_gif();
    void create_screen();
    void init_input_group();

    static void bind_home_input_group();
    static lv_group_t *home_input_group();
    static lv_obj_t *panel(size_t slot);
    static lv_obj_t *label(size_t slot);

    void update_left_slot(lv_obj_t *panel, lv_obj_t *label);
    void update_right_slot(lv_obj_t *panel, lv_obj_t *label);
    void launch_selected_app();

private:
    enum class PendingSwitch { None, Left, Right };

    void switch_left();
    void switch_right();
    void finish_switch_animation();
    void run_pending_switch();
    void handle_home_key(lv_event_t *event);
    void handle_startup_gif_event(lv_event_t *event);

    static void on_left_arrow_clicked(lv_event_t *event);
    static void on_right_arrow_clicked(lv_event_t *event);
    static void on_app_clicked(lv_event_t *event);
    static void on_home_key(lv_event_t *event);
    static void on_startup_gif_event(lv_event_t *event);

    bool is_animating_ = false;
    PendingSwitch pending_switch_ = PendingSwitch::None;
    int switch_current_pos_ = kPageDot2;
};
```

It has two categories of responsibilities:

- Static compatibility responsibilities: keep the shared `carousel_elements` array, maintain the home input group bridge, and provide `panel()` / `label()` accessors used by `launch.cpp`.
- Instance responsibilities: hold the `Launch` pointer, own per-page UI state, handle LVGL events, and forward carousel updates / app launches to `Launch`.

LVGL still requires C-style static callbacks, but the current code no longer relies on global state for normal event dispatch. Each callback receives the owning page instance through LVGL user data:

```cpp
static UILaunchPage *page_from_event(lv_event_t *event)
{
    return event ? static_cast<UILaunchPage *>(lv_event_get_user_data(event)) : nullptr;
}

void UILaunchPage::on_left_arrow_clicked(lv_event_t *event)
{
    if (UILaunchPage *self = page_from_event(event))
        self->switch_right();
}
```

Callbacks are registered with `this`:

```cpp
lv_obj_add_event_cb(left_arrow_button_, on_left_arrow_clicked, LV_EVENT_CLICKED, this);
lv_obj_add_event_cb(right_arrow_button_, on_right_arrow_clicked, LV_EVENT_CLICKED, this);
lv_obj_add_event_cb(screen(), on_home_key, (lv_event_code_t)LV_EVENT_KEYBOARD, this);
lv_obj_add_event_cb(startup_gif_, on_startup_gif_event, LV_EVENT_ALL, this);
```

`active_launch_page` is kept only as a compatibility bridge for static external accessors such as `UILaunchPage::panel()`, `UILaunchPage::label()`, and `UILaunchPage::home_input_group()`.

## 4. Carousel Element Array

All core objects of the home carousel are stored in a fixed array:

```cpp
std::array<lv_obj_t *, UILaunchPage::kLauncherCarouselElementCount>
    UILaunchPage::carousel_elements = {};
```

The enum is defined in `ui_launch_page.h`:

```cpp
enum LauncherCarouselElement : size_t {
    kCardFarLeft = 0,
    kCardLeft,
    kCardCenter,
    kCardRight,
    kCardFarRight,
    kTitleFarLeft,
    kTitleLeft,
    kTitleCenter,
    kTitleRight,
    kTitleFarRight,
    kPageDot0,
    kPageDot1,
    kPageDot2,
    kPageDot3,
    kPageDot4,
    kLauncherCarouselElementCount,
};
```

The array is divided into three segments:

| Index Range | Object | Description |
| --- | --- | --- |
| `0..4` | Card panel | far-left, left, center, right, far-right |
| `5..9` | Title label | Corresponds to the card slots |
| `10..14` | Page dot | 5 bottom status dots |

Helper accessors:

```cpp
lv_obj_t *UILaunchPage::panel(size_t slot)
{
    return carousel_elements[kCardFarLeft + slot];
}

lv_obj_t *UILaunchPage::label(size_t slot)
{
    return carousel_elements[kTitleFarLeft + slot];
}
```

Therefore, `panel(2)` is the center card, and `label(2)` is the center title.

## 5. Standard Slot Layout

`ui_launch_page.cpp` uses `CarouselSlot` to describe the static carousel layout:

```cpp
struct CarouselSlot {
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t width;
    lv_coord_t height;
    bool hidden;
};

static const CarouselSlot CAROUSEL_SLOTS[] = {
    {-177, 4, 61, 61, true},
    {-99, -6, 80, 80, false},
    {0, -16, 100, 100, false},
    {99, -6, 80, 80, false},
    {177, 4, 61, 61, true},
    {-177, LABEL_Y_SIDE, 0, 0, true},
    {-99, LABEL_Y_SIDE, 0, 0, false},
    {0, LABEL_Y_CENTER, 0, 0, false},
    {99, LABEL_Y_SIDE, 0, 0, false},
    {177, LABEL_Y_SIDE, 0, 0, true},
};
```

Slot semantics:

```text
Cards:  far-left(hidden)  left  center  right  far-right(hidden)
Titles: far-left(hidden)  left  center  right  far-right(hidden)
```

The hidden far-side slots are animation buffers: before switching, the card that is about to enter is placed on the far side; after the animation ends, the array order is rotated.

## 6. Home Creation Flow

`home_base` constructs the root screen, top status bar, and content container. `UILaunchPage::create_screen()` only fills the home content area, and it avoids rebuilding the carousel if it already exists:

```cpp
void UILaunchPage::create_screen()
{
    if (carousel_elements[kCardCenter])
        return;

    create_app_container(content_container());
}
```

### 6.1 Top Status Bar

The top status bar comes from `home_base::creat_Top_UI()` and contains:

- The top-left `ZERO` text or `launcher_brand_logo.png`.
- `ui_wifiPanel` and `ui_wifiBar1..4`, hidden by default and shown by signal strength during status refresh.
- `ui_Panel1`, the time background image `status_time_background.png`, and `ui_timeLabel`.
- `ui_batteryPanel`, the battery background image `status_battery_background.png`, `ui_Bar1`, and `ui_powerLabel`.

Status bar data is not refreshed in `UILaunchPage`, but in `Launch::update_home_status_bar()`:

```cpp
cp0_wifi_status_t wifi = cp0_wifi_get_status();
cp0_time_str(time_buf, sizeof(time_buf));
cp0_battery_info_t bat = cp0_battery_read();
```

`Launch` creates a 5-second timer during construction:

```cpp
status_timer = lv_timer_create(home_status_timer_cb, 5000, this);
```

### 6.2 Carousel Container

`create_app_container()` uses the inherited `content_container()` as the carousel container:

```cpp
lv_obj_t *app_container = parent;
if (!app_container)
    return;

lv_obj_set_size(app_container, 320, 150);
lv_obj_clear_flag(app_container,
                  (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
```

It then creates, in order:

- 5 page dots: `kPageDot0..kPageDot4`; the center page dot defaults to 10x10 and yellow.
- 5 titles: the center defaults to `CLI`, left/right default to `STORE` / `GAME`, and far-side titles are hidden.
- 5 cards: the center is 100x100, left/right are 80x80, and far-side cards are 61x61 and hidden.
- Left/right buttons: background images `carousel_left_arrow.png` / `carousel_right_arrow.png`.

The default titles are only UI placeholders. Real content is written by `Launch` after it initializes the application list.

## 7. Carousel Switch Flow

Carousel switching is split into two parts: UI animation and application data update.

### 7.1 Switching Right with `switch_right()`

`UILaunchPage::switch_right()` means the cards move right as a group, and the current selection becomes the previous application in the list:

```cpp
void UILaunchPage::switch_right()
{
    if (is_animating_) {
        pending_switch_ = PendingSwitch::Right;
        return;
    }

    is_animating_ = true;
    lv_obj_clear_flag(carousel_elements[0], LV_OBJ_FLAG_HIDDEN);
    launcher_home_animation::animate_right(
        carousel_elements.data(),
        [this]() { finish_switch_animation(); });

    snap_panel_to_slot(carousel_elements[4], 0);
    snap_label_to_slot(carousel_elements[9], 5);

    update_right_slot(carousel_elements[4], carousel_elements[9]);
    rotate_carousel_right(0, 4);
    rotate_carousel_right(5, 9);
}
```

Key steps:

1. If an animation is already running, store `PendingSwitch::Right`; only the latest pending direction is kept.
2. Show the far-left hidden card as the side entering the viewport during the animation.
3. Call `launcher_home_animation::animate_right()` and pass a lambda that captures `this`.
4. Pre-snap the far-right object to the far-left slot and fill it with the new application content that will enter.
5. Rotate `carousel_elements[0..4]` and `[5..9]` so the array order matches the new visual order.
6. Update page dot highlighting.

### 7.2 Switching Left with `switch_left()`

`UILaunchPage::switch_left()` means the cards move left as a group, and the current selection becomes the next application in the list:

```cpp
void UILaunchPage::switch_left()
{
    if (is_animating_) {
        pending_switch_ = PendingSwitch::Left;
        return;
    }

    is_animating_ = true;
    lv_obj_clear_flag(carousel_elements[4], LV_OBJ_FLAG_HIDDEN);
    launcher_home_animation::animate_left(
        carousel_elements.data(),
        [this]() { finish_switch_animation(); });

    snap_panel_to_slot(carousel_elements[0], 4);
    snap_label_to_slot(carousel_elements[5], 9);

    update_left_slot(carousel_elements[0], carousel_elements[5]);
    rotate_carousel_left(0, 4);
    rotate_carousel_left(5, 9);
}
```

It is symmetric with `switch_right()`: the far-right side enters the viewport, while the far-left object is moved to the far-right slot and filled with new content.

## 8. Snapping Back After Animation

The animation completion path is `UILaunchPage::finish_switch_animation()`:

```cpp
void UILaunchPage::finish_switch_animation()
{
    for (int i = 0; i < 5; i++)
        snap_panel_to_slot(carousel_elements[i], i);

    for (int i = 5; i < 10; i++)
        snap_label_to_slot(carousel_elements[i], i);

    is_animating_ = false;
    run_pending_switch();
}
```

`run_pending_switch()` consumes the enum state and invokes the corresponding instance method:

```cpp
void UILaunchPage::run_pending_switch()
{
    PendingSwitch pending = pending_switch_;
    pending_switch_ = PendingSwitch::None;

    if (pending == PendingSwitch::Left)
        switch_left();
    else if (pending == PendingSwitch::Right)
        switch_right();
}
```

It solves two problems:

- Animation interpolation may introduce small errors, so objects are force-snapped to the standard slots after the animation ends.
- If the user repeatedly presses direction keys during the animation, only one pending switch enum is kept and executed after the animation completes.

## 9. How Application Data Is Written into the Carousel

`Launch` maintains `current_app` and `app_list`. During a switch, `UILaunchPage` only passes in the panel/label to be reused; `Launch` calculates which application should be displayed.

Fill the new right end after switching left:

```cpp
void update_left_slot(lv_obj_t *panel, lv_obj_t *label)
{
    current_app = current_app == app_list.size() - 1 ? 0 : current_app + 1;
    int next_app = current_app;
    next_app = next_app == app_list.size() - 1 ? 0 : next_app + 1;
    next_app = next_app == app_list.size() - 1 ? 0 : next_app + 1;
    auto it = std::next(app_list.begin(), next_app);
    lv_label_set_text(label, it->Name.c_str());
    panel_set_icon(panel, it->Icon.c_str());
}
```

Fill the new left end after switching right:

```cpp
void update_right_slot(lv_obj_t *panel, lv_obj_t *label)
{
    current_app = current_app == 0 ? app_list.size() - 1 : current_app - 1;
    int next_app = current_app;
    next_app = next_app == 0 ? app_list.size() - 1 : next_app - 1;
    next_app = next_app == 0 ? app_list.size() - 1 : next_app - 1;
    auto it = std::next(app_list.begin(), next_app);
    lv_label_set_text(label, it->Name.c_str());
    panel_set_icon(panel, it->Icon.c_str());
}
```

Diagram:

```text
Visual slots:       [far-left] [left] [center] [right] [far-right]
Application index:  current-2  current-1 current current+1 current+2

Press RIGHT:
  current -> current-1
  New far-left needs to display current-2

Press LEFT:
  current -> current+1
  New far-right needs to display current+2
```

## 10. Input Events and Sound Effects

The home keyboard event is bound at the end of `create_app_container()` through the LVGL callback bridge:

```cpp
lv_obj_add_event_cb(screen(), on_home_key,
                    (lv_event_code_t)LV_EVENT_KEYBOARD, this);
```

`on_home_key()` calls `handle_home_key()` on the owning `UILaunchPage` instance. Its logic is:

```text
Press LEFT/Z
  -> audio_play_switch()
  -> switch_right()

Press RIGHT/C
  -> audio_play_switch()
  -> switch_left()

Release ENTER
  -> audio_play_enter()
  -> launch_selected_app()

Release F12
  -> Toggle green test background lvping_lock
```

The code first maps `F/X/Z/C` to arrow keys through `fzxc_to_arrow()`:

```cpp
KEY_F -> KEY_UP
KEY_X -> KEY_DOWN
KEY_Z -> KEY_LEFT
KEY_C -> KEY_RIGHT
```

Sound effect entry points:

```cpp
cp0_signal_system_play_asset("switch.wav");
cp0_signal_system_play_asset("enter.wav");
```

The startup sound is played in `load_home_screen()`:

```cpp
cp0_signal_audio_api_play_asset("startup.mp3");
```

## 11. Home Sequence Text

```text
UILaunchPage constructed as home_base
  -> home_base::creat_Top_UI()
      -> Create logo / WiFi / time / battery objects
UILaunchPage::create_screen()
  -> create_app_container(content_container())
      -> Create page dots
      -> Create labels
      -> Create cards
      -> Create arrows
      -> Bind click and keyboard callbacks

User presses RIGHT
  -> on_home_key() -> handle_home_key()
  -> audio_play_switch()
  -> switch_left()
      -> is_animating=true
      -> animate_left()
      -> update_left_slot()
      -> rotate cards / labels
      -> Update page dot
  -> finish_switch_animation()
      -> Snap objects to standard slots
      -> is_animating=false
      -> If pending_switch_ exists, continue executing it

User presses ENTER
  -> on_home_key() -> handle_home_key()
  -> audio_play_enter()
  -> UILaunchPage::launch_selected_app()
  -> Launch::launch_app()
```

## 12. Notes

- `carousel_elements` stores LVGL object pointers; carousel switching rotates the pointer array instead of destroying and recreating objects.
- The names `switch_left()` / `switch_right()` describe animation direction and are not necessarily identical to user key direction. Currently, `KEY_LEFT` calls `switch_right()`, and `KEY_RIGHT` calls `switch_left()`.
- During animation, only one `pending_switch_` enum value is recorded, so rapid repeated key presses do not create an unbounded queue.
- Home card click events are bound to `on_app_clicked()`, which bridges to `launch_selected_app()`, but normal interaction mainly uses center selection + Enter launch. If mouse/touch interaction is enabled, confirm whether clicking a non-center card matches expectations.
- Status bar objects are created by `UILaunchPage`, but the refresh timer is created during `Launch` construction. If the home screen is created without executing `Launch::bind_ui()`, the application list and status bar refresh will not start.
- When adding or adjusting carousel slots, update `CAROUSEL_SLOTS`, the initial positions in `create_app_container()`, and the slot definitions in the animation file together to avoid jumps after animation completion.
