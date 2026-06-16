# 12 - よく使う変更入口

この章では、「何を変更したいか」に応じて APPLaunch の一般的な入口を整理します。変更前には、他の agent の変更を上書きしないよう、まず現在の作業ツリー状態を確認してください。

```bash
cd /home/nihao/w2T/github/launcher
git status --short
```

## 1. 高頻度タスク入口表

| Task | Main files/directories | Key points | Verification |
| --- | --- | --- | --- |
| 組み込みページを追加 | `projects/APPLaunch/main/ui/page_app/` | `ui_app_xxx.hpp` を作成し、`AppPage` を継承 | SDL2 でビルドし、ページを開く |
| ホームに組み込みページを登録 | `projects/APPLaunch/main/ui/launch.cpp` | `kBuiltinApps[]` に `append_page_app<PageT>` エントリを追加 | ホームカルーセルにアイコンが表示される |
| 組み込みページ表示トグルを制御 | `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp`, `projects/APPLaunch/main/ui/launch.cpp` | Settings が `AppDescriptor.config_key` を書き、Launcher が `launcher_app_registry_is_enabled()` を読む | Settings で切替後、再起動またはホーム更新 |
| 外部 `.desktop` アプリを追加 | `projects/APPLaunch/APPLaunch/applications/` | ファイル名は `.desktop` で終わり、`Name` と `Exec` を含む必要がある | skip ログなしでホームに表示される |
| アイコンを追加 | `projects/APPLaunch/APPLaunch/share/images/` | 組み込みページは `img_path()`、`.desktop` は `Icon=share/images/xxx.png` を使う | `missing/unreadable` ログがない |
| 効果音を追加 | `projects/APPLaunch/APPLaunch/share/audio/` | ページは `audio_path()` と `cp0_signal_audio_api()` を使う | デバイスで音が鳴る |
| フォントを追加 | `projects/APPLaunch/APPLaunch/share/font/` | `launcher_fonts().get()` を使い、FreeType 依存を確認 | ページテキストが新しいフォントを使う |
| ホームカルーセルレイアウトを変更 | `projects/APPLaunch/main/ui/ui_launch_page.cpp`, `projects/APPLaunch/main/ui/ui_launch_page.h` | 5 slots、左右切替、中央カード | SDL2 でアニメーションと入力を確認 |
| カルーセルアニメーションを変更 | `projects/APPLaunch/main/ui/animation/ui_launcher_animation.cpp` | カード移動、scale、opacity などのアニメーション | SDL2 で左右切替を繰り返す |
| ホームステータスバーを変更 | `projects/APPLaunch/main/ui/launch.cpp`, `projects/APPLaunch/main/ui/ui.cpp` | `update_home_status_bar()` が WiFi/time/battery を更新 | `[HOME_STATUS]` ログを確認 |
| Settings メニューを変更 | `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp` | `menu_init()` に `MenuItem`/`SubItem` を追加 | SETTING ページに入りテスト |
| 設定保存ロジックを変更 | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_config.cpp` | 現在は `/var/lib/applaunch/settings` に保存、最大 32 エントリ | settings ファイルを確認 |
| アセットパスルールを変更 | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_file.cpp`, `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_file.cpp` | デバイスと SDL2 の整合性を考慮 | SDL2 とデバイスの両方でアセット確認 |
| 外部アプリ起動/復帰を変更 | `projects/APPLaunch/main/ui/launch.cpp`, `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_process.cpp` | `launch_Exec()`, `cp0_process_exec_blocking()` | 外部アプリ起動、ESC で戻る |
| ターミナルアプリを変更 | `projects/APPLaunch/main/ui/page_app/ui_app_st.hpp`, `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_pty.cpp` | PTY、コマンド実行、入出力 | `Terminal=true` アプリで確認 |
| 入力マッピングを変更 | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_keyboard.c`, `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_keyboard.c` | デバイスと SDL2 の入力差分 | `evtest` + SDL2 keyboard |
| 起動フローを変更 | `projects/APPLaunch/main/src/main.cpp` | `lv_init()`、`cp0_lvgl_init()`、`ui_init()`、main loop | `[BOOT]` ログを確認 |
| ビルド依存関係を変更 | `projects/APPLaunch/main/SConstruct` | `SRCS`, `INCLUDE`, `REQUIREMENTS`, `STATIC_FILES` | scons build |
| ビルド設定を変更 | `projects/APPLaunch/*.mk` | SDL2、device、cross build ごとに異なる設定 | 特定の `CONFIG_DEFAULT_FILE` でビルド |
| パッケージ内容を変更 | `scripts/debian_packager.py`, `projects/APPLaunch/APPLaunch/` | アセットツリーとインストールパス | パッケージ作成後のファイル一覧を確認 |
| platform HAL を変更 | `ext_components/cp0_lvgl/src/cp0/`, `ext_components/cp0_lvgl/include/hal/` | framebuffer、audio、network、settings、process など | デバイスでテスト |

## 2. ソースディレクトリ早見表

| Path | Purpose |
| --- | --- |
| `projects/APPLaunch/main/src/main.cpp` | APPLaunch プロセス入口、初期化順序、メインループ、外部アプリロック検出 |
| `projects/APPLaunch/main/ui/ui.cpp` | グローバル LVGL UI オブジェクトを作成。多くの `ui_*` グローバルはここから来る |
| `projects/APPLaunch/main/ui/ui.cpp` | C++ UI 初期化ブリッジ |
| `projects/APPLaunch/main/ui/ui.h` | UI グローバル宣言と C/C++ 共通インターフェース |
| `projects/APPLaunch/main/ui/launch.cpp` | アプリモデル、アプリリスト、起動ロジック、動的 `.desktop` 読み込み、ステータスバー更新 |
| `projects/APPLaunch/main/ui/launch.h` | `Launch` の public wrapper class |
| `projects/APPLaunch/main/ui/ui_launch_page.cpp` | ホーム画面、カルーセルスロット、入力イベント、ホームページ挙動 |
| `projects/APPLaunch/main/ui/ui_launch_page.h` | ホームクラスインターフェース。panel/label/input group accessor を含む |
| `projects/APPLaunch/main/ui/ui_loading.cpp` | Loading オーバーレイ表示/非表示 |
| `projects/APPLaunch/main/ui/ui_global_hint.cpp` | グローバルヒントオーバーレイ |
| `projects/APPLaunch/main/ui/launcher_ui_runtime.cpp` | LVGL OS/thread helpers |
| `projects/APPLaunch/main/ui/animation/` | ホームカルーセルアニメーション実装 |
| `projects/APPLaunch/main/ui/ui_app_page.hpp` | 組み込みページ基底クラス、トップバー、共有アセットパス helper |
| `projects/APPLaunch/build/generated/include/generated/page_app.h` | 自動生成される組み込みページ include 集約 |
| `projects/APPLaunch/main/ui/page_app/` | 組み込みページ実装ディレクトリ |
| `ext_components/cp0_lvgl/include/` | 共有 CP0/LVGL ヘッダー。keyboard と互換 input ヘッダーを含む |

## 3. 組み込みページ入口表

| Page/feature | File | Registered name or icon | Description |
| --- | --- | --- | --- |
| SETTING | `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp` | `SETTING` / `setting_100.png` | Settings ページ。app toggles、brightness、volume、WiFi、camera などを含む |
| GAME | `projects/APPLaunch/main/ui/page_app/ui_app_game.hpp` | `GAME` / `game_100.png` | 組み込みゲームエントリ |
| Compass | `projects/APPLaunch/main/ui/page_app/ui_app_compass.hpp` | `Compass` / `compass_needle_80.png` | Compass ページ |
| IP_PANEL | `projects/APPLaunch/main/ui/page_app/ui_app_ip_panel.hpp` | `IP_PANEL` / `ip_panel_100.png` | IP 情報パネル。デバイスで有効 |
| FILE | `projects/APPLaunch/main/ui/page_app/ui_app_file.hpp` | `FILE` / `file_100.png` | File ページ。デバイスで有効 |
| SSH | `projects/APPLaunch/main/ui/page_app/ui_app_ssh.hpp` | `SSH` / `ssh_100.png` | SSH ページ。デバイスで有効 |
| MESH | `projects/APPLaunch/main/ui/page_app/ui_app_mesh.hpp` | `MESH` / `mesh_100.png` | Mesh ページ。デバイスで有効 |
| REC | `projects/APPLaunch/main/ui/page_app/ui_app_rec.hpp` | `REC` / `rec_100.png` | 録音ページ。デバイスで有効 |
| CAMERA | `projects/APPLaunch/main/ui/page_app/ui_app_camera.hpp` | `CAMERA` / `camera_100.png` | Camera ページ。デバイスで有効 |
| LORA | `projects/APPLaunch/main/ui/page_app/ui_app_lora.hpp` | `LORA` / `lora_100.png` | LoRa ページ。デバイスで有効 |
| TANK | `projects/APPLaunch/main/ui/page_app/ui_app_tank_battle.hpp` | `TANK` / `tank_100.png` | Tank game。デバイスで有効 |
| CLI/terminal | `projects/APPLaunch/main/ui/page_app/ui_app_st.hpp` | `CLI` / `cli_100.png` | `UISTPage`。bash、python、`Terminal=true` アプリで使用 |

固定エントリは `kBuiltinApps[]` で宣言されます:

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

## 4. 外部アプリ入口表

| Item | Path/function | Description |
| --- | --- | --- |
| `.desktop` directory | `projects/APPLaunch/APPLaunch/applications/` | 開発ツリー。`/usr/share/APPLaunch/applications/` としてパッケージ化 |
| Template | `projects/APPLaunch/APPLaunch/applications/vim.desktop.temple` | サンプルテンプレート。サフィックスが `.desktop` ではないためスキャンされない |
| Scan function | `Launch::applications_load()` in `projects/APPLaunch/main/ui/launch.cpp` | `[Desktop Entry]`、`Name`、`Icon`、`Exec`、`Terminal`、`Sysplause` を解析 |
| Directory watching | `Launch::inotify_init_watch()`, `app_dir_watch_cb()` | アプリケーション変更を監視し、動的アプリリストを更新 |
| Dynamic refresh | `Launch::applications_reload()` | 固定アプリを保持し、動的アプリを削除してから再スキャン |
| Terminal launch | `Launch::launch_Exec_in_terminal()` | `UISTPage` を作成してコマンドを実行 |
| Non-terminal launch | `Launch::launch_Exec()` | LVGL を一時停止し、`cp0_process_exec_blocking()` を呼ぶ |
| Device-side process execution | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_process.cpp` | fork、権限降格、長押し ESC による終了、キーボード復元 |
| PTY execution | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_pty.cpp` | ターミナルページのコマンド実行とユーザー選択 |

最小 `.desktop` テンプレート:

```ini
[Desktop Entry]
Name=MyApp
Exec=/usr/share/APPLaunch/bin/my_app
Terminal=false
Icon=share/images/my_app_100.png
Type=Application
```

## 5. アセット入口表

| Asset | Development path | Access method | Common issue |
| --- | --- | --- | --- |
| Home/app icons | `projects/APPLaunch/APPLaunch/share/images/` | `img_path("xxx.png")` | デバイス側相対パスは作業ディレクトリに依存。`.desktop` は `share/images/xxx.png` を使うべき |
| Page images | `projects/APPLaunch/APPLaunch/share/images/` | `img_path("xxx.png")` または `cp0_file_path_c("xxx.png")` | ファイル名の大文字小文字は完全一致が必要 |
| Audio | `projects/APPLaunch/APPLaunch/share/audio/` | `audio_path("xxx.wav")` | デバイス側音声パスは絶対パス `/usr/share/APPLaunch/share/audio/` |
| Fonts | `projects/APPLaunch/APPLaunch/share/font/` | `launcher_fonts().get("xxx.ttf", size, style)` | FreeType が必要。font object はキャッシュして再利用すべき |
| External binaries/scripts | `projects/APPLaunch/APPLaunch/bin/` | `.desktop` `Exec=/usr/share/APPLaunch/bin/xxx` | 実行権限と動的ライブラリ依存に注意 |
| External app descriptors | `projects/APPLaunch/APPLaunch/applications/` | `.desktop` を自動スキャン | `.desktop.temple` はスキャンされない |
| Packaged libraries | `projects/APPLaunch/APPLaunch/lib/` | プログラムまたはスクリプトから読み込み | 実行時の `LD_LIBRARY_PATH` または rpath に注意 |

パス解決コード:

| Platform | File | Focus |
| --- | --- | --- |
| Device | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_file.cpp` | アセットルートは `/usr/share/APPLaunch` 固定。画像は相対 `share/images/` パスを返す |
| SDL2 | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_file.cpp` | 実行ファイルディレクトリ、カレントディレクトリ、親ディレクトリから `APPLaunch/share` を推定 |
| C interface | `cp0_file_path_c()` | thread-local にキャッシュされた `const char *` を返し、LVGL style API に適する |
| C++ interface | `cp0_file_path()` | `std::string` を返す。ページ内で推奨 |

## 6. Settings と永続化入口表

| Setting item | UI entry | Configuration key | Implementation location |
| --- | --- | --- | --- |
| App visibility toggle | SETTING -> Launcher | `AppDescriptor.config_key` | `ui_app_setup.hpp` の `save_app_toggle()`、`launch.cpp` の `launcher_app_registry_is_enabled()` |
| Brightness | SETTING -> Screen -> Brightness | `brightness` | `ui_app_setup.hpp`, `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_settings.cpp` |
| Screen-off timeout | SETTING -> Screen -> DarkTime | `dark_time` | `ui_app_setup.hpp` |
| Volume | SETTING -> Speaker -> Volume | `volume` | `ui_app_setup.hpp`, `cp0_volume_read/write()` |
| Camera resolution | SETTING -> Camera -> Resolution | `cam_resolution` | `ui_app_setup.hpp`。camera page が読み取る |
| Startup mode | Settings page の関連選択 | `startup_mode` | `ui_app_setup.hpp` |
| USB extension port | SETTING -> ExtPort | `extport_usb` | `ui_app_setup.hpp` |
| 5V output | SETTING -> ExtPort | `extport_5vout` | `ui_app_setup.hpp` |
| External app runtime user | 手動設定 | `run_as_user` | `cp0_lvgl_process.cpp`, `cp0_lvgl_pty.cpp` |

設定実装:

| File | Description |
| --- | --- |
| `ext_components/cp0_lvgl/include/cp0_lvgl_app.h` | `cp0_config_get_int/set_int/get_str/set_str/save` の宣言 |
| `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_config.cpp` | デバイス側設定 read/write。`/var/lib/applaunch/settings` に保存 |
| `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_config.cpp` | SDL2 互換実装 |
| `ext_components/cp0_lvgl/src/commount.c` | 起動時に保存済み輝度と音量を適用 |

## 7. ビルド入口表

| Scenario | Command/file | Description |
| --- | --- | --- |
| Default SDL2 build | `projects/APPLaunch/SConstruct` が自動的に `linux_x86_sdl2_config_defaults.mk` を選択 | x86_64 開発ホストの既定設定 |
| Explicit SDL2 build | `CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j8 --implicit-deps-changed` | ローカル開発確認に推奨 |
| Cross build | `CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8 --implicit-deps-changed` | x86 Linux から AArch64 デバイスへ |
| Native device build | `CONFIG_DEFAULT_FILE=config_defaults.mk scons -j4 --implicit-deps-changed` | デバイス側 framebuffer/evdev 設定 |
| macOS cross build | `CONFIG_DEFAULT_FILE=mac_cross_cp0_config_defaults.mk scons ...` | macOS からデバイスへ |
| macOS/Darwin | `darwin_config_defaults.mk` | Darwin/SDL 関連設定 |
| Main build script | `projects/APPLaunch/SConstruct` | SDK、EXT_COMPONENTS、sysroot download を設定 |
| Component build script | `projects/APPLaunch/main/SConstruct` | sources、dependencies、static files、git commit macro |
| APPLaunch configuration | `projects/APPLaunch/main/Kconfig` | メインプロジェクト Kconfig |
| cp0_lvgl configuration | `ext_components/cp0_lvgl/Kconfig` | プラットフォーム適応コンポーネント設定 |

## 8. プラットフォーム適応入口表

| Capability | Header | Device implementation | SDL2/compat implementation |
| --- | --- | --- | --- |
| LVGL initialization | `ext_components/cp0_lvgl/include/hal_lvgl_bsp.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl.c` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl.c` |
| framebuffer/display | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_freambuffer.c` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_display.c` |
| Keyboard input | `ext_components/cp0_lvgl/include/keyboard_input.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_keyboard.c` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_keyboard.c` |
| File paths | `ext_components/cp0_lvgl/include/cp0_lvgl_file.hpp` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_file.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_file.cpp` |
| Process | `ext_components/cp0_lvgl/include/hal/hal_process.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_process.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_process.cpp` |
| PTY | `ext_components/cp0_lvgl/include/hal/hal_pty.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_pty.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_pty.cpp` |
| Audio | `ext_components/cp0_lvgl/include/hal/hal_audio.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_audio.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_audio.cpp` |
| Settings/brightness/volume | `ext_components/cp0_lvgl/include/hal/hal_settings.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_settings.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_settings.cpp` |
| Network/WiFi | `ext_components/cp0_lvgl/include/hal/hal_network.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_network.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_network.cpp` |
| Screenshot | `ext_components/cp0_lvgl/include/hal/hal_screenshot.h` | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_screenshot.cpp` | `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_screenshot.cpp` |
| Camera | `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_camera.cpp` | Device camera | `ext_components/cp0_lvgl/src/sdl/cp0_lvgl_camera.cpp` |

## 9. デバッグコマンド早見表

| Purpose | Command |
| --- | --- |
| 現在の変更を見る | `git status --short` |
| SDL2 build | `cd projects/APPLaunch && CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j8 --implicit-deps-changed` |
| SDL2 run | `cd projects/APPLaunch && ./dist/M5CardputerZero-APPLaunch` |
| Cross build | `cd projects/APPLaunch && CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8 --implicit-deps-changed` |
| systemd status を見る | `systemctl --user status APPLaunch.service --no-pager` |
| ログを追跡 | `journalctl --user -u APPLaunch.service -f` |
| boot logs を見る | `journalctl --user -u APPLaunch.service -b --no-pager` |
| アセット確認 | `find /usr/share/APPLaunch -maxdepth 3 -type f | sort` |
| `.desktop` ファイル確認 | `find /usr/share/APPLaunch/applications -maxdepth 1 -type f -name '*.desktop' -print -exec sed -n '1,80p' {} \;` |
| settings 確認 | `sudo cat /var/lib/applaunch/settings` |
| 入力デバイス確認 | `ls -l /dev/input/by-path/ && sudo evtest` |
| 外部アプリプロセス確認 | `ps -eo pid,ppid,pgid,stat,cmd | grep -E 'APPLaunch|sh -c|M5CardputerZero'` |
| 動的ライブラリ確認 | `ldd /usr/share/APPLaunch/bin/my_app` |
| アイコンログ確認 | `journalctl --user -u APPLaunch.service -b --no-pager | grep 'set panel icon'` |

## 10. 変更前後チェックリスト

| Stage | Check item |
| --- | --- |
| Before change | `git status --short` を実行し、他者の既存変更がどのファイルにあるか確認 |
| After adding a page | `.hpp` ファイルが `page_app/` にあり、クラス名が `launch.cpp` の登録と一致することを確認 |
| After adding assets | ファイルがソースツリーとデバイス `/usr/share/APPLaunch` の両方で見つかることを確認 |
| After adding `.desktop` | ファイルサフィックスが `.desktop` で、`[Desktop Entry]`、`Name`、`Exec` を含む |
| After changing settings | `/var/lib/applaunch/settings` に正しいキーが含まれ、設定エントリ上限を超えていない |
| After build | SDL2 またはクロスビルドが成功し、予期しない自動生成 diff がない |
| After running on device | `journalctl` に missing、skip、segfault、permission denied メッセージがない |
| After external app changes | アプリが正常終了する、または長押し ESC でホームへ戻る |
