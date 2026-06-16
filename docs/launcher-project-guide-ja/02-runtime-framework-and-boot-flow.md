# 02 - Runtime Framework and Boot Flow

この章では、APPLaunch プロセスのエントリポイントからホーム画面の最初のフレームが表示されるまでの全体経路を説明します。主な参照先は `projects/APPLaunch/main/src/main.cpp`、`projects/APPLaunch/main/ui/ui.cpp`、`projects/APPLaunch/main/ui/launcher_ui_runtime.cpp`、`projects/APPLaunch/main/ui/ui_launch_page.cpp` です。

## 1. Runtime Framework Overview

APPLaunch は単一プロセスの LVGL アプリケーションです。メインスレッドがプラットフォーム初期化、UI オブジェクト作成、最初のフレーム更新を行い、その後 `lv_timer_handler()` によって駆動されるループに入ります。

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

主要な特徴:

- LVGL 初期化とプラットフォーム適応初期化は `main()` で一度だけ実行されます。
- ホーム UI は `LauncherUiRuntime` の制御下で作成され、実際のオブジェクトは `UILaunchPage::create_screen()` で作られます。
- `Launch` / `Launch` は、アプリケーションリスト、起動方式、ステータスバー更新、動的アプリケーションディレクトリの監視を担当します。
- `ui_init()` の直後に `lv_obj_invalidate()` + `lv_refr_now(NULL)` で最初のホームフレームを強制更新し、起動後の自然な更新を待つ間に黒画面が出ることを避けます。

## 2. Entry Files and Key Source Paths

| Path | Role |
| --- | --- |
| `projects/APPLaunch/main/src/main.cpp` | プロセスエントリポイント、LVGL メインループ、外部アプリケーション実行時ロック検出 |
| `projects/APPLaunch/main/ui/ui.cpp` | `ui_init()`、グローバルな `LauncherUiRuntime home` を作成する |
| `projects/APPLaunch/main/ui/launcher_ui_runtime.cpp` | LVGL テーマを設定し、ホーム画面を作成し、Launch 連携オブジェクトを作成する |
| `projects/APPLaunch/main/ui/ui_launch_page.cpp` | ホーム画面、起動 GIF、ホーム loading、入力グループ |
| `projects/APPLaunch/main/ui/launch.cpp` | アプリケーションマネージャ。外部/端末/組み込みページを起動し、ステータスバータイマーを所有する |
| `ext_components/cp0_lvgl` | `cp0_lvgl_init()`、ファイルパス、入力、プロセス、システム機能のラッパー |

## 3. `main()` Boot Flow

`main()` のフレームワークコードは次のとおりです。

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

1. `cp0_file_path("lock_file")` が実行時ロックファイルのパスを解決します。
2. `lv_init()` が LVGL コアオブジェクト、メモリ、タイマー、display/indev 抽象を初期化します。
3. `cp0_lvgl_init()` がプラットフォーム層を初期化します。display、入力、framebuffer/SDL、システムシグナル、その他の機能が対象です。
4. `lv_event_register_id()` がカスタムキーボードイベント `LV_EVENT_KEYBOARD` を登録します。
5. `ui_init()` が APPLaunch 独自の UI 構築フローに入ります。

### 3.2 First-Frame Refresh

`ui_init()` が戻った後、コードはすぐに次を実行します。

```cpp
lv_obj_invalidate(lv_scr_act());
lv_refr_now(NULL);
```

このステップの目的は通常の更新ではなく、現在のアクティブ画面の内容を framebuffer/SDL window へ強制的に flush することです。ホームオブジェクトが作成された直後に、後続の `lv_timer_handler()` だけに頼ると一瞬黒画面が見える場合があります。最初のフレームを強制することで、起動時の挙動をより決定的にします。

### 3.3 Main Loop

メインループは 5 ms 間隔で動作します。

```text
Each loop iteration
  -> APPLaunch_lock()
  -> lv_timer_handler()
  -> sleep 5ms
```

- `APPLaunch_lock()` は外部アプリケーションがフォアグラウンドを占有しているか確認します。
- `lv_timer_handler()` は LVGL タイマー、アニメーション、入力イベント、再描画を駆動します。
- `usleep(5000)` は CPU 使用率と更新間隔を制御します。

## 4. From `ui_init()` to Home Object Creation

`ui_init()` は `projects/APPLaunch/main/ui/ui.cpp` にあります。

```cpp
std::unique_ptr<LauncherUiRuntime> home;

void ui_init(void)
{
    home = std::make_unique<LauncherUiRuntime>();
}
```

`LauncherUiRuntime` コンストラクタは次の処理へ進みます。

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

ここでは順序に注意してください。

1. `create_display()` が最初にフォントマネージャを作成し、LVGL テーマを設定します。
2. `Launch` と `UILaunchPage` を構築し、`Launch::set_launch_page()` によって双方向の協調関係を確立します。
3. `build_launcher_home()` がホーム画面を作成し、`Launch::bind_ui()` を呼んでアプリケーションリストを構築し、入力グループを初期化し、ホーム画面または起動 GIF を表示します。

## 5. Display / Theme Initialization

`LauncherUiRuntime::create_display()` の中核コード:

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

注意点:

- `LauncherFonts` は、ホーム画面とページで共有される FreeType フォントキャッシュです。入口関数は `launcher_fonts()` です。
- `lv_disp_get_default()` は、`cp0_lvgl_init()` がすでに表示デバイスを登録していることに依存します。
- このテーマはベーステーマにすぎません。多くのホームコントロールは、サイズ、色、背景画像、フォントを `ui_launch_page.cpp` 内で手動設定します。

## 6. Home Creation and Display Flow

`LauncherUiRuntime::build_launcher_home()` はホーム画面を表示するための主要なエントリポイントです。

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

`UILaunchPage` は `home_base` を継承しているため、ルート画面、上部ステータスバー、コンテンツコンテナ、入力グループは共有ページフレームワークによって準備されます。`UILaunchPage::create_screen()` はホームコンテンツコンテナを埋めるだけで、一度だけ実行されます。

```cpp
void UILaunchPage::create_screen()
{
    if (carousel_elements[kCardCenter])
        return;

    create_app_container(content_container());
}
```

ここでホームカルーセル領域を作成します。5 枚のカード、5 つのタイトル、5 つのページドット、左右の矢印です。左上ロゴ、WiFi インジケータ、時刻ラベル、バッテリーバーは `home_base::creat_Top_UI()` が作成します。

### 6.2 Input Group Binding

ホーム入力グループは `AppPageRoot::input_group()` から得られます。`UILaunchPage::init_input_group()` はこれを互換ブリッジに保存し、アクティブなキーボード入力デバイスへバインドします。

```cpp
void UILaunchPage::init_input_group()
{
    ::home_input_group = input_group();
    bind_home_input_group();
}
```

これにより、キーボードイベントは `screen()` に配送され、LVGL コールバック `on_home_key()` が `handle_home_key()` へディスパッチして左右切り替えと Enter 起動を処理します。

### 6.3 Startup GIF and Home Display

`APPLAUNCH_STARTUP_ANIMATION` が有効で、プラットフォームが SDL ではない場合:

```text
Check cp0_file_path_c("logo_output.gif")
  -> file exists: UILaunchPage::start_startup_gif()
  -> file does not exist: UILaunchPage::load_home_screen()
```

`start_startup_gif()` は独立した GIF 画面を作成し、`this` とともにコールバックをバインドします。

```cpp
startup_gif_ = lv_gif_create(NULL);
lv_gif_set_src(startup_gif_, startup_gif_path_.data());
lv_obj_center(startup_gif_);
lv_obj_add_event_cb(startup_gif_, on_startup_gif_event, LV_EVENT_ALL, this);
lv_disp_load_scr(startup_gif_);
```

GIF 再生が終わると `LV_EVENT_READY` を受け取ります。`on_startup_gif_event()` は所有元の `UILaunchPage` インスタンスに戻り、`handle_startup_gif_event()` が GIF を一時停止してホーム画面を一度だけ読み込みます。

```cpp
if (event_code == LV_EVENT_READY && !startup_gif_done_) {
    startup_gif_done_ = true;
    if (startup_gif_) lv_gif_pause(startup_gif_);
    load_home_screen();
}
```

`load_home_screen()` の責務:

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

`APPLaunch_lock()` は、APPLaunch と外部の独立プロセスの間で、フォアグラウンド描画の関係を調整します。

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

実際のコードには複数の状態変数があります。

- `lvgl_lock`: 各ループで LVGL 更新復帰を繰り返すことを避けます。ロック解除後に一度だけ `invalidate` します。
- `home_back_status` / `start_time`: HOME キーが押され続けている時間を追跡します。
- `holder_pid`: 現在ロックファイルを保持している外部プロセスの PID。

ロジック:

```text
No external application holds the lock
  -> APPLaunch restores LVGL_RUN_FLAGE=1
  -> If just recovered from the locked state, redraw the current screen

An external application holds the lock
  -> APPLaunch sets LVGL_RUN_FLAGE=0 and pauses its own rendering
  -> If the HOME key has been held for >= 5 seconds, try to kill the external application
```

## 9. Notes

- `ui_init()` は内部ですでにホーム画面を作成し、読み込む場合があります。`main()` の後続の `lv_refr_now(NULL)` は最初のフレームの安全策であり、安易に削除しないでください。
- `cp0_lvgl_init()` は `ui_init()` より前に実行する必要があります。そうしないと `lv_disp_get_default()`、入力デバイス、パス、システムインターフェースが準備できていない可能性があります。
- SDL プラットフォームでは、起動 GIF はデフォルトでスキップされます。`logo_output.gif` を確認して再生するのはデバイス側だけです。
- ホーム入力は `UILaunchPage::bind_home_input_group()` を通して再バインドする必要があります。組み込みページや端末ページからホーム画面へ戻る場合も、このグループを復元する必要があります。
- 外部の独立アプリケーションが実行中は `LVGL_RUN_FLAGE=0` になります。この期間に APPLaunch が UI 更新を続けるとは想定しないでください。
- `APPLaunch_lock()` は `cp0_process_exec_blocking()` とロックファイルの協調に依存します。外部アプリケーションが異常終了してロックが解放されない場合、ホーム画面が更新されないように見えることがあります。その場合はロックファイルと holder PID を調査してください。
