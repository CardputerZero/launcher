# 03 - UI Framework and Home Carousel

この章では、APPLaunch のホーム UI がどのように構成され、データがカルーセルカードを通してどのように流れ、キーイベントがどのように処理されるかを説明します。主な参照先は `projects/APPLaunch/main/ui/ui_launch_page.cpp`、`projects/APPLaunch/main/ui/ui_launch_page.h`、`projects/APPLaunch/main/ui/animation/ui_launcher_animation.cpp`、`projects/APPLaunch/main/ui/launch.cpp` です。

## 1. UI Framework Overview

APPLaunch は従来型のデスクトップフレームワークを使いません。代わりに、共有 LVGL ページ基底とカルーセルコンテンツ領域からホームページを構築します。

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

ホームは、ルート画面、ステータスバー、入力グループに共通の `home_base` / `AppPageRoot` ページフレームワークを使用します。`ui_launch_page.cpp` は継承したコンテンツコンテナにカルーセルを配置し、LVGL コールバックを接続します。

## 2. Key Source Paths

| Path | Description |
| --- | --- |
| `projects/APPLaunch/main/ui/ui_launch_page.h` | ホームクラス定義、カルーセル要素 enum、`carousel_elements` 配列 |
| `projects/APPLaunch/main/ui/ui_launch_page.cpp` | ホーム画面作成、カルーセル切り替え、キーボードイベント、起動 GIF、フォントキャッシュ |
| `projects/APPLaunch/main/ui/animation/ui_launcher_animation.cpp` | カルーセルの左右切り替えアニメーション |
| `projects/APPLaunch/main/ui/launch.cpp` | 切り替え後の新しいカード内容の設定、現在のアプリケーション起動、ステータスバー更新 |
| `projects/APPLaunch/main/ui/ui.h` | `LABEL_Y_CENTER` や `BORDER_COLOR_CENTER` などのホームレイアウト定数 |

## 3. Responsibilities of `UILaunchPage`

`UILaunchPage` はホーム UI の facade クラスです。

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

責務は大きく 2 種類です。

- static 互換責務: 共有 `carousel_elements` 配列を保持し、ホーム入力グループのブリッジを維持し、`launch.cpp` が使用する `panel()` / `label()` アクセサを提供します。
- インスタンス責務: `Launch` ポインタを保持し、ページ単位の UI 状態を所有し、LVGL イベントを処理し、カルーセル更新とアプリ起動を `Launch` へ委譲します。

LVGL は引き続き C スタイルの static コールバックを必要としますが、現在のコードは通常のイベントディスパッチでグローバル状態に依存しません。各コールバックは LVGL user data を通して所有元ページインスタンスを受け取ります。

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

コールバックは `this` とともに登録されます。

```cpp
lv_obj_add_event_cb(left_arrow_button_, on_left_arrow_clicked, LV_EVENT_CLICKED, this);
lv_obj_add_event_cb(right_arrow_button_, on_right_arrow_clicked, LV_EVENT_CLICKED, this);
lv_obj_add_event_cb(screen(), on_home_key, (lv_event_code_t)LV_EVENT_KEYBOARD, this);
lv_obj_add_event_cb(startup_gif_, on_startup_gif_event, LV_EVENT_ALL, this);
```

`active_launch_page` は、`UILaunchPage::panel()`、`UILaunchPage::label()`、`UILaunchPage::home_input_group()` のような static 外部アクセサ向けの互換ブリッジとしてだけ保持されます。

## 4. Carousel Element Array

ホームカルーセルの中核オブジェクトはすべて固定配列に格納されます。

```cpp
std::array<lv_obj_t *, UILaunchPage::kLauncherCarouselElementCount>
    UILaunchPage::carousel_elements = {};
```

enum は `ui_launch_page.h` で定義されています。

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

配列は 3 つの区間に分かれます。

| Index Range | Object | Description |
| --- | --- | --- |
| `0..4` | Card panel | far-left、left、center、right、far-right |
| `5..9` | Title label | カードスロットに対応 |
| `10..14` | Page dot | 下部の 5 つの状態ドット |

ヘルパーアクセサ:

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

したがって、`panel(2)` は中央カード、`label(2)` は中央タイトルです。

## 5. Standard Slot Layout

`ui_launch_page.cpp` は `CarouselSlot` を使って静的なカルーセルレイアウトを表現します。

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

スロットの意味:

```text
Cards:  far-left(hidden)  left  center  right  far-right(hidden)
Titles: far-left(hidden)  left  center  right  far-right(hidden)
```

非表示の両端スロットはアニメーション用バッファです。切り替え前に、これから入ってくるカードを far side に置き、アニメーション終了後に配列順を回転します。

## 6. Home Creation Flow

`home_base` はルート画面、上部ステータスバー、コンテンツコンテナを構築します。`UILaunchPage::create_screen()` はホームコンテンツ領域を埋めるだけで、カルーセルがすでに存在する場合は再構築を避けます。

```cpp
void UILaunchPage::create_screen()
{
    if (carousel_elements[kCardCenter])
        return;

    create_app_container(content_container());
}
```

### 6.1 Top Status Bar

上部ステータスバーは `home_base::creat_Top_UI()` 由来で、次を含みます。

- 左上の `ZERO` テキストまたは `launcher_brand_logo.png`。
- `ui_wifiPanel` と `ui_wifiBar1..4`。デフォルトでは非表示で、ステータス更新時に信号強度に応じて表示されます。
- `ui_Panel1`、時刻背景画像 `status_time_background.png`、`ui_timeLabel`。
- `ui_batteryPanel`、バッテリー背景画像 `status_battery_background.png`、`ui_Bar1`、`ui_powerLabel`。

ステータスバーのデータ更新は `UILaunchPage` ではなく `Launch::update_home_status_bar()` で行われます。

```cpp
cp0_wifi_status_t wifi = cp0_wifi_get_status();
cp0_time_str(time_buf, sizeof(time_buf));
cp0_battery_info_t bat = cp0_battery_read();
```

`Launch` は構築時に 5 秒タイマーを作成します。

```cpp
status_timer = lv_timer_create(home_status_timer_cb, 5000, this);
```

### 6.2 Carousel Container

`create_app_container()` は継承した `content_container()` をカルーセルコンテナとして使用します。

```cpp
lv_obj_t *app_container = parent;
if (!app_container)
    return;

lv_obj_set_size(app_container, 320, 150);
lv_obj_clear_flag(app_container,
                  (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
```

その後、次の順で作成します。

- 5 つのページドット: `kPageDot0..kPageDot4`。中央ページドットはデフォルトで 10x10、黄色です。
- 5 つのタイトル: 中央はデフォルトで `CLI`、左右は `STORE` / `GAME`、両端のタイトルは非表示です。
- 5 枚のカード: 中央は 100x100、左右は 80x80、両端カードは 61x61 で非表示です。
- 左右ボタン: 背景画像 `carousel_left_arrow.png` / `carousel_right_arrow.png`。

デフォルトタイトルは UI プレースホルダにすぎません。実際の内容は、アプリケーションリスト初期化後に `Launch` が書き込みます。

## 7. Carousel Switch Flow

カルーセル切り替えは、UI アニメーションとアプリケーションデータ更新の 2 つに分かれます。

### 7.1 Switching Right with `switch_right()`

`UILaunchPage::switch_right()` はカード群が右へ動き、現在選択がリスト内の前のアプリケーションになることを意味します。

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

主要ステップ:

1. すでにアニメーション中なら `PendingSwitch::Right` を保存します。保持されるのは最新の保留方向だけです。
2. アニメーション中に viewport へ入る側として、非表示だった far-left カードを表示します。
3. `launcher_home_animation::animate_right()` を呼び、`this` をキャプチャした lambda を渡します。
4. far-right オブジェクトを far-left スロットへ事前に snap し、これから入ってくる新しいアプリ内容で埋めます。
5. `carousel_elements[0..4]` と `[5..9]` を回転し、配列順を新しい見た目の順序に合わせます。
6. ページドットのハイライトを更新します。

### 7.2 Switching Left with `switch_left()`

`UILaunchPage::switch_left()` はカード群が左へ動き、現在選択がリスト内の次のアプリケーションになることを意味します。

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

これは `switch_right()` と対称です。far-right 側が viewport に入り、far-left オブジェクトは far-right スロットへ移動して新しい内容で埋められます。

## 8. Snapping Back After Animation

アニメーション完了経路は `UILaunchPage::finish_switch_animation()` です。

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

`run_pending_switch()` は enum 状態を消費し、対応するインスタンスメソッドを呼びます。

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

これにより 2 つの問題を解決します。

- アニメーション補間で小さな誤差が出る可能性があるため、終了後にオブジェクトを標準スロットへ強制 snap します。
- ユーザーがアニメーション中に方向キーを連打しても、保留される switch enum は 1 つだけで、アニメーション完了後に実行されます。

## 9. How Application Data Is Written into the Carousel

`Launch` は `current_app` と `app_list` を管理します。切り替え中、`UILaunchPage` は再利用する panel/label を渡すだけで、どのアプリケーションを表示すべきかは `Launch` が計算します。

左へ切り替えた後、新しい右端を埋める処理:

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

右へ切り替えた後、新しい左端を埋める処理:

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

図:

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

ホームのキーボードイベントは、`create_app_container()` の末尾で LVGL コールバックブリッジを通してバインドされます。

```cpp
lv_obj_add_event_cb(screen(), on_home_key,
                    (lv_event_code_t)LV_EVENT_KEYBOARD, this);
```

`on_home_key()` は所有元の `UILaunchPage` インスタンス上で `handle_home_key()` を呼びます。ロジックは次のとおりです。

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

コードはまず `fzxc_to_arrow()` によって `F/X/Z/C` を矢印キーへマップします。

```cpp
KEY_F -> KEY_UP
KEY_X -> KEY_DOWN
KEY_Z -> KEY_LEFT
KEY_C -> KEY_RIGHT
```

効果音のエントリポイント:

```cpp
cp0_signal_system_play_asset("switch.wav");
cp0_signal_system_play_asset("enter.wav");
```

起動音は `load_home_screen()` で再生されます。

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

- `carousel_elements` は LVGL オブジェクトポインタを保存します。カルーセル切り替えでは、オブジェクトを破棄して再作成するのではなく、ポインタ配列を回転します。
- `switch_left()` / `switch_right()` という名前はアニメーション方向を表しており、必ずしもユーザーのキー方向と同一ではありません。現在は `KEY_LEFT` が `switch_right()` を呼び、`KEY_RIGHT` が `switch_left()` を呼びます。
- アニメーション中は `pending_switch_` の enum 値を 1 つだけ記録するため、連打しても無制限のキューは作られません。
- ホームカードのクリックイベントは `on_app_clicked()` にバインドされ、`launch_selected_app()` へブリッジします。ただし通常操作では中央選択 + Enter 起動が主です。マウス/タッチ操作を有効にする場合、中央以外のカードクリックが期待どおりか確認してください。
- ステータスバーオブジェクトは `UILaunchPage` が作成しますが、更新タイマーは `Launch` 構築時に作られます。`Launch::bind_ui()` を実行せずにホーム画面を作成すると、アプリケーションリストとステータスバー更新は開始されません。
- カルーセルスロットを追加または調整する場合、`CAROUSEL_SLOTS`、`create_app_container()` 内の初期位置、アニメーションファイル内のスロット定義を同時に更新し、アニメーション完了後のジャンプを避けてください。
