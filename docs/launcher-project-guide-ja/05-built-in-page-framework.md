# 05 - Built-in Page Framework

この章では、組み込み APPLaunch ページを追加するためのクラス階層、ライフサイクル、ページ一覧、ページ登録方法、規約を説明します。主なソースファイルは `projects/APPLaunch/main/ui/ui_app_page.hpp`、`projects/APPLaunch/main/ui/page_app/*.hpp`、`projects/APPLaunch/main/ui/launch.cpp`、`projects/APPLaunch/main/ui/ui_launch_page.cpp` です。

## 1. What a Built-in Page Is

組み込みページとは、APPLaunch プロセスにコンパイルされる LVGL ページクラスです。外部 `.desktop` アプリケーションとは異なります。

- 組み込みページは `lv_obj_t *root_screen_` を直接作成し、`lv_disp_load_scr(page->screen())` によって自身の画面へ切り替えます。
- ページオブジェクトは `Launch::app_Page` に保存され、終了時には `navigate_home` コールバックによって非同期に解放されます。
- ページはホーム画面と同じ APPLaunch プロセス、LVGL メインループ、入力スレッド、リソース解決、`cp0_lvgl_app.h` のシステムインターフェースを共有します。
- ページは通常 header-only で、`projects/APPLaunch/main/ui/page_app/` 配下に置かれ、`build/generated/include/generated/page_app.h` によって集約されます。

簡略化した関係:

```text
UILaunchPage home carousel
        |
        v
Launch::launch_app()
        |
        +-- External command: cp0_process_exec_blocking()
        +-- Terminal command: UISTPage + PTY
        +-- Built-in page: std::make_shared<PageT>()
                         |
                         v
                    lv_disp_load_scr(page->screen())
```

## 2. Page Base-Class Hierarchy

### 2.1 `AppPageRoot`

`AppPageRoot` はすべての組み込みページのルート基底クラスです。場所は `projects/APPLaunch/main/ui/ui_app_page.hpp` です。独立した screen と LVGL 入力グループを作成します。

```cpp
class AppPageRoot
{
public:
    std::string page_title_ = "APP";
    lv_group_t *input_group_;
    lv_obj_t *root_screen_;
    std::function<void(void)> navigate_home;
    bool has_bottom_bar_ = false;
    int top_bar_height_px_ = 20;

    AppPageRoot()
    {
        creat_base_UI();
        creat_input_group();
    }

    virtual ~AppPageRoot()
    {
        lv_obj_del(root_screen_);
        lv_group_delete(input_group_);
    }
};
```

重要な点:

- `root_screen_` はページ自身の top-level screen であり、ホーム `UILaunchPage::screen()` の子ではありません。
- デフォルトでは、`input_group_` には `root_screen_` だけが含まれます。ページ起動時、これが現在の `lv_indev_t` にバインドされます。
- `navigate_home` は `Launch` によって注入されます。ページは ESC またはタスク完了後にこれを呼んでホームへ戻ります。
- デストラクタは `root_screen_` と `input_group_` を削除するため、ページ内に作成された LVGL child objects は screen とともに解放されます。

### 2.2 Top Bar, Content Area, and Bottom Bar Regions

`ui_app_page.hpp` はページを複数の再利用可能な領域に分割します。

| Class | Responsibility | Default size |
| --- | --- | --- |
| `AppTopBarRegion` | タイトル、WiFi、時刻、バッテリーを表示する上部ステータスバーを作成する | Height `20px` |
| `AppContentRegion` | `ui_APP_Container` コンテンツ領域を作成する | Height `150px`、bottom bar がある場合は `130px` |
| `AppBottomBarRegion` | `ui_BOTTOM_Container` bottom bar を作成する | Height `20px` |
| `AppPageLayout` | Top bar + content area | `320x170` 内で `20+150` |
| `AppPageWithBottomBarLayout` | Top bar + content area + bottom bar | `20+130+20` |
| `home_base` | ホーム専用の基底クラス。AppPage と完全に同等ではない | Home status bar + carousel container |

典型的なページは `AppPage` を直接継承します。

```cpp
class UIIpPanelPage : public AppPage
{
public:
    UIIpPanelPage() : AppPage()
    {
        set_page_title("IP INFO");
        creat_UI();
        event_handler_init();
    }
};
```

一部のゲームやフルスクリーンページは `AppPageRoot` を継承し、デフォルトの top bar を使わずに `320x170` 全体を自分で占有します。例として `UIGamePage`、`UICompassPage`、`UITankBattlePage` があります。

## 3. Top Bar and Status Refresh

共通 top bar は `UIAppTopBar` によって実装され、次を含みます。

- 左側タイトル: `set_page_title()` は最終的に `top_bar_.set_title()` を更新します。
- WiFi 信号: `cp0_wifi_get_status()`。未接続時は WiFi panel を非表示にします。
- 時刻: `cp0_time_str()`。デフォルトでは 5 秒ごとに更新されます。
- バッテリー: `LV_EVENT_BATTERY` に応答し、`cp0_battery_info_t` を使ってパーセントと bar を更新します。

主要ソースパス:

- `projects/APPLaunch/main/ui/ui_app_page.hpp`: `UIAppTopBar`、`AppTopBarRegion`。
- `ext_components/cp0_lvgl/include/cp0_lvgl_app.h`: `cp0_wifi_get_status()`、`cp0_time_str()`、`cp0_battery_read()` などのインターフェース宣言。

Top-bar リソースは `cp0_file_path_c()` を使います。

```cpp
lv_obj_set_style_bg_img_src(time_panel_,
    cp0_file_path_c("status_time_background.png"),
    LV_PART_MAIN | LV_STATE_DEFAULT);
```

注意: 通常の組み込みページは自身のステータス更新タイマーを持ちます。ページが自分で作成したタイマーはデストラクタで解放する必要があります。`AppTopBarRegion` は top-bar status timer をすでに解放します。

## 4. Page Lifecycle

### 4.1 Launching a Built-in Page from Home

`launch.cpp` は template を通して組み込みページの app descriptor を構築します。

```cpp
template <class PageT>
app::app(std::string name, std::string icon, page_t<PageT>)
    : Name(std::move(name)), Icon(std::move(icon))
{
    launch = [](Launch *ctx) {
        auto p = std::make_shared<PageT>();
        ctx->app_Page = p;
        p->navigate_home = std::bind(&Launch::go_back_home, ctx);
        lv_disp_load_scr(p->screen());
        lv_indev_set_group(lv_indev_get_next(NULL), p->input_group());
    };
}
```

`projects/APPLaunch/main/ui/launch.cpp` の実際のコードでは、中核フローは次のとおりです。

1. ユーザーがホーム画面で ENTER を離すと、`UILaunchPage::handle_home_key()` が `launch_selected_app()` を呼びます。
2. `UILaunchPage::launch_selected_app()` は `Launch::launch_app()` へ転送します。
3. `Launch::launch_app()` は現在の app を見つけ、その app の `launch` 関数を実行します。
4. 組み込みページオブジェクトが作成され、screen が読み込まれ、入力グループが切り替わります。
5. ページは ESC 後、または業務ロジック完了後に `navigate_home()` を呼びます。
6. `Launch::go_back_home()` は `lv_async_call()` を使ってホーム画面へ戻り、ホーム入力グループを再バインドし、`app_Page` を reset します。

### 4.2 Returning Home

ホームへ戻る処理はすべて `navigate_home` 経由にしてください。ページ内から直接ページを削除してはいけません。

```cpp
if (navigate_home)
    navigate_home();
```

`Launch::lv_go_back_home()` は次を行います。

- `lv_timer_enable(true)` で LVGL タイマーを復元します。
- `UILaunchPage::bind_home_input_group()` でホーム入力グループをバインドします。
- `launch_page_->show_home_screen()` でホーム画面を読み込み、ホーム入力グループをバインドします。
- `app_Page.reset()` で現在ページオブジェクトを解放します。

注意:

- ページのデストラクタは、ページが作成した `lv_timer_t`、バックグラウンドスレッド、ファイル watcher、PTY、音声リソースを停止する必要があります。
- キーボードイベントコールバックスタック内で直接 `delete this` しないでください。`navigate_home` を使い、`Launch` に非同期で処理させてください。
- ページが一時的に子ページや nested page へ切り替える場合は、正しい入力グループを復元する必要があります。

## 5. Current Built-in Page List

ページ実装は `projects/APPLaunch/main/ui/page_app/` に集中しています。

| Page class | File | Launcher name | Inheritance | Description |
| --- | --- | --- | --- | --- |
| `UISTPage` | `ui_app_st.hpp` | `CLI` or terminal external command | `AppPage` | 端末エミュレータ。PTY read/write、ANSI/VT シーケンス、キーボード escape sequence をサポート |
| `UIGamePage` | `ui_app_game.hpp` | `GAME` | `AppPageRoot` | Snake game。フルスクリーンのカスタム描画で LVGL timer により駆動 |
| `UISetupPage` | `ui_app_setup.hpp` | `SETTING` | `AppPage` | システム設定、アプリ切り替え、brightness、volume、WiFi、camera resolution など |
| `UICompassPage` | `ui_app_compass.hpp` | `Compass` | `AppPageRoot` | コンパスページ。sensor thread + UI timer |
| `UIIpPanelPage` | `ui_app_ip_panel.hpp` | `IP_PANEL` | `AppPage` | ネットワークインターフェース/IP 情報リスト。毎秒更新 |
| `UIFilePage` | `ui_app_file.hpp` | `FILE` | `AppPage` | ファイルブラウザ。ディレクトリ一覧と enter/back navigation |
| `UISSHPage` | `ui_app_ssh.hpp` | `SSH` | `AppPage` | SSH パラメータ入力。接続後に `UISTPage` を埋め込む |
| `UIMeshPage` | `ui_app_mesh.hpp` | `MESH` | `AppPage` | Mesh メッセージ一覧、入力 overlay、send/refresh |
| `UIRecPage` | `ui_app_rec.hpp` | `REC` | Custom `rec_page` | 録音/再生/ファイル一覧と非同期リソース管理 |
| `UICameraPage` | `ui_app_camera.hpp` | `CAMERA` | `AppPage` | カメラ preview、gallery、capture、status page |
| `UILoraPage` | `ui_app_lora.hpp` | `LORA` | `AppPage` | LoRa 業務ページ。内部に C スタイルの create/destroy インターフェースも含む |
| `UITankBattlePage` | `ui_app_tank_battle.hpp` | `TANK` | `AppPageRoot` | Tank mini-game。フルスクリーン、固定キー mapping |

`Python`、`STORE`、`MATH` は組み込みページではありません。コマンドまたは外部プロセスとして起動されます。

## 6. Page Registration and Display Order

組み込みエントリは `launch.cpp` の `kBuiltinApps[]` で宣言されます。最初の 5 つの有効なエントリが、まず 5 つのホームカルーセルスロットを初期化します。

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

組み込みページの表示可否は現在 `kBuiltinApps[]` と `AppDescriptor.config_key` によって駆動されます。`Launch::rebuild_builtin_apps()` は各 descriptor を追加する前に `launcher_app_registry_is_enabled()` を呼び、Settings の変更は `launcher_app_registry_set_enabled()` の後に `Launch::applications_reload()` を呼びます。

規約:

- `Store`、`CLI`、`Game`、`Setting` は settings page で常に有効で、無効化できません。
- `Compass` は現在 `launch.cpp` で無条件に追加され、`UISetupPage` の Launcher toggle list では制御されません。
- `IP_PANEL`、`FILE`、`SSH`、`MESH`、`REC`、`CAMERA`、`LORA`、`TANK` などのページは Linux デバイスビルドでのみ追加されます。SDL ビルドでは `#if defined(__linux__) && !defined(HAL_PLATFORM_SDL)` により制限されます。
- 動的 `.desktop` アプリケーションは組み込みページの後にスキャンされ追加されます。ディレクトリ変更は watcher により 3 秒ごとに確認されます。

## 7. Page Code Skeleton

新しい通常ページは、一般に `AppPage` を継承します。

```cpp
#pragma once
#include "../ui_app_page.hpp"
#include "compat/input_keys.h"

class UINewPage : public AppPage
{
public:
    UINewPage() : AppPage()
    {
        set_page_title("NEW");
        create_ui();
        event_handler_init();
    }

    ~UINewPage()
    {
        if (timer_) {
            lv_timer_delete(timer_);
            timer_ = nullptr;
        }
    }

private:
    lv_timer_t *timer_ = nullptr;

    void create_ui()
    {
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, 320, 150);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    }

    void event_handler_init()
    {
        lv_obj_add_event_cb(root_screen_, &UINewPage::event_cb, LV_EVENT_ALL, this);
    }

    static void event_cb(lv_event_t *e)
    {
        auto *self = static_cast<UINewPage *>(lv_event_get_user_data(e));
        if (!self || !IS_KEY_RELEASED(e))
            return;

        uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
        if (key == KEY_ESC && self->navigate_home)
            self->navigate_home();
    }
};
```

新しいフルスクリーンページは `AppPageRoot` を継承してもかまいませんが、`320x170` レイアウト、ステータスヒント、戻るキーを自分で扱う必要があります。

## 8. Page UI Conventions

- `320x170` 解像度を前提に設計します。共通ページコンテンツ領域は `320x150` で、上部 `20px` は top bar が占有します。
- ページオブジェクトは通常 `std::unordered_map<std::string, lv_obj_t *> ui_obj_` に保存し、再描画や削除をしやすくします。
- リストページでは、小さな画面で focus が混乱しないよう、自由な LVGL container scrolling よりも固定 row height + virtual scrolling を優先してください。
- 頻繁に更新するページでは `lv_timer_create()` を使い、デストラクタで `lv_timer_delete()` を呼びます。
- バックグラウンドスレッドや非同期コールバックには `std::atomic<bool>` alive flag を使い、解放済みページにコールバックが触れないようデストラクタでスレッドを停止します。
- 画像、音声、フォントの相対パスをハードコードしないでください。`img_path()`、`audio_path()`、`cp0_file_path_c()` を使います。

## 9. Nested Pages and Special Pages

`UISSHPage` は典型的な nested page です。SSH パラメータ入力中は `UISSHPage` がキーボードを処理します。接続後は `UISTPage` を作成し、screen と input group を切り替えます。

```cpp
terminal_page_ = std::make_shared<UISTPage>();
terminal_page_->navigate_home = [this]() {
    terminal_page_.reset();
    view_state_ = ViewState::INPUT;
    lv_disp_load_scr(this->screen());
    lv_indev_set_group(lv_indev_get_next(NULL), this->input_group());
};

lv_disp_load_scr(terminal_page_->screen());
lv_indev_set_group(lv_indev_get_next(NULL), terminal_page_->input_group());
```

この種のページには特別な注意が必要です。

- 子ページの終了は必ずしもホーム復帰を意味しません。親ページへ戻るだけの場合があります。
- 入力グループは現在の画面と一緒に切り替える必要があります。そうしないとキーが見えないページへ配送されます。
- 親ページのデストラクタは、先に子ページオブジェクトを解放する必要があります。

## 10. Relationship with the Home Carousel

ホームカルーセル自体は `ui_launch_page.cpp` が管理します。

- `carousel_elements` は 5 枚のカード、5 つのタイトル、5 つのページドットを保存します。
- 左右切り替え時には `switch_left()` / `switch_right()` が呼ばれます。アニメーション完了後、配列が回転され、`Launch` が far-side スロットの内容を更新します。
- ENTER は `UILaunchPage::launch_selected_app()` を発火し、最終的に現在 app の `launch()` を呼びます。

組み込みページはホームカルーセルを直接操作しません。ホームへ戻った後、カルーセル状態は `Launch` により保持されます。

## 11. Common Notes

- ページコンストラクタ内で長時間ブロックする処理を行わないでください。まずページまたは loading 状態を表示し、その後タスクを開始してください。
- `lv_indev_get_next(NULL)` が常に non-null だと仮定しないでください。入力グループを切り替える前に確認するのが望ましいです。
- 明確にホーム画面機能でない限り、ページからホームのグローバルオブジェクトへ直接アクセスしないでください。
- ページタイトルは内部 top-bar label を直接変更せず、`set_page_title()` を呼びます。
- 終了可能なすべてのページは `KEY_ESC` をサポートし、`navigate_home` または前の view へ戻る処理を呼ぶ必要があります。
- ページ toggle key は `AppDescriptor.config_key`、`UISetupPage::save_app_toggle()`、`launch.cpp` の `launcher_app_registry_is_enabled()` と一貫している必要があります。
