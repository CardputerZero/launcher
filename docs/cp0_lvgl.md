# cp0_lvgl 接口说明

语言：中文（当前页） | [English](cp0_lvgl.en.md) | [日本語](cp0_lvgl.ja.md)

本文是 `ext_components/cp0_lvgl` 的对外接口契约。新增页面或扩展模块可以先按本文调用；定位 bug、修改行为、确认线程和平台差异时，必须继续阅读本目录 `src/` 中的实际实现。设备端和 SDL 端共享信号名字及参数格式，但底层能力可能不同。

## 1. 快速接入

### 头文件

- `include/hal_lvgl_bsp.h`：C++ 信号声明、`cp0_file_path`。
- `include/signal_register_plan.h`：全部 `cp0_signal_*` 的签名清单。
- `include/cp0_lvgl_app.h`：C ABI 的文件系统、网络、进程、电池、背光、时间和 sudo 接口。
- `include/cp0_lvgl_app_runner.hpp`：LVGL runner。
- `include/ui_app_page.hpp`：页面基类和页面启动辅助函数。

信号是 eventpp `CallbackList` 全局对象。请求型信号的参数通常是 `std::list<std::string>`，第一个元素是命令名，最后一个参数是可选回调 `std::function<void(int, std::string)>`。回调约定为 `(code, data)`：`code == 0` 表示成功，负值表示错误；`data` 是命令结果，可能为空、文本、逐行记录或二进制结构体字节。调用方不要假定回调一定在当前线程执行，也不要在回调中抛出异常。

服务必须先由 `cp0_lvgl_init()`（通常由 `cp0_lvgl_run()` 自动调用）初始化。初始化由 Kconfig 的 `CONFIG_CP0_LVGL_INIT_*` 控制，并按 `cp0_init_plan` 顺序安装服务；未启用的服务没有可用实现。重复初始化一般是幂等的，但不要依赖未启用服务的默认行为。

最小请求示例：

```cpp
cp0_signal_wifi_api({"Status"}, [](int code, std::string payload) {
    if (code != 0) return;
    // payload: connected:ssid:ip:signal:ethernet
});
```

应用代码优先使用 `cp0_lvgl_app.h` 的 C 包装函数（例如 `cp0_wifi_status_read`），它们负责参数检查和 wire 格式解码。

## 2. LVGL 和页面接口

### `cp0_lvgl_run`

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

runner 在一个进程中只允许成功运行一次。流程为：`lv_init` -> `after_lvgl_init` -> `cp0_lvgl_init` -> `after_resource_init` -> `setup` -> LVGL timer loop -> `teardown` -> 服务释放。`setup` 返回 false 会触发清理并返回 1；回调抛异常也返回 1。`should_quit` 返回 true 时退出主循环。后台线程或外部事件需要唤醒等待中的 LVGL loop 时调用 `cp0_lvgl_wake()`。

### 页面

- `AppPageRoot`：拥有一个 LVGL screen 和 input group；用 `screen()`、`input_group()` 访问。
- `AppPage`：标准页面，包含内容区 `ui_APP_Container` 和顶部栏。
- `AppPageWithBottomBarLayout`：额外提供 `ui_BOTTOM_Container`。
- `cp0_lvgl_start_app_page(AppPageRoot&)`：加载已有页面并绑定 keypad/encoder 输入组。
- `cp0_lvgl_start_app<PageT>(args...)`：构造、加载并返回 `std::unique_ptr<PageT>`。

页面对象必须在 LVGL 线程创建和销毁。页面析构前不要手动删除其 root screen；基类负责 LVGL 对象生命周期。顶部栏固定高度为 `AppPageRoot::kTopBarHeightPx`（20 px）。

## 3. C ABI 公共接口

声明集中在 `include/cp0_lvgl_app.h` 和 `include/hal_lvgl_bsp.h`。`cp0_lvgl_init()` 负责按 Kconfig 初始化服务；通常由 runner 调用一次，不要在服务运行后重复手动初始化。除特别说明外，返回 0 表示成功，负值表示失败；字符串输出均要求调用方提供有效缓冲区。

### 路径和文件

```c
void cp0_lvgl_init(void);
const char *cp0_file_path_c(const char *file);
int cp0_dir_list(const char *path, cp0_dirent_t *entries, int max_entries, int *out_count);
cp0_watcher_t cp0_dir_watch_start(const char *path);
int cp0_dir_watch_poll(cp0_watcher_t watcher);
void cp0_dir_watch_stop(cp0_watcher_t watcher);
int cp0_file_read_first_line(const char *path, char *out, int out_size);
```

`cp0_file_path_c` 将逻辑资源名解析为平台路径；常用键包括 `applications`、`appstore_exec`、`calculator_exec`、`adb_helper`、`launcher_settings`、`oobe_marker`、`home_dir`、`lock_file`、`keyboard_device`、`keyboard_map`，以及 `share/images/`、`share/audio/`、字体资源。C++ 可用 `std::string cp0_file_path(std::string)`。

返回的 C 字符串属于当前线程的缓存，不要释放；需要长期保存时立即复制到自己的缓冲区或 `std::string`。

目录 watcher 的句柄只可交给同一组 `start/poll/stop` 使用；`poll` 返回非负事件状态，非法或已停止句柄返回负值。`cp0_dir_list` 将目录项写入调用方数组并通过 `out_count` 返回数量。

### 网络和 Wi-Fi

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

`cp0_wifi_scan` 返回 AP 数量；失败返回 `CP0_WIFI_ERROR_*`（`INVALID=-1`、`RADIO_OFF=-2`、`AUTH=-3`、`NOT_FOUND=-4`、`IP_CONFIG=-5`、`SERVICE=-6`、`TIMEOUT=-7`）。`signal` 通常是 RSSI dBm，旧后端可能返回 0..100。`cp0_wifi_profile_exists` 和 `cp0_wifi_radio_enabled` 返回后端整数状态，调用方应按实际平台处理。

### 进程和系统

```c
int cp0_process_exec_blocking(const char *exec_path, int keep_root);
cp0_pid_t cp0_process_spawn(const char *exec_path, int keep_root);
void cp0_process_stop(cp0_pid_t pid);
int cp0_process_check_lock(const char *lock_path, int *holder_pid);
void cp0_process_kill(int pid, int grace_ms);
void cp0_system_shutdown(void);
void cp0_system_reboot(void);
int cp0_process_run_argv(const char *const *argv, int background);
int cp0_process_capture_argv(const char *const *argv, char *out, int out_size);
```

`keep_root` 和 `background` 使用 0/1。`grace_ms` 最大 300000，`DelayMs` 信号参数也有同样上限。`cp0_process_run_sudo` 已 deprecated，必须改用异步 sudo 接口。外部应用执行前应使用 `cp0_desktop_exec_is_safe` 检查。

### sudo

```c
int cp0_sudo_run_argv_async(const char *const *argv,
    cp0_sudo_callback_thread_t callback_thread,
    cp0_sudo_output_cb_t output_cb, cp0_sudo_complete_cb_t complete_cb, void *user);
int cp0_sudo_run_shell_async(const char *command,
    cp0_sudo_callback_thread_t callback_thread,
    cp0_sudo_output_cb_t output_cb, cp0_sudo_complete_cb_t complete_cb, void *user);
int cp0_sudo_run_argv_async_ex(const char *const *argv,
    cp0_sudo_callback_thread_t callback_thread,
    cp0_sudo_output_cb_t output_cb, cp0_sudo_complete_cb_t complete_cb, void *user,
    int auth_timeout_ms, int exec_timeout_ms, uint64_t *request_id);
int cp0_sudo_cancel(uint64_t request_id);
int cp0_sudo_queue_password(const char *password);
```

`CP0_SUDO_CALLBACK_LVGL` 将输出/完成回调投递到 LVGL 线程，`CP0_SUDO_CALLBACK_WORKER` 在 worker 线程直接调用。输出按顺序在完成前送达，LVGL 输出有有界背压，回调必须快速返回。完成结果为 `CP0_SUDO_RESULT_SUCCESS`、`AUTH_FAILED`、`EXEC_FAILED`、`CANCELLED` 或 `TIMED_OUT`，同时提供进程退出码。`cp0_sudo_cancel` 可重复调用；未知或过期 request id 返回 `-ENOENT`。密码不会通过普通日志输出；调用者负责避免在 UI 状态中保存明文。

### 电池、背光和时间

```c
cp0_battery_info_t cp0_battery_read(void);
int cp0_bq27220_calibrate(int command_index);
int cp0_backlight_read(void);
int cp0_backlight_max(void);
int cp0_backlight_write(int val);
void cp0_time_str(char *buf, int buf_size);
int cp0_time_set(const char *timestamp);
int cp0_time_ntp_get(void);
int cp0_time_ntp_set(int enable);
```

`cp0_battery_read` 只读后台缓存，不触发硬件 I/O；`valid == 1` 才可使用数据。电池字段单位：电压 mV、电流 mA、温度 0.1 C、容量 mAh、`soc` 为百分比。`cp0_time_set` 格式必须为 `YYYY-MM-DD HH:MM:SS`；手动 RTC 设置要在 NTP 关闭时才可持久化。`cp0_backlight_write` 会限制在 0..max。

### 其他 C 接口

- `cp0_network_default_info_read` / `cp0_eth_info_read`：读取 IPv4、gateway、MAC。
- `cp0_account_info_read`：读取当前 user 和 hostname。
- `cp0_system_apt_update_background`、`cp0_system_update_launcher_background`：启动后台更新。
- `cp0_desktop_exec_is_safe`：返回安全检查结果并写入原因。
- `cp0_lora_request_stop`、`cp0_lora_clear_stop`：取消/清除 LoRa 初始化停止请求。

## 4. `cp0_signal_*` 完整用法

以下命令中的 `args` 都是 `std::list<std::string>`，命令名位于第 0 个元素。除单向信号外，回调签名统一为 `void(int code, std::string data)`。

### 音频信号

#### `cp0_signal_audio_play(std::string path)`

单向播放文件路径，不回调结果。`cp0_signal_system_play(std::string name)` 播放系统音效名；名称可以是平台的三个索引音效，也可以先通过 `RegisterSystemSounds` 注册的名称。

#### `cp0_signal_audio_cap(bool enable)`

单向开始/停止录音。需要状态回调、波形和停止播放时使用 `cp0_signal_audio_setup`：

| args | 作用 |
| --- | --- |
| `{"set_callback"}` | 设置录音/播放状态回调；后续无 callback 的状态也发到该回调 |
| `{"set_waveform"}`、`{"set_waveform","0"}` | 开启/关闭波形回调；也接受 `on/off/true/false` |
| `{"stop_play"}` | 停止当前播放 |

#### `cp0_signal_audio_api(args, cb)`

| 命令 | 参数 | 成功 data/行为 |
| --- | --- | --- |
| `PlayFile`, `Play` | 文件/资源名 | `play start\n` |
| `PlayPause`, `PlayContinue`, `PlayEnd` | 无 | 暂停、继续、停止 |
| `Cap`, `CapPause`, `CapContinue`, `CapEnd` | 无 | 录音开始、暂停、继续、停止 |
| `CapFileSave` | 输出文件 | 保存录音 |
| `SetCallback` | 无 | 设置状态回调 |
| `VolumeRead` | 无 | data 为 0..100 音量 |
| `VolumeWrite` | 0..100 | data 为实际值 |
| `MuteRead` | 无 | data 为 0/1 |
| `MuteToggle` | 无 | data 为切换后的 0/1 |
| `SetSystemSoundNames` | 最多 3 个名称 | 重置平台索引音效名 |
| `RegisterSystemSounds` | 1..32 个名称 | 追加应用音效名 |
| `SystemSoundPlay` | 索引 0..2 | 播放索引音效 |
| `SystemSoundSuspend` | 无 | 暂停系统音效队列 |
| `SystemSoundPrepare` | 无 | 预热系统音效播放器 |
| `SystemSoundEnable` | 可省略，或 0/1、on/off 等 | data 为当前 0/1 |

`Play` 的无分隔文件名按资源路径解析；`PlayFile` 按原始路径处理。无 callback 时通常使用 `set_callback` 设置的状态回调。播放/录音错误会返回负 code 和说明文本。

### PTY：`cp0_signal_pty_api(args, cb)`

| 命令 | 参数 | data/code |
| --- | --- | --- |
| `Open` | `可执行文件 [columns] [rows] [argv...]` | 成功 data 为数字句柄；默认 80x24 |
| `Read` | `句柄 [max_read]` | code 为读取字节数，data 为原始字节；默认 4096，最大 1 MiB；无数据为 code=0 |
| `Write` | `句柄 数据` | code 为写入字节数 |
| `Resize` | `句柄 columns rows` | 成功 code=0 |
| `CheckChild` | `句柄` | code=0 运行中，code=1 已结束，data 为退出码 |
| `Close` | `句柄` | 关闭并回收子进程 |

句柄是模块内部注册的非零数字字符串，不是 PID。Linux PTY 子进程默认设置 `TERM=vt100`，并可能按 `run_as_user` 配置降权。关闭/模块反初始化会杀死仍存活的子进程。

### 配置：`cp0_signal_config_api(args, cb)`

配置文件为 `$HOME/.config/cardputerzero/config.json`，最多 64 项；key 最大 63 字节，value 最大 255 字节。

| 命令 | 参数 | 结果 |
| --- | --- | --- |
| `Init` | 无 | 重新加载，data=`ok` |
| `Save` | 无 | 原子保存，data=`ok` |
| `GetInt` | `key fallback` | data 为整数 |
| `SetInt` | `key value` | data=`ok`，仅修改内存 |
| `GetStr` | `key fallback` | data 为字符串 |
| `SetStr` | `key value` | data=`ok`，仅修改内存 |
| `SetManyAndSave` | `key value [key value...]` | 成功才同时提交并保存 |

`SetInt/SetStr` 后必须调用 `Save` 才持久化；事务参数必须成对且至少一对。

### 文件系统：`cp0_signal_filesystem_api(args, cb)`

命令：`Path logical_name`（解析路径）、`DirList path`、`DirListDetail path`、`Exists path`、`ReadFile path [max_bytes]`、`EnsureDirForUser path`、`Touch path`、`Remove path`、`WatchStart path`、`WatchPoll handle`、`WatchStop handle`。成功的 `Exists` data 为 `1/0`；`WatchStart` data 为句柄；`WatchPoll` data 为轮询结果；目录列表为编码后的逐行记录。路径不能为空且不能包含 NUL；未知命令 code=-2。

### LoRa：`cp0_signal_lora_api(args, cb)`

命令：`Init`、`Poll`、`Info`、`SendText text`、`StartReceive`、`SetTxMode 0|1`、`Shutdown`。`SendText` 非空且最多 127 字节。`Poll`/`Info` 成功 data 是原始 `cp0_lora_info_t` 二进制（必须按同一 ABI 解码），字段见 `cp0_lvgl_app.h`。`Poll` 会先执行硬件轮询，`Info` 只读当前状态。

### Wi-Fi：`cp0_signal_wifi_api(args, cb)`

命令：`Status`、`Scan [limit]`（0..32）、`Connect ssid [password]`、`ConnectHidden ssid [password]`、`Disconnect`、`ProfileForget ssid`、`ProfileExists ssid`、`ProfileDisconnectActive`、`RadioEnabled`、`RadioSetEnabled on|off|1|0|true|false`。`Status` data 为 `connected:ssid:ip:signal:ethernet`；`Scan` 每行是 `ssid:signal:security:in_use:saved`，字段中的 `\\`、`\:`、`\n`、`\r` 已转义。推荐使用 C 包装函数解码。

### 蓝牙：`cp0_signal_bt_api(args, cb)`

常规查询/扫描命令：`BtStatus`、`BtList [max=16]`、`BtConnectedList [max=16]`、`BtScan [max=16]`。控制命令：`BtReset`、`BtPower 0|1`、`BtAlias name`、`BtDiscoverable 0|1`、`BtDiscoveryStart`、`BtDiscoveryStop`、`BtPair address`、`BtCancelPairing address`、`BtConnect address`、`BtDisconnect address`、`BtRemove address`。address 必须是 `XX:XX:XX:XX:XX:XX`；列表每行是 `address\trssi\tconnected\tpaired\ttrusted\tname`；状态是 `powered\taddress\tdiscoverable\talias`。`BtScan`、配对和连接操作的最终回调可能异步到达。

长操作可使用会话命令：`BtSessionInit`（返回非零 session id）、`BtSessionDeinit id`、`BtStatusGet id`、`BtConnectedListInit id`、`BtConnectedListGet id`、`BtConnectedListDeinit id`、`BtScanOn id`、`BtScanOff id`。`BtScanOn` 会在后台线程持续扫描，通常每 3 秒回调一次；不用时必须 `BtScanOff` 后再销毁会话。

### 蓝牙 Agent：`cp0_signal_bt_agent`

签名：`(uint64_t id, std::string method, std::string device, std::string value, std::function<void(bool accepted, std::string text)> reply)`。请求来自 BlueZ worker 线程，UI 必须切到自己的 LVGL/event loop 后显示确认框，再调用 `reply`。`method` 可能是 `RequestConfirmation`（value 为 passkey）或 PIN/UUID 相关请求；没有 UI 订阅者时后端会拒绝不安全请求。reply 必须最终调用一次。

### 设置：`cp0_signal_settings_api(args, cb)`

命令：`BacklightRead`、`BacklightMax`、`BacklightWrite value`、`Log topic message`、`TimeStr`、`GpioSet name 0|1`、`GpioGet name`。GPIO 名支持 `GROVE5V`/`extport_usb`、`EXT5V`/`extport_5vout`、`BACKLIGHT`。背光读写返回整数；`TimeStr` 返回 `HH:MM`；GPIO 读返回 `0/1`。

### 进程：`cp0_signal_process_api(args, cb)`

命令和参数：`ExecBlocking executable keep_root`、`Spawn executable keep_root`、`Stop pid`、`CheckLock path`、`Kill pid grace_ms`、`RunArgv background argv...`、`RunSudo password argv...`（旧接口，禁止新增使用）、`CaptureArgv argv...`、`AdbStatus`、`DesktopExecIsSafe executable`、`Shutdown`、`Reboot`、`DelayMs milliseconds`。`Spawn` data 为 PID；`CaptureArgv` data 为 stdout；`AdbStatus` data 为 `adbd=active|inactive` 和 `enabled=enabled|disabled` 两行。

### OS 信息和时间：`cp0_signal_osinfo_api` / `cp0_signal_timedate_api`

两者由同一个 OS info 服务处理。命令：`NetworkDefaultInfoRead`、`EthInfoRead`（data 三行 IPv4/gateway/MAC）、`NetworkList`（每行 iface、IPv4、netmask、is_up，以 tab 分隔）、`AccountInfoRead`（user 和 hostname 两行）、`TimeSet timestamp`、`LocalTime`（`year,month,day,hour,minute,second`）、`RandomU32`、`NtpGet`、`NtpSet 0|1`、`AptUpdateBackground`、`UpdateLauncherBackground`。

### 电池和截图

- `cp0_signal_bq27220_api(args, cb)`：`Read` 返回编码后的 `cp0_battery_info_t`；`Calibrate index` 执行校准命令，当前有效索引为 0..3。推荐 `cp0_battery_read` / `cp0_bq27220_calibrate`。
- `cp0_signal_screenshot_api(args, cb)`：`Save directory` 将当前 framebuffer 保存为 BMP，成功 data 为完整文件路径。设备端支持 16/32 bpp；SDL 端可能采用模拟实现。
- `cp0_signal_battery_pub(std::function<void()> callback)`：触发一次电池发布。当前后端会读取缓存并向活动 LVGL screen 发送电池事件，传入的 callback 参数仅为信号签名兼容而保留，不要把它当作订阅注册接口；调用应发生在 LVGL 线程，避免阻塞。

### 摄像头：`cp0_signal_camera_api(args, cb)`

命令：`Open [width] [height]`、`Start [width] [height]`、`Close`、`Stop`、`Capture [path] [width] [height]`、`Photo [path] [width] [height]`、`Status`、`SetCallback`、`SetFrameCallback`、`ZoomIn`、`ZoomOut`、`Pan dx dy`。默认尺寸 320x150。`Status` code=0 表示 streaming，code=1 表示 stopped；不支持 libcamera 时返回 -10。帧回调属于后端线程语境，UI 使用前必须转发到 LVGL 线程；关闭摄像头会清除两个 callback。

### 声卡：`cp0_signal_soundcard_api(args, cb)`

命令：`ListCards`（`/proc/asound/cards` 编码）、`ListControls card_index`、`GetControlDetail card_index control`、`SetControl card_index control value`。底层调用 `amixer`；没有 ALSA/进程服务时返回 `not supported` 或后端错误。控制值保持原始 amixer 文本，不要假设只能是整数。

### sudo 专用信号

#### `cp0_signal_sudo_argv_async`

签名：`(std::list<std::string> argv, int auth_timeout_ms, int exec_timeout_ms, std::function<void(int,int)> complete, std::function<void(int,uint64_t)> started)`。`started(ret, request_id)` 先返回提交结果；成功 ret=0 且 request_id 非零。之后 `complete(result_code, exit_code)` 恰好一次。需要取消时调用 `cp0_signal_sudo_cancel(request_id, done)`，`done` 收到 0 表示已接受取消。超时和取消结果仍通过 complete 回调报告。

#### `cp0_signal_system_admin_async`

签名同上，但 args 不是任意 argv，而是受限的系统管理命令：

| args | 实际操作 |
| --- | --- |
| `{"AdbSet","0|1"}` | 切换 ADB |
| `{"AdbAuthorize","公钥"}` | 授权 ADB 公钥，长度 680..2048 且不能含换行 |
| `{"AdbRevoke","指纹"}` | 撤销 64 位十六进制指纹 |
| `{"AdbClearAuthorizations"}` | 清理全部授权 |
| `{"NtpSet","0|1"}` | 设置 NTP |
| `{"TimeSet","YYYY-MM-DD HH:MM:SS"}` | 设置系统时间并写 RTC |

参数校验失败只调用 `started(-EINVAL, 0)`，不会产生 complete 回调。UI 页面应保存 request id，并在离开页面或超时路径调用取消。

### 保留/当前未安装实现的信号

`cp0_signal_network()`、`cp0_signal_forkexec()` 在信号计划中保留，但当前 `cp0_lvgl` 源码没有对应注册点；不要把它们当作可用公共服务。新增实现时必须同时补注册、生命周期清理、SDL/设备行为和测试。

## 5. 修改和排错规则

1. 先看本文件和 `include/signal_register_plan.h`，确认调用签名、命令名、返回编码。
2. 修改接口时同步更新设备端与 SDL 后端、C 包装函数、调用方和测试；新增外部信号必须在 `signal_register_plan.h` 使用 `def_hal_fun` 声明。
3. 修 bug 时必须检查对应实现文件和 contract 文件，不能只根据本文推断行为；重点检查回调线程、服务是否初始化、反初始化是否仍有后台线程、wire 数据是否需要转义。
4. 运行模块测试：`ext_components/cp0_lvgl/tests/run_tests.sh`。涉及平台命令、BlueZ、libcamera、framebuffer 或 sudo 的改动，还要在目标平台做对应集成验证。
