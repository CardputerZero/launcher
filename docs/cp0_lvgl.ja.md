# cp0_lvgl インターフェースリファレンス

言語: [中文](cp0_lvgl.md) | [English](cp0_lvgl.en.md) | 日本語（このページ）

この文書は `ext_components/cp0_lvgl` の公開インターフェース契約です。新しいページや拡張モジュールは最初にここを参照できます。バグ修正、動作変更、プラットフォーム差異の確認では、必ず `src/` の実装と対応する `*_contract.*` を確認してください。デバイス版と SDL 版は信号名と引数形式を共有しますが、利用できる低レベル機能は異なる場合があります。

## クイックスタート

主なヘッダー:

- `include/hal_lvgl_bsp.h`: C++ 信号宣言と `cp0_file_path`。
- `include/signal_register_plan.h`: 全 `cp0_signal_*` のシグネチャ。
- `include/cp0_lvgl_app.h`: ファイル、ネットワーク、プロセス、バッテリー、バックライト、時刻、sudo の C ABI。
- `include/cp0_lvgl_app_runner.hpp`: LVGL runner。
- `include/ui_app_page.hpp`: ページ基底クラスと起動ヘルパー。

信号はグローバルな eventpp `CallbackList` です。リクエスト型は通常 `std::list<std::string>` を受け取り、0 番目がコマンド、最後が `std::function<void(int, std::string)>` コールバックです。コールバックの `(code, data)` で `code == 0` は成功、負値はエラーです。`data` は空、テキスト、行レコード、または構造体のバイナリです。コールバックが呼び出し元と同じスレッドで実行されるとは限らず、例外を外へ出してはいけません。

サービスを使う前に `cp0_lvgl_init()` を呼びます。`cp0_lvgl_run()` は自動で呼び出します。サービスの有効化は `CONFIG_CP0_LVGL_INIT_*`、初期化順序は `cp0_init_plan` で決まります。無効なサービスには実装がありません。

```cpp
cp0_signal_wifi_api({"Status"}, [](int code, std::string payload) {
    if (code != 0) return;
    // payload: connected:ssid:ip:signal:ethernet
});
```

対応する場合は `cp0_lvgl_app.h` の C ラッパーを優先してください。引数検証と wire データのデコードを行います。

## LVGL とページ API

```cpp
struct Cp0LvglRunOptions {
    std::function<void()> after_lvgl_init;
    std::function<void()> after_resource_init;
    std::function<bool()> setup;
    std::function<bool()> should_quit;
    std::function<void()> teardown;
};
int cp0_lvgl_run(Cp0LvglRunOptions options = {});
void cp0_lvgl_wake();
```

runner は 1 プロセスにつき成功する実行を 1 回だけ許可します。順序は `lv_init`、`after_lvgl_init`、`cp0_lvgl_init`、`after_resource_init`、`setup`、LVGL timer loop、`teardown`、サービス解放です。`setup` が false、例外、または display 作成失敗の場合は 1 を返します。`should_quit` が true になると終了し、`cp0_lvgl_wake()` は次の timer を待つ loop を起こします。

`AppPageRoot` は LVGL screen と input group を所有します。`AppPage` は `ui_APP_Container` と top bar を追加し、`AppPageWithBottomBarLayout` は `ui_BOTTOM_Container` を追加します。既存ページには `cp0_lvgl_start_app_page(AppPageRoot&)`、構築とロードには `cp0_lvgl_start_app<PageT>(args...)` を使います。ページの生成と破棄は LVGL スレッドで行い、root screen を手動で削除しないでください。標準 top bar の高さは 20 px です。

## 公開 C ABI

宣言は `include/cp0_lvgl_app.h` と `include/hal_lvgl_bsp.h` にあります。特記がない限り 0 は成功、負値は失敗です。

### ファイルとパス

```c
void cp0_lvgl_init(void);
const char *cp0_file_path_c(const char *file);
int cp0_dir_list(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count);
cp0_watcher_t cp0_dir_watch_start(const char *path);
int cp0_dir_watch_poll(cp0_watcher_t watcher);
void cp0_dir_watch_stop(cp0_watcher_t watcher);
int cp0_file_read_first_line(const char *path, char *out, int out_size);
```

`cp0_file_path_c` は `applications`、`appstore_exec`、`calculator_exec`、`adb_helper`、`launcher_settings`、`oobe_marker`、`home_dir`、`lock_file`、`keyboard_device`、`keyboard_map`、`share/images/`、`share/audio/`、フォントなどの論理名を解決します。返される C 文字列はスレッドローカルキャッシュに属するため、長く保持する場合はコピーしてください。watcher handle はモジュール所有の非ゼロ値で、対応する start/poll/stop の組だけで使用します。

### ネットワークと Wi-Fi

`cp0_network_list`、`cp0_wifi_status_read`、`cp0_wifi_scan`、`cp0_wifi_connect`、`cp0_wifi_connect_hidden`、`cp0_wifi_profile_forget`、`cp0_wifi_profile_exists`、`cp0_wifi_disconnect_active`、`cp0_wifi_radio_enabled`、`cp0_wifi_radio_set_enabled` を提供します。scan は AP 数、または `CP0_WIFI_ERROR_INVALID=-1`、`RADIO_OFF=-2`、`AUTH=-3`、`NOT_FOUND=-4`、`IP_CONFIG=-5`、`SERVICE=-6`、`TIMEOUT=-7` を返します。`signal` は通常 RSSI dBm ですが、旧 backend は 0..100 の場合があります。

### プロセス、システム、sudo

`cp0_process_exec_blocking`、`cp0_process_spawn`、`cp0_process_stop`、`cp0_process_check_lock`、`cp0_process_kill`、shutdown/reboot、argv 実行・捕捉 API を提供します。`keep_root` と `background` は 0/1、`grace_ms` の上限は 300000 です。`cp0_process_run_sudo` は deprecated なので使用せず、非同期 sudo を使います。外部実行前に `cp0_desktop_exec_is_safe` を確認してください。

`cp0_sudo_run_argv_async`、`cp0_sudo_run_shell_async`、拡張版 `cp0_sudo_run_argv_async_ex` は output/completion callback と callback thread を受け取ります。`CP0_SUDO_CALLBACK_LVGL` は LVGL スレッド、`CP0_SUDO_CALLBACK_WORKER` は worker スレッドで実行します。出力は順序保証されますが LVGL 配信には上限付き backpressure があるため、callback はすぐ戻してください。完了結果は `SUCCESS`、`AUTH_FAILED`、`EXEC_FAILED`、`CANCELLED`、`TIMED_OUT` です。request id は `cp0_sudo_cancel` でキャンセルできます。未知または期限切れ id は `-ENOENT` です。

### バッテリー、バックライト、時刻

`cp0_battery_read` は最新のバックグラウンド snapshot のみを読み、ハードウェア I/O は行いません。`valid == 1` の場合だけ利用します。`cp0_bq27220_calibrate` の index は 0..3、バックライト書き込みは 0..max に clamp されます。`cp0_time_set` の形式は `YYYY-MM-DD HH:MM:SS` で、RTC を永続化するには NTP を無効にします。Ethernet/account 情報、時刻文字列、背景更新、LoRa 初期化停止 API も提供します。

## `cp0_signal_*` 完全な使い方

以下の `args` はすべて `std::list<std::string>` で、0 番目がコマンドです。特記しない request callback は `void(int code, std::string data)` です。

### Audio

- `cp0_signal_audio_play(std::string path)`: パスを再生する一方向信号。
- `cp0_signal_audio_cap(bool enable)`: 録音の開始/停止。
- `cp0_signal_system_play(std::string name)`: システム音を名前で再生。追加名は `RegisterSystemSounds` で登録します。
- `cp0_signal_audio_setup`: `{"set_callback"}`、`{"set_waveform"}` または `{"set_waveform","0"}`、`{"stop_play"}`。
- `cp0_signal_audio_api`: `PlayFile path`、`Play asset`、`PlayPause`、`PlayContinue`、`PlayEnd`、`Cap`、`CapPause`、`CapContinue`、`CapEnd`、`CapFileSave path`、`SetCallback`、`VolumeRead`、`VolumeWrite 0..100`、`MuteRead`、`MuteToggle`、`SetSystemSoundNames [最大3名]`、`RegisterSystemSounds [1..32名]`、`SystemSoundPlay 0..2`、`SystemSoundSuspend`、`SystemSoundPrepare`、`SystemSoundEnable [0/1 または on/off]`。`Play` は区切り文字のない名前を resource として解決し、`PlayFile` は指定パスを使います。

### PTY

`cp0_signal_pty_api` は `Open executable [columns] [rows] [argv...]`（既定 80x24、数値 handle を返す）、`Read handle [max_read]`（既定 4096、最大 1 MiB、code は byte 数）、`Write handle data`、`Resize handle columns rows`、`CheckChild handle`（code 0 は実行中、1 は終了、data は終了コード）、`Close handle` を提供します。handle は PID ではありません。Linux の子プロセスは `TERM=vt100` を設定し、`run_as_user` により権限を下げる場合があります。

### 設定とファイルシステム

`cp0_signal_config_api` のファイルは `$HOME/.config/cardputerzero/config.json` です。コマンドは `Init`、`Save`、`GetInt key fallback`、`SetInt key value`、`GetStr key fallback`、`SetStr key value`、`SetManyAndSave key value [key value...]`。set はメモリだけを変更するため、永続化には `Save` を呼びます。

`cp0_signal_filesystem_api` は `Path logical_name`、`DirList path`、`DirListDetail path`、`Exists path`、`ReadFile path [max_bytes]`、`EnsureDirForUser path`、`Touch path`、`Remove path`、`WatchStart path`、`WatchPoll handle`、`WatchStop handle` を提供します。`Exists` は `1/0`、空または NUL を含むパスは無効、未知コマンドは code -2 です。

### LoRa: `cp0_signal_lora_api(args, cb)` と Wi-Fi: `cp0_signal_wifi_api(args, cb)`

LoRa のコマンドは `Init`、`Poll`、`Info`、`SendText text`、`StartReceive`、`SetTxMode 0|1`、`Shutdown`。text は空でなく最大 127 byte、`Poll`/`Info` の data は raw `cp0_lora_info_t` です。`Poll` は hardware poll を実行し、`Info` は状態だけを読みます。

Wi-Fi のコマンドは `Status`、`Scan [0..32]`、`Connect ssid [password]`、`ConnectHidden ssid [password]`、`Disconnect`、`ProfileForget ssid`、`ProfileExists ssid`、`ProfileDisconnectActive`、`RadioEnabled`、`RadioSetEnabled on|off|1|0|true|false`。Status は `connected:ssid:ip:signal:ethernet`、Scan は 1 行ごとに `ssid:signal:security:in_use:saved` です。backslash、colon、LF、CR は escape されます。

### Bluetooth と Agent

`cp0_signal_bt_api` は `BtStatus`、`BtList [max=16]`、`BtConnectedList [max=16]`、`BtScan [max=16]`、`BtReset`、`BtPower 0|1`、`BtAlias name`、`BtDiscoverable 0|1`、`BtDiscoveryStart`、`BtDiscoveryStop`、`BtPair address`、`BtCancelPairing address`、`BtConnect address`、`BtDisconnect address`、`BtRemove address` をサポートします。address は `XX:XX:XX:XX:XX:XX`。session API は `BtSessionInit`、`BtSessionDeinit id`、`BtStatusGet id`、`BtConnectedListInit/Get/Deinit id`、`BtScanOn id`、`BtScanOff id` です。`BtScanOn` は約 3 秒ごとに worker から報告するため、終了前に停止してください。

`cp0_signal_bt_agent` のシグネチャは `(uint64_t id, std::string method, std::string device, std::string value, std::function<void(bool accepted, std::string text)> reply)`。BlueZ worker スレッドから来るので、UI 処理を LVGL スレッドへ marshal してから reply を 1 回だけ呼びます。

### Settings: `cp0_signal_settings_api(args, cb)`、process: `cp0_signal_process_api(args, cb)`、OS 情報: `cp0_signal_osinfo_api(args, cb)` / `cp0_signal_timedate_api(args, cb)`

Settings のコマンドは `BacklightRead`、`BacklightMax`、`BacklightWrite value`、`Log topic message`、`TimeStr`、`GpioSet name 0|1`、`GpioGet name`。GPIO 名は `GROVE5V`/`extport_usb`、`EXT5V`/`extport_5vout`、`BACKLIGHT` です。

Process のコマンドは `ExecBlocking executable keep_root`、`Spawn executable keep_root`、`Stop pid`、`CheckLock path`、`Kill pid grace_ms`、`RunArgv background argv...`、legacy の `RunSudo`、`CaptureArgv argv...`、`AdbStatus`、`DesktopExecIsSafe executable`、`Shutdown`、`Reboot`、`DelayMs milliseconds`。OS 情報と timedate は同じ service で、network/account 情報、`TimeSet`、`LocalTime`、`RandomU32`、`NtpGet`、`NtpSet`、背景更新を処理します。

### Battery、screenshot、camera、soundcard

- `cp0_signal_bq27220_api`: `Read` は encoded `cp0_battery_info_t`、`Calibrate index` は 0..3。
- `cp0_signal_screenshot_api`: `Save directory` は BMP を保存し、完全パスを返します。
- `cp0_signal_battery_pub(std::function<void()>)`: active LVGL screen に一度 battery event を発行します。引数は互換性のためで、subscription 登録ではありません。LVGL スレッドで呼びます。
- `cp0_signal_camera_api`: `Open [width] [height]`、`Start`、`Close`、`Stop`、`Capture`、`Photo`、`Status`、`SetCallback`、`SetFrameCallback`、`ZoomIn`、`ZoomOut`、`Pan dx dy`。既定は 320x150、libcamera がない場合は -10。frame callback は別スレッドの場合があるため LVGL へ marshal します。
- `cp0_signal_soundcard_api`: `ListCards`、`ListControls card_index`、`GetControlDetail card_index control`、`SetControl card_index control value`。値は raw `amixer` テキストです。

### Sudo signal

`cp0_signal_sudo_argv_async` は `(argv, auth_timeout_ms, exec_timeout_ms, complete, started)` を受け取ります。先に `started(ret, request_id)`、その後に `complete(result_code, exit_code)` が 1 回呼ばれます。`cp0_signal_sudo_cancel(request_id, done)` でキャンセルし、`done(0)` は受理を意味します。

`cp0_signal_system_admin_async` は `AdbSet 0|1`、`AdbAuthorize public_key`（680..2048 byte、改行なし）、`AdbRevoke fingerprint`（64 桁の小文字 hex）、`AdbClearAuthorizations`、`NtpSet 0|1`、`TimeSet YYYY-MM-DD HH:MM:SS` のみを受け付けます。不正引数では `started(-EINVAL, 0)` だけが呼ばれ、complete は呼ばれません。画面を離れる時や timeout 時には request id をキャンセルしてください。

`cp0_signal_network()` と `cp0_signal_forkexec()` は plan に残る予約宣言ですが、現在の `cp0_lvgl` には登録実装がありません。呼び出す前に実装の存在を確認してください。

## 変更とデバッグのルール

1. call site を変更する前に本書と `include/signal_register_plan.h` を読みます。
2. インターフェースを変更したら、英語・中国語・日本語の 3 文書、対応 contract、C wrapper、call site、テストを更新します。
3. バグ修正では実装と contract を確認し、callback thread、初期化、teardown、worker、wire escape を検証します。
4. `ext_components/cp0_lvgl/tests/run_tests.sh` を実行します。Linux command、BlueZ、libcamera、framebuffer、sudo に関わる変更は対象デバイスでも検証します。
