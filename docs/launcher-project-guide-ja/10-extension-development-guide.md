# 10 - 拡張開発ガイド

この章では APPLaunch の拡張方法を説明します。特に、組み込みページの追加、外部 `.desktop` アプリの追加、画像/音声/フォントアセットの追加、設定トグルの変更という 4 つの一般的な変更に焦点を当てます。中核コードは `projects/APPLaunch/main/ui` 配下にあり、プラットフォーム適応とパス解決は `ext_components/cp0_lvgl` 配下にあります。

## 1. 拡張前に理解しておく入口

| Entry point | Purpose |
| --- | --- |
| `projects/APPLaunch/main/ui/launch.cpp` | 固定アプリリスト、動的 `.desktop` スキャン、組み込みページまたは外部プロセスの起動 |
| `projects/APPLaunch/main/ui/page_app/` | 組み込みページ実装ディレクトリ。ページは通常 header-only の `.hpp` ファイル |
| `projects/APPLaunch/main/ui/ui_app_page.hpp` | `AppPage`、トップバー、`img_path()`、`audio_path()` などの共通ページ機能 |
| `projects/APPLaunch/main/ui/generate_page_app_includes.py` | ビルド前に `generated/page_app.h` を自動生成し、すべての `page_app/*.hpp` ファイルを include |
| `projects/APPLaunch/APPLaunch/` | 実行時アセットツリー。パッケージング後はデバイス上の `/usr/share/APPLaunch/` に対応 |
| `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_file.cpp` | デバイス側 `cp0_file_path()` パスルール |
| `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_file.cpp` | SDL2 開発ホスト側 `cp0_file_path()` パスルール |
| `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_config.cpp` | デバイス側設定永続化。`/var/lib/applaunch/settings` に保存 |

APPLaunch には 2 種類のアプリソースがあります。

- **組み込みページ**: APPLaunch プロセスにコンパイルされ、`kBuiltinApps[]` で `append_page_app<PageT>` を使って登録されます。開くと APPLaunch が `PageT` オブジェクトを作成し、その画面へ切り替えます。
- **外部アプリ**: 固定 `Exec` 値または `.desktop` 記述子を通じて独立プロセスとして起動されます。非ターミナルアプリでは、Launcher は LVGL タイマーを一時停止し、子プロセス終了を待ってからホームページへ戻ります。

## 2. 組み込みページを追加する

組み込みページは、Launcher と同じプロセスで動作し、LVGL を直接使い、入力グループ、トップバー、ステータスバーを共有する必要がある機能に適しています。例として Settings、Game、File、Camera、LoRa ページがあります。

### 2.1 ページファイルを作成

`projects/APPLaunch/main/ui/page_app/` 配下に新しい `.hpp` を作成します。推奨命名スタイルは `ui_app_xxx.hpp` です。ページクラスは `AppPage` を継承し、コンストラクタでタイトル設定、UI 作成、キーイベントのバインドを行います。

最小スケルトン:

```cpp
#pragma once

#include "../ui_app_page.hpp"

class UIMyToolPage : public AppPage
{
public:
    UIMyToolPage() : AppPage()
    {
        set_page_title("MY TOOL");
        create_ui();
        event_handler_init();
    }

private:
    lv_obj_t *title_ = nullptr;

    void create_ui()
    {
        lv_obj_t *root = screen();
        lv_obj_set_style_bg_color(root, lv_color_hex(0x101820), LV_PART_MAIN | LV_STATE_DEFAULT);

        UIAppTopBar top("MY TOOL");
        top.create(root);

        title_ = lv_label_create(root);
        lv_label_set_text(title_, "Hello APPLaunch");
        lv_obj_center(title_);
    }

    void event_handler_init()
    {
        lv_obj_add_event_cb(screen(), &UIMyToolPage::key_event_cb, LV_EVENT_KEY, this);
    }

    static void key_event_cb(lv_event_t *e)
    {
        auto *self = static_cast<UIMyToolPage *>(lv_event_get_user_data(e));
        uint32_t key = lv_event_get_key(e);
        if (key == LV_KEY_ESC && self->navigate_home) {
            self->navigate_home();
        }
    }
};
```

注意:

- ページは `AppPage` を継承する必要があります。`screen()`、`input_group()`、`navigate_home` などの機構を再利用するためです。
- ホームへ戻るには `navigate_home()` を呼ぶことを推奨します。ホーム画面を直接 load しないでください。そうしないと `Launch` が現在のページオブジェクトを正しく解放できません。
- ページが LVGL timer、ファイルディスクリプタ、スレッド、周辺機器ハンドルを作成する場合は、デストラクタで解放してください。
- ページサイズは 320x170 を基準にします。一般的なレイアウトは 20 px のトップバーと 320x150 の本文です。
- アセットの絶対パスをハードコードしないでください。画像は `img_path("xxx.png")`、音声は `audio_path("xxx.wav")` を使います。

### 2.2 ページが include されることを確認

`projects/APPLaunch/main/SConstruct` はビルド前にこのスクリプトを実行します。

```python
ui/generate_page_app_includes.py
```

このスクリプトは `projects/APPLaunch/main/ui/page_app/*.hpp` をスキャンし、`projects/APPLaunch/build/generated/include/generated/page_app.h` を生成します。ほとんどの場合、ファイル拡張子が `.hpp` であればビルド中に自動 include されます。

手動確認する場合、`generated/page_app.h` には次が含まれるはずです。

```cpp
#include "page_app/ui_app_my_tool.hpp"
```

### 2.3 ホームのアプリリストへ登録

`projects/APPLaunch/main/ui/launch.cpp` を開き、`kBuiltinApps[]` に組み込み登録を追加します。

```cpp
{{"MYTOOL", "mytool_100.png", "app_MyTool", true, false},
 nullptr, false, true, false, append_page_app<UIMyToolPage>},
```

ページがデバイス側ハードウェアだけをサポートする場合、SDL2 ビルド失敗を避けるため、既存の `#if defined(__linux__) && !defined(HAL_PLATFORM_SDL)` ブロック内に置いてください。

登録フィールド:

- `AppDescriptor.label` はホームカルーセルの表示名です。小さい画面で切れないよう短くしてください。
- `AppDescriptor.icon` は `cp0_file_path()` で解決されるアイコンファイル名です。
- `AppDescriptor.config_key` は Settings 永続化用の完全な key です。例: `app_MyTool`。
- `append_page_app<PageT>` は、アプリをクリックしたときに組み込みページが作成されることを意味します。

### 2.4 Settings ページのトグルを追加

Settings の `Launcher` メニューは `launcher_app_registry_entries()` から生成されます。表示トグルを出すには、`kBuiltinApps[]` エントリの `AppDescriptor.configurable` を `true` にします。

```cpp
{{"MYTOOL", "mytool_100.png", "app_MyTool", true, false},
 nullptr, false, true, false, append_page_app<UIMyToolPage>},
```

`launcher_app_registry_set_enabled()` は `config_key` で switch を保存します。例: `app_MyTool=0`。`Launch::rebuild_builtin_apps()` は `launcher_app_registry_is_enabled()` を通して読み取ります。

デバイス側の永続化ファイル:

```text
/var/lib/applaunch/settings
```

### 2.5 ビルド確認

SDL2 ローカル確認:

```bash
cd projects/APPLaunch
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j8 --implicit-deps-changed
./dist/M5CardputerZero-APPLaunch
```

デバイスクロスコンパイル確認:

```bash
cd projects/APPLaunch
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8 --implicit-deps-changed
```

デバイス専用ページでは、少なくともクロスビルドを実行してください。ページが開発ホストでも表示できる場合は、まず SDL2 で UI とキーを素早く確認します。

## 3. 外部 `.desktop` アプリを追加する

外部 `.desktop` アプリは、独立した実行ファイル、スクリプト、ターミナルコマンドに適しています。C++ のアプリリストを変更する必要はありません。APPLaunch が `applications` ディレクトリをスキャンし、ホームページへ動的に追加します。

### 3.1 `.desktop` ファイルを置く

開発ツリーのパス:

```text
projects/APPLaunch/APPLaunch/applications/
```

インストール後のデバイスパス:

```text
/usr/share/APPLaunch/applications/
```

既存テンプレート:

```text
projects/APPLaunch/APPLaunch/applications/vim.desktop.temple
```

現在のスキャンロジックは `.desktop` で終わるファイル名だけを扱います。`.desktop.temple` はテンプレートであり、読み込まれません。

### 3.2 `.desktop` フィールド形式

最小例:

```ini
[Desktop Entry]
Name=Vim
Exec=vim
Terminal=true
Icon=share/images/email.png
Type=Application
```

APPLaunch が現在解析するフィールド:

| Field | Required | Description |
| --- | --- | --- |
| `Name` | Yes | ホームページに表示する名前 |
| `Exec` | Yes | 実行コマンド。絶対パスまたは shell コマンドを指定可能 |
| `Icon` | No | アイコンパス。推奨形式は `share/images/xxx.png`、または LVGL が読める任意パス |
| `Terminal` | No | `true`/`True`/`1` の場合、組み込み `UISTPage` 内で実行 |
| `Sysplause` | No | ターミナルアプリのみ。ターミナルコマンド終了後の一時停止動作を制御。既定は true |
| `Type` | No | desktop-file 慣例との互換用。APPLaunch は現在依存していない |
| `TryExec` | No | APPLaunch は現在解析しない。説明用フィールドとしてのみ使用可能 |

例 1: ターミナルコマンドを起動。

```ini
[Desktop Entry]
Name=TOP
Exec=top
Terminal=true
Sysplause=false
Icon=share/images/cli_100.png
Type=Application
```

例 2: 独立プログラムを起動。

```ini
[Desktop Entry]
Name=MyApp
Exec=/usr/share/APPLaunch/bin/my_app
Terminal=false
Icon=share/images/my_app_100.png
Type=Application
```

例 3: スクリプトを起動。

```ini
[Desktop Entry]
Name=NetInfo
Exec=/bin/sh /usr/share/APPLaunch/bin/netinfo.sh
Terminal=true
Icon=share/images/ip_panel_100.png
Type=Application
```

### 3.3 外部アプリ起動時の挙動

`launch.cpp` は 2 種類の外部アプリ起動モードをサポートします。

- `Terminal=true`: `UISTPage` を作成し、APPLaunch プロセス内に PTY ターミナルを表示して `Exec` を実行します。
- `Terminal=false`: `cp0_process_exec_blocking()` を呼んで外部プロセスを開始します。APPLaunch は LVGL タイマーと入力グループを一時停止し、子プロセス終了を待ってからホームページを復元します。

非ターミナル外部アプリからの復帰は次の挙動に依存します。

- 子プロセスが正常終了すると、APPLaunch は `launch_page_->show_home_screen()` を呼んでホーム画面と入力グループを復元します。
- デバイス上では ESC を約 3 秒押し続けると外部アプリのプロセスグループへ SIGTERM を送ります。さらに 3 秒後も終了していなければ SIGKILL が送られます。
- `cp0_process_exec_blocking()` は Launcher のキーボードスレッドを一時停止し、外部プログラムが evdev 入力を直接読めるようにします。

### 3.4 動的更新

APPLaunch は起動時に `applications_load()` を呼んで `.desktop` ファイルをスキャンします。その後、`inotify`/SDL ディレクトリ監視が 3 秒ごとにアプリケーションディレクトリを確認します。`.desktop` ファイルの追加、削除、編集後、通常は Launcher を再起動しなくてもカルーセルが自動更新されます。

更新されない場合:

```bash
# Device side
ls -l /usr/share/APPLaunch/applications
journalctl --user -u APPLaunch.service -f

# SDL2 development host: confirm APPLaunch/applications exists near the run directory
find projects/APPLaunch -path '*APPLaunch/applications*' -maxdepth 5 -type f
```

### 3.5 重複排除ルール

動的アプリは `Exec` によって重複排除されます。2 つの `.desktop` ファイルが完全に同じ `Exec` を持つ場合、後からスキャンされたファイルはスキップされ、次のメッセージが出力されます。

```text
applications_load: skip ... (duplicate Exec)
```

## 4. アセットを追加する

アセットには画像、音声、フォント、外部プログラム/スクリプトが含まれます。開発ツリーでは `projects/APPLaunch/APPLaunch/` 配下に置きます。ビルド時、`main/SConstruct` は `STATIC_FILES += [ADir('../APPLaunch')]` により、このツリーを出力/インストールパッケージへコピーします。

### 4.1 アセットディレクトリ

| Type | Development-tree path | Device path | Recommended access method |
| --- | --- | --- | --- |
| Images | `projects/APPLaunch/APPLaunch/share/images/` | `/usr/share/APPLaunch/share/images/` | `img_path("xxx.png")` または `.desktop` `Icon=share/images/xxx.png` |
| Audio | `projects/APPLaunch/APPLaunch/share/audio/` | `/usr/share/APPLaunch/share/audio/` | `audio_path("xxx.wav")` |
| Fonts | `projects/APPLaunch/APPLaunch/share/font/` | `/usr/share/APPLaunch/share/font/` | `launcher_fonts().get("xxx.ttf", size, style)` |
| External apps | `projects/APPLaunch/APPLaunch/bin/` | `/usr/share/APPLaunch/bin/` | `.desktop` `Exec=/usr/share/APPLaunch/bin/xxx` |
| `.desktop` | `projects/APPLaunch/APPLaunch/applications/` | `/usr/share/APPLaunch/applications/` | 自動スキャン |

`bin/` ディレクトリやスクリプトを追加する場合は、スクリプトに実行権限を付けるか、`.desktop` ファイルで `/bin/sh script.sh` 経由で呼び出してください。

### 4.2 `cp0_file_path()` パスルール

デバイス側 `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_file.cpp` の主要ルール:

- `cp0_file_path("applications")` -> `/usr/share/APPLaunch/applications`
- `cp0_file_path("lock_file")` -> `/tmp/M5CardputerZero-APPLaunch_fcntl.lock`
- 画像拡張子 `png/gif/jpg/jpeg/svg` -> `share/images/<file>`
- 音声拡張子 `wav/mp3/ogg` -> `/usr/share/APPLaunch/share/audio/<file>`
- フォント拡張子 `ttf/otf` -> `/usr/share/APPLaunch/share/font/<file>`

SDL2 では、`sdl_lvgl_file.cpp` が実行ファイルディレクトリ、カレントディレクトリ、`APPLaunch/share` からアセットルートを推定し、開発ホストで扱いやすいようカレントディレクトリ相対のパスへ変換します。

### 4.3 画像アセットの推奨事項

- ホームカルーセルアイコンには `mytool_100.png` のような 100 px 版を用意してください。
- ページ内の小さなアイコンには、必要に応じて 80 px 以下の版を用意します。
- LVGL は画像パスと形式に敏感です。リポジトリ既存の PNG 命名とサイズスタイルを再利用するのが最も安全です。
- `panel_set_icon()` が `missing/unreadable` を出力する場合、まずファイルがソースディレクトリだけでなく実行時アセットツリーにも存在するか確認します。

### 4.4 音声アセットの推奨事項

ページのキー音では `UISetupPage` を参考にします。

```cpp
std::string snd_enter_ = audio_path("key_enter.wav");
cp0_signal_audio_api({"PlayFile", snd_enter_}, nullptr);
```

デバイス側音声は通常 `/usr/share/APPLaunch/share/audio/xxx.wav` を使います。SDL2 側はパス適応レイヤーで解決されます。

### 4.5 フォントアセットの推奨事項

フォントは `share/font/` 配下に置きます。ページでは繰り返し作成を避けるため、共有フォントキャッシュを優先してください。

```cpp
lv_font_t *font = launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD);
lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
```

フォント追加後は、SDL2 とデバイスビルドの両方で FreeType が有効であることを確認してください。SDL2 設定とクロスビルド設定はいずれも LVGL に FreeType 関連 include/link パラメータを追加します。

## 5. Settings トグルを変更する

Settings ページは `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp` に集約されています。現在の設定には Launcher アプリ可視性トグル、Boot、Screen、WiFi、Speaker、Camera、Info、About、Help、ExtPort などがあります。

### 5.1 Launcher アプリトグルを追加

手順:

1. `kBuiltinApps[]` でアプリの `AppDescriptor` を追加または更新します。
2. `configurable=true` にし、`app_MyTool` のような完全な `config_key` を選びます。
3. Settings ページを開き、`Launcher` メニューへ入り、O/X を切り替えます。
4. Settings は `launcher_app_registry_set_enabled()` を呼び、Launcher に組み込みアプリリストの再構築を通知します。

### 5.2 通常設定を追加

`menu_init()` の対応するグループを探し、`sub_items` に項目を追加します。

```cpp
{"My Option", true, cp0_config_get_int("my_option", 1) != 0, [this]() {
    bool en = cp0_config_get_int("my_option", 1) == 0;
    cp0_config_set_int("my_option", en ? 1 : 0);
    cp0_config_save();
}},
```

値を選択する第 2 階層または第 3 階層ページについては、既存実装を参照してください。

- `enter_brightness_adjust()`: 輝度選択。
- `enter_darktime_adjust()`: 画面オフタイムアウト選択。
- `enter_volume_adjust()` と `apply_volume()`: 音量保存と適用。
- `enter_camera_resolution()`: カメラ解像度。
- `enter_startup_mode()`: 起動モード。

### 5.3 設定永続化場所

デバイス側設定実装: `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_config.cpp`。

- 設定ディレクトリ: `/var/lib/applaunch`
- 設定ファイル: `/var/lib/applaunch/settings`
- 形式: 1 行に 1 つの `key=value`
- 最大エントリ数: `MAX_ENTRIES=32`

よく使うコマンド:

```bash
sudo cat /var/lib/applaunch/settings
sudo sed -i 's/^app_Game=.*/app_Game=1/' /var/lib/applaunch/settings
systemctl --user restart APPLaunch.service
```

設定項目を多数追加する場合、現在の最大数が 32 エントリであることに注意してください。この上限を超えると `cp0_config_set_*` はすぐ return し、設定は保存されません。

## 6. 拡張時の確認チェックリスト

| Check item | Method |
| --- | --- |
| ファイルが正しいディレクトリだけに置かれている | 組み込みページは `main/ui/page_app/`、アセットは `APPLaunch/share/`、`.desktop` ファイルは `APPLaunch/applications/` |
| SDL2 ビルドが成功する | `CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j8 --implicit-deps-changed` |
| デバイスクロスビルドが成功する | `CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8 --implicit-deps-changed` |
| アイコンが正しく表示される | `set panel icon missing/unreadable` ログを確認 |
| ページがホームへ戻れる | 組み込みページは ESC で `navigate_home()` を呼ぶ。外部ページは自分で終了するか、長押し ESC で戻る |
| `.desktop` が読み込まれる | ファイル名が `.desktop` で終わり、`[Desktop Entry]`、`Name`、`Exec` を含む |
| 設定が保存される | 対応キーが `/var/lib/applaunch/settings` に書き込まれているか確認 |
