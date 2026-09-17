# cp0_lvgl Interface Reference

Languages: [中文](cp0_lvgl.md) | English (this page) | [日本語](cp0_lvgl.ja.md)

This document is the public contract for `ext_components/cp0_lvgl`. New pages and extension modules can use it as their first reference. When fixing a bug, changing behavior, or checking a platform difference, inspect the actual implementation under `src/` and the matching `*_contract.*` files. Device and SDL backends share signal names and argument formats, but their underlying capabilities can differ.

## Quick Start

Important headers:

- `include/hal_lvgl_bsp.h`: C++ signal declarations and `cp0_file_path`.
- `include/signal_register_plan.h`: the complete `cp0_signal_*` signature list.
- `include/cp0_lvgl_app.h`: C ABI for filesystem, network, process, battery, backlight, time, and sudo.
- `include/cp0_lvgl_app_runner.hpp`: LVGL runner.
- `include/ui_app_page.hpp`: page bases and page startup helpers.

Signals are global eventpp `CallbackList` objects. Request signals normally take `std::list<std::string>`: element zero is the command and the final argument is usually `std::function<void(int, std::string)>`. A callback receives `(code, data)`; `code == 0` is success and a negative code is an error. `data` may be empty, text, line records, or binary struct bytes. Do not assume the callback runs on the calling thread, and do not let callback exceptions escape.

Call `cp0_lvgl_init()` before using services; `cp0_lvgl_run()` does this automatically. Kconfig options named `CONFIG_CP0_LVGL_INIT_*` select services and `cp0_init_plan` controls their order. A disabled service has no usable implementation.

```cpp
cp0_signal_wifi_api({"Status"}, [](int code, std::string payload) {
    if (code != 0) return;
    // payload: connected:ssid:ip:signal:ethernet
});
```

Prefer the C wrappers in `cp0_lvgl_app.h` when one exists; they validate arguments and decode wire data.

## LVGL and Page API

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

The runner supports one successful run per process. Its order is `lv_init`, `after_lvgl_init`, `cp0_lvgl_init`, `after_resource_init`, `setup`, the LVGL timer loop, `teardown`, and service cleanup. A false `setup`, an exception, or a missing display returns 1. `should_quit` ends the loop; `cp0_lvgl_wake()` wakes a loop waiting for the next LVGL timer.

`AppPageRoot` owns an LVGL screen and input group. `AppPage` adds the standard content container `ui_APP_Container` and top bar. `AppPageWithBottomBarLayout` adds `ui_BOTTOM_Container`. Use `cp0_lvgl_start_app_page(AppPageRoot&)` for an existing page or `cp0_lvgl_start_app<PageT>(args...)` to construct and load one. Create and destroy page objects on the LVGL thread. The standard top bar is 20 px high.

## Public C ABI

Declarations are in `include/cp0_lvgl_app.h` and `include/hal_lvgl_bsp.h`. Unless noted otherwise, 0 means success and a negative value means failure.

### Filesystem and paths

```c
void cp0_lvgl_init(void);
const char *cp0_file_path_c(const char *file);
int cp0_dir_list(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count);
cp0_watcher_t cp0_dir_watch_start(const char *path);
int cp0_dir_watch_poll(cp0_watcher_t watcher);
void cp0_dir_watch_stop(cp0_watcher_t watcher);
int cp0_file_read_first_line(const char *path, char *out, int out_size);
```

`cp0_file_path_c` resolves logical names such as `applications`, `appstore_exec`, `calculator_exec`, `adb_helper`, `launcher_settings`, `oobe_marker`, `home_dir`, `lock_file`, `keyboard_device`, `keyboard_map`, and `share/images/`, `share/audio/`, or font resources. The returned C string belongs to a thread-local cache; copy it if it must outlive the call. Watch handles are nonzero module-owned values and may only be passed to the matching start/poll/stop functions.

### Network and Wi-Fi

```c
int cp0_network_list(cp0_netif_info_t *entries, int max_entries, int *out_count);
int cp0_wifi_status_read(cp0_wifi_status_t *status);
int cp0_wifi_scan(cp0_wifi_ap_t *entries, int max_entries);
int cp0_wifi_connect(const char *ssid, const char *password);
int cp0_wifi_connect_hidden(const char *ssid, const char *password);
int cp0_wifi_profile_forget(const char *ssid);
int cp0_wifi_profile_exists(const char *ssid);
int cp0_wifi_disconnect_active(void);
int cp0_wifi_radio_enabled(void);
int cp0_wifi_radio_set_enabled(int enabled);
```

`cp0_wifi_scan` returns an AP count or `CP0_WIFI_ERROR_INVALID=-1`, `RADIO_OFF=-2`, `AUTH=-3`, `NOT_FOUND=-4`, `IP_CONFIG=-5`, `SERVICE=-6`, or `TIMEOUT=-7`. `signal` is normally RSSI dBm; legacy backends may use 0..100.

### Process and system

`cp0_process_exec_blocking`, `cp0_process_spawn`, `cp0_process_stop`, `cp0_process_check_lock`, `cp0_process_kill`, `cp0_system_shutdown`, `cp0_system_reboot`, `cp0_process_run_argv`, and `cp0_process_capture_argv` are declared in `cp0_lvgl_app.h`. `keep_root` and `background` are 0/1. `grace_ms` is limited to 300000. `cp0_process_run_sudo` is deprecated; use asynchronous sudo. Check an executable with `cp0_desktop_exec_is_safe` before launching it.

### Sudo C API

`cp0_sudo_run_argv_async` and `cp0_sudo_run_shell_async` accept a callback-thread selection, output callback, completion callback, and user pointer. `cp0_sudo_run_argv_async_ex` additionally accepts `auth_timeout_ms`, `exec_timeout_ms`, and an output `request_id`. `cp0_sudo_cancel(request_id)` is idempotent while the id is retained; unknown or expired ids return `-ENOENT`. `cp0_sudo_queue_password` queues a short-lived password for the LVGL prompt.

`CP0_SUDO_CALLBACK_LVGL` delivers output and completion on the LVGL thread; `CP0_SUDO_CALLBACK_WORKER` calls them on a worker. Output is ordered and LVGL delivery has bounded backpressure, so callbacks must return quickly. Completion results are `CP0_SUDO_RESULT_SUCCESS`, `AUTH_FAILED`, `EXEC_FAILED`, `CANCELLED`, or `TIMED_OUT`, together with the process exit code.

### Battery, backlight, and time

`cp0_battery_read` reads the latest background snapshot and performs no hardware I/O; use it only when `valid == 1`. Battery units are mV, mA, 0.1 C, mAh, and percent SOC. `cp0_bq27220_calibrate` accepts indices 0..3. `cp0_backlight_write` clamps to 0..max. `cp0_time_set` requires `YYYY-MM-DD HH:MM:SS`; manual RTC persistence requires NTP to be disabled. `cp0_time_ntp_get/set` controls NTP.

Other C functions read Ethernet/default network information, account information, time text, desktop execution safety, start background updates, and request/clear LoRa initialization stop.

## Complete `cp0_signal_*` Usage

All `args` below are `std::list<std::string>` with the command at element zero. Request callbacks use `void(int code, std::string data)` unless a different signature is shown.

### Audio

- `cp0_signal_audio_play(std::string path)`: one-way playback of a path.
- `cp0_signal_audio_cap(bool enable)`: one-way start/stop capture.
- `cp0_signal_system_play(std::string name)`: play a system sound name. Register extra names first with `RegisterSystemSounds`.
- `cp0_signal_audio_setup`: `{"set_callback"}`, `{"set_waveform"}` or `{"set_waveform","0"}`, and `{"stop_play"}` set status callbacks, waveform reporting, and stop playback.
- `cp0_signal_audio_api`: `PlayFile path`, `Play asset`, `PlayPause`, `PlayContinue`, `PlayEnd`, `Cap`, `CapPause`, `CapContinue`, `CapEnd`, `CapFileSave path`, `SetCallback`, `VolumeRead`, `VolumeWrite 0..100`, `MuteRead`, `MuteToggle`, `SetSystemSoundNames [up to 3 names]`, `RegisterSystemSounds [1..32 names]`, `SystemSoundPlay 0..2`, `SystemSoundSuspend`, `SystemSoundPrepare`, and `SystemSoundEnable [0|1/on/off...]`. `Play` resolves a separator-free name as a resource; `PlayFile` uses the supplied path. Errors are negative codes with text.

### PTY

`cp0_signal_pty_api` commands are `Open executable [columns] [rows] [argv...]` (default 80x24, returns a numeric handle), `Read handle [max_read]` (default 4096, max 1 MiB; code is byte count), `Write handle data` (code is byte count), `Resize handle columns rows`, `CheckChild handle` (code 0 running, 1 exited; data is exit code), and `Close handle`. Handles are not PIDs. Linux children use `TERM=vt100` and may drop privileges according to `run_as_user`; teardown kills remaining children.

### Configuration

`cp0_signal_config_api` uses `$HOME/.config/cardputerzero/config.json`, up to 64 entries, 63-byte keys, and 255-byte values. Commands are `Init`, `Save`, `GetInt key fallback`, `SetInt key value`, `GetStr key fallback`, `SetStr key value`, and `SetManyAndSave key value [key value...]`. Set operations change memory; call `Save` to persist, or use the transaction command for atomic update-and-save.

### Filesystem signal: `cp0_signal_filesystem_api(args, cb)`

Commands are `Path logical_name`, `DirList path`, `DirListDetail path`, `Exists path`, `ReadFile path [max_bytes]`, `EnsureDirForUser path`, `Touch path`, `Remove path`, `WatchStart path`, `WatchPoll handle`, and `WatchStop handle`. `Exists` returns `1/0`; `WatchStart` returns a handle; directory data is line-encoded. Empty or NUL-containing paths are invalid. Unknown commands return code -2.

### LoRa: `cp0_signal_lora_api(args, cb)`

Commands are `Init`, `Poll`, `Info`, `SendText text`, `StartReceive`, `SetTxMode 0|1`, and `Shutdown`. Text must be nonempty and at most 127 bytes. `Poll` and `Info` return raw `cp0_lora_info_t` bytes; `Poll` performs hardware polling while `Info` only reads state. `cp0_lora_request_stop` and `cp0_lora_clear_stop` control initialization cancellation.

### Wi-Fi

Commands are `Status`, `Scan [0..32]`, `Connect ssid [password]`, `ConnectHidden ssid [password]`, `Disconnect`, `ProfileForget ssid`, `ProfileExists ssid`, `ProfileDisconnectActive`, `RadioEnabled`, and `RadioSetEnabled on|off|1|0|true|false`. Status data is `connected:ssid:ip:signal:ethernet`. Scan data has one `ssid:signal:security:in_use:saved` record per line, with backslash, colon, LF, and CR escaped. Prefer the C wrappers for decoding.

### Bluetooth and Agent

`cp0_signal_bt_api` supports `BtStatus`, `BtList [max=16]`, `BtConnectedList [max=16]`, `BtScan [max=16]`, `BtReset`, `BtPower 0|1`, `BtAlias name`, `BtDiscoverable 0|1`, `BtDiscoveryStart`, `BtDiscoveryStop`, `BtPair address`, `BtCancelPairing address`, `BtConnect address`, `BtDisconnect address`, and `BtRemove address`. Addresses must be `XX:XX:XX:XX:XX:XX`. Status data is `powered<TAB>address<TAB>discoverable<TAB>alias`; device lines are `address<TAB>rssi<TAB>connected<TAB>paired<TAB>trusted<TAB>name`.

Session commands are `BtSessionInit` (returns an id), `BtSessionDeinit id`, `BtStatusGet id`, `BtConnectedListInit id`, `BtConnectedListGet id`, `BtConnectedListDeinit id`, `BtScanOn id`, and `BtScanOff id`. `BtScanOn` reports from a worker about every three seconds; stop it before destroying the session. `BtScan`, pairing, and connection operations can complete asynchronously.

`cp0_signal_bt_agent` has signature `(uint64_t id, std::string method, std::string device, std::string value, std::function<void(bool accepted, std::string text)> reply)`. It originates on a BlueZ worker thread. Marshal UI work to the LVGL/event loop, then call `reply` exactly once.

### Settings

`cp0_signal_settings_api` commands are `BacklightRead`, `BacklightMax`, `BacklightWrite value`, `Log topic message`, `TimeStr`, `GpioSet name 0|1`, and `GpioGet name`. GPIO names are `GROVE5V`/`extport_usb`, `EXT5V`/`extport_5vout`, and `BACKLIGHT`. Integer results are returned as decimal text; `TimeStr` is `HH:MM`.

### Process, OS information, and time

`cp0_signal_process_api` commands are `ExecBlocking executable keep_root`, `Spawn executable keep_root`, `Stop pid`, `CheckLock path`, `Kill pid grace_ms`, `RunArgv background argv...`, `RunSudo password argv...` (legacy only), `CaptureArgv argv...`, `AdbStatus`, `DesktopExecIsSafe executable`, `Shutdown`, `Reboot`, and `DelayMs milliseconds`. `Spawn` returns a PID; `CaptureArgv` returns stdout; `AdbStatus` returns two status lines.

`cp0_signal_osinfo_api` and `cp0_signal_timedate_api` share one service. Commands are `NetworkDefaultInfoRead`, `EthInfoRead` (three lines: IPv4, gateway, MAC), `NetworkList` (tab-separated interface records), `AccountInfoRead` (user and hostname), `TimeSet timestamp`, `LocalTime` (`year,month,day,hour,minute,second`), `RandomU32`, `NtpGet`, `NtpSet 0|1`, `AptUpdateBackground`, and `UpdateLauncherBackground`.

### Battery, screenshot, camera, and soundcard

- `cp0_signal_bq27220_api`: `Read` returns encoded `cp0_battery_info_t`; `Calibrate index` accepts 0..3.
- `cp0_signal_screenshot_api`: `Save directory` writes a BMP and returns its complete path.
- `cp0_signal_battery_pub(std::function<void()>)`: triggers one battery event publication to the active LVGL screen. The function argument is retained for signature compatibility, not subscription registration; call on the LVGL thread.
- `cp0_signal_camera_api`: `Open [width] [height]`, `Start`, `Close`, `Stop`, `Capture [path] [width] [height]`, `Photo`, `Status`, `SetCallback`, `SetFrameCallback`, `ZoomIn`, `ZoomOut`, and `Pan dx dy`. Defaults are 320x150. Status code 0 means streaming and 1 means stopped; unsupported libcamera returns -10. Frame callbacks may run off-thread and must be marshalled to LVGL.
- `cp0_signal_soundcard_api`: `ListCards`, `ListControls card_index`, `GetControlDetail card_index control`, and `SetControl card_index control value`. Values preserve raw `amixer` text; unavailable ALSA/process support returns `not supported` or an error.

### Sudo signals

`cp0_signal_sudo_argv_async` takes `(argv, auth_timeout_ms, exec_timeout_ms, complete, started)`. `started(ret, request_id)` arrives first; on success `ret == 0` and the id is nonzero. `complete(result_code, exit_code)` then arrives once. Cancel with `cp0_signal_sudo_cancel(request_id, done)`; `done(0)` means cancellation was accepted.

`cp0_signal_system_admin_async` has the same signature but only accepts `AdbSet 0|1`, `AdbAuthorize public_key` (680..2048 bytes, no CR/LF), `AdbRevoke fingerprint` (64 lowercase hexadecimal characters), `AdbClearAuthorizations`, `NtpSet 0|1`, and `TimeSet YYYY-MM-DD HH:MM:SS`. Invalid arguments call only `started(-EINVAL, 0)` and do not produce a completion callback. Keep request ids and cancel them when a UI workflow leaves or times out.

`cp0_signal_network()` and `cp0_signal_forkexec()` are reserved plan declarations. The current `cp0_lvgl` sources do not register them, so confirm an implementation before calling.

## Change and Debug Rules

1. Read this document and `include/signal_register_plan.h` before changing a call site.
2. When changing an interface, update all three language documents, the matching contract, C wrapper, call sites, and tests.
3. For bug fixes, inspect the implementation and contract files; verify callback threads, initialization, teardown, background workers, and wire escaping.
4. Run `ext_components/cp0_lvgl/tests/run_tests.sh`. Changes involving Linux commands, BlueZ, libcamera, framebuffer, or sudo also require target-platform validation.
