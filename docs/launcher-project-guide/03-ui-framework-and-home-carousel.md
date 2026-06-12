# 03 - UI Framework and Home Carousel

This chapter explains how the APPLaunch home UI is organized, how data flows through the carousel cards, and how key events are handled. Key references are `projects/APPLaunch/main/ui/UILaunchPage.cpp`, `projects/APPLaunch/main/ui/UILaunchPage.h`, `projects/APPLaunch/main/ui/Animation/ui_launcher_animation.cpp`, and `projects/APPLaunch/main/ui/Launch.cpp`.

## 1. UI Framework Overview

APPLaunch does not use a traditional desktop framework. Instead, it builds the UI directly from an LVGL object tree:

```text
ui_Screen1
├── Top status bar create_top()
│   ├── ZERO / logo
│   ├── WiFi signal bars
│   ├── Time panel
│   └── Battery panel
└── ui_APP_Container create_app_container()
    ├── 5 carousel card panels
    ├── 5 title labels
    ├── Left/right arrow buttons
    └── 5 page dots
```

The global entry points for home objects come from declarations in `ui_obj.h`, such as `ui_Screen1`, `ui_APP_Container`, `ui_timeLabel`, and `ui_Bar1`. Actual creation and styling are concentrated in `UILaunchPage.cpp`.

## 2. Key Source Paths

| Path | Description |
| --- | --- |
| `projects/APPLaunch/main/ui/UILaunchPage.h` | Home class definition, carousel element enum, and `carousel_elements` array |
| `projects/APPLaunch/main/ui/UILaunchPage.cpp` | Home screen creation, carousel switching, keyboard events, startup GIF, and font cache |
| `projects/APPLaunch/main/ui/Animation/ui_launcher_animation.cpp` | Carousel left/right switch animation |
| `projects/APPLaunch/main/ui/Launch.cpp` | Fills new card content after switching, launches the current application, and refreshes the status bar |
| `projects/APPLaunch/main/ui/ui.h` | Home layout constants such as `LABEL_Y_CENTER` and `BORDER_COLOR_CENTER` |
| `projects/APPLaunch/main/ui/ui_obj.h` | Global LVGL object declarations |

## 3. Responsibilities of `UILaunchPage`

`UILaunchPage` is the facade class for the home UI:

```cpp
class UILaunchPage : public home_base
{
public:
    static void load_home_screen();
    static void start_startup_gif();
    static void create_screen();

    static void init_input_group();
    static void bind_home_input_group();
    static lv_group_t *home_input_group();
    static lv_obj_t *panel(size_t slot);
    static lv_obj_t *label(size_t slot);

    void update_left_slot(lv_obj_t *panel, lv_obj_t *label);
    void update_right_slot(lv_obj_t *panel, lv_obj_t *label);
    void launch_selected_app();

    static std::array<lv_obj_t *, kLauncherCarouselElementCount> carousel_elements;
};
```

It has two categories of responsibilities:

- Static responsibilities: create the screen, maintain the home input group, and provide `panel()` / `label()` accessors.
- Instance responsibilities: hold the `Launch` pointer and forward carousel updates and application launch operations to `LaunchImpl`.

The current code stores the active home page instance in `active_launch_page` so static event callbacks can call it:

```cpp
namespace {
UILaunchPage *active_launch_page = nullptr;
}

UILaunchPage::UILaunchPage(std::shared_ptr<Launch> launch)
    : home_base(), launch_(std::move(launch))
{
    active_launch_page = this;
}
```

## 4. Carousel Element Array

All core objects of the home carousel are stored in a fixed array:

```cpp
std::array<lv_obj_t *, UILaunchPage::kLauncherCarouselElementCount>
    UILaunchPage::carousel_elements = {};
```

The enum is defined in `UILaunchPage.h`:

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

`UILaunchPage.cpp` uses `CarouselSlot` to describe the static carousel layout:

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

`create_screen()` creates the root screen:

```cpp
ui_Screen1 = lv_obj_create(NULL);
lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), LV_PART_MAIN);

create_top(ui_Screen1);
create_app_container(ui_Screen1);
```

### 6.1 Top Status Bar

`create_top()` contains:

- The top-left `ZERO` text or `launcher_brand_logo.png`.
- `ui_wifiPanel` and `ui_wifiBar1..4`, hidden by default and shown by signal strength during status refresh.
- `ui_Panel1`, the time background image `status_time_background.png`, and `ui_timeLabel`.
- `ui_batteryPanel`, the battery background image `status_battery_background.png`, `ui_Bar1`, and `ui_powerLabel`.

Status bar data is not refreshed in `UILaunchPage`, but in `LaunchImpl::update_home_status_bar()`:

```cpp
cp0_wifi_status_t wifi = cp0_wifi_get_status();
cp0_time_str(time_buf, sizeof(time_buf));
cp0_battery_info_t bat = cp0_battery_read();
```

`LaunchImpl` creates a 5-second timer during construction:

```cpp
status_timer = lv_timer_create(home_status_timer_cb, 5000, this);
```

### 6.2 Carousel Container

`create_app_container()` creates `ui_APP_Container`:

```cpp
ui_APP_Container = lv_obj_create(parent);
lv_obj_remove_style_all(ui_APP_Container);
lv_obj_set_width(ui_APP_Container, 320);
lv_obj_set_height(ui_APP_Container, 150);
lv_obj_set_align(ui_APP_Container, LV_ALIGN_CENTER);
```

It then creates, in order:

- 5 page dots: `kPageDot0..kPageDot4`; the center page dot defaults to 10x10 and yellow.
- 5 titles: the center defaults to `CLI`, left/right default to `STORE` / `GAME`, and far-side titles are hidden.
- 5 cards: the center is 100x100, left/right are 80x80, and far-side cards are 61x61 and hidden.
- Left/right buttons: background images `carousel_left_arrow.png` / `carousel_right_arrow.png`.

The default titles are only UI placeholders. Real content is written by `LaunchImpl` after it initializes the application list.

## 7. Carousel Switch Flow

Carousel switching is split into two parts: UI animation and application data update.

### 7.1 Switching Right with `switch_right()`

`switch_right()` means the cards move right as a group, and the current selection becomes the previous application in the list:

```cpp
void switch_right(lv_event_t *e)
{
    if (is_animating) {
        pending_switch = &switch_right;
        return;
    }

    is_animating = true;
    lv_obj_clear_flag(carousel_elements[0], LV_OBJ_FLAG_HIDDEN);
    launcher_home_animation::animate_right(carousel_elements.data(), snap_all_panels);

    snap_panel_to_slot(carousel_elements[4], 0);
    snap_label_to_slot(carousel_elements[9], 5);

    active_launch_page->update_right_slot(carousel_elements[4], carousel_elements[9]);
    rotate_carousel_right(0, 4);
    rotate_carousel_right(5, 9);
}
```

Key steps:

1. If an animation is already running, store this request in `pending_switch` and execute it after the current animation finishes.
2. Show the far-left hidden card as the side entering the viewport during the animation.
3. Call `launcher_home_animation::animate_right()` to start the animation.
4. Pre-snap the far-right object to the far-left slot and fill it with the new application content that will enter.
5. Rotate `carousel_elements[0..4]` and `[5..9]` so the array order matches the new visual order.
6. Update page dot highlighting.

### 7.2 Switching Left with `switch_left()`

`switch_left()` means the cards move left as a group, and the current selection becomes the next application in the list:

```cpp
void switch_left(lv_event_t *e)
{
    if (is_animating) {
        pending_switch = &switch_left;
        return;
    }

    is_animating = true;
    lv_obj_clear_flag(carousel_elements[4], LV_OBJ_FLAG_HIDDEN);
    launcher_home_animation::animate_left(carousel_elements.data(), snap_all_panels);

    snap_panel_to_slot(carousel_elements[0], 4);
    snap_label_to_slot(carousel_elements[5], 9);

    active_launch_page->update_left_slot(carousel_elements[0], carousel_elements[5]);
    rotate_carousel_left(0, 4);
    rotate_carousel_left(5, 9);
}
```

It is symmetric with `switch_right()`: the far-right side enters the viewport, while the far-left object is moved to the far-right slot and filled with new content.

## 8. Snapping Back After Animation

The animation completion callback is `snap_all_panels()`:

```cpp
static void snap_all_panels()
{
    for (int i = 0; i < 5; i++)
        snap_panel_to_slot(carousel_elements[i], i);

    for (int i = 5; i < 10; i++)
        snap_label_to_slot(carousel_elements[i], i);

    is_animating = false;

    if (pending_switch) {
        switch_cb_t cb = pending_switch;
        pending_switch = NULL;
        cb(NULL);
    }
}
```

It solves two problems:

- Animation interpolation may introduce small errors, so objects are force-snapped to the standard slots after the animation ends.
- If the user repeatedly presses direction keys during the animation, only one pending switch is kept and executed after the animation completes.

## 9. How Application Data Is Written into the Carousel

`LaunchImpl` maintains `current_app` and `app_list`. During a switch, `UILaunchPage` only passes in the panel/label to be reused; `LaunchImpl` calculates which application should be displayed.

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

The home keyboard event is bound at the end of `create_app_container()`:

```cpp
lv_obj_add_event_cb(ui_Screen1, main_key_switch,
                    (lv_event_code_t)LV_EVENT_KEYBOARD, NULL);
```

`main_key_switch()` logic:

```text
Press LEFT/Z
  -> audio_play_switch()
  -> switch_right()

Press RIGHT/C
  -> audio_play_switch()
  -> switch_left()

Release ENTER
  -> audio_play_enter()
  -> app_launch()

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
UILaunchPage::create_screen()
  -> create_top()
      -> Create logo / WiFi / time / battery objects
  -> create_app_container()
      -> Create page dots
      -> Create labels
      -> Create cards
      -> Create arrows
      -> Bind click and keyboard callbacks

User presses RIGHT
  -> main_key_switch()
  -> audio_play_switch()
  -> switch_left()
      -> is_animating=true
      -> animate_left()
      -> update_left_slot()
      -> rotate cards / labels
      -> Update page dot
  -> snap_all_panels()
      -> Snap objects to standard slots
      -> is_animating=false
      -> If pending_switch exists, continue executing it

User presses ENTER
  -> main_key_switch()
  -> audio_play_enter()
  -> app_launch()
  -> UILaunchPage::launch_selected_app()
  -> Launch::launch_app()
```

## 12. Notes

- `carousel_elements` stores LVGL object pointers; carousel switching rotates the pointer array instead of destroying and recreating objects.
- The names `switch_left()` / `switch_right()` describe animation direction and are not necessarily identical to user key direction. Currently, `KEY_LEFT` calls `switch_right()`, and `KEY_RIGHT` calls `switch_left()`.
- During animation, only one `pending_switch` is recorded, so rapid repeated key presses do not create an unbounded queue.
- Home card click events are all bound to `app_launch()`, but normal interaction mainly uses center selection + Enter launch. If mouse/touch interaction is enabled, confirm whether clicking a non-center card matches expectations.
- Status bar objects are created by `UILaunchPage`, but the refresh timer is created during `LaunchImpl` construction. If the home screen is created without executing `Launch::bind_ui()`, the application list and status bar refresh will not start.
- When adding or adjusting carousel slots, update `CAROUSEL_SLOTS`, the initial positions in `create_app_container()`, and the slot definitions in the animation file together to avoid jumps after animation completion.
