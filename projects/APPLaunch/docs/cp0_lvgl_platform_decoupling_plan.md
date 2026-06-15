# cp0_lvgl / APPLaunch 平台解耦计划

日期：2026-06-15

## 目标

让 APPLaunch 保持平台无关：页面和业务逻辑只调用 `cp0_*` 接口，不直接包含或调用 Linux/macOS/Windows/SDL 特有 API、系统命令、`/dev`、`/sys`、GPIO/SPI/I2C、RadioLib HAL 等实现细节。

`cp0_lvgl` 负责承载平台和硬件实现，并保持现有风格：

- APP 层使用 `cp0_*` facade。
- 平台层能力尽量靠近已有 `hal_*`/service 实现。
- C ABI、POD struct、简单返回码。
- SDL/CP0/Win/Web 可以分别提供实现或 stub。

## 当前状态

已完成：

- Settings 页中的 `system()`/`popen()`/`nmcli`/账号信息/eth0 信息/BQ27220 I2C 校准已下沉到 `cp0_lvgl`。
- APPLaunch 使用 `cp0_*` 调用。
- `Launch` 的 `.desktop Exec` 安全校验使用 `cp0_desktop_exec_is_safe()`。

仍需处理：

- `cp0_app_platform.cpp` 是杂物文件，职责过宽。
- `cp0_lvgl_app.h` 继续膨胀，需要后续分组收敛。
- `ui_app_compass.hpp` 仍直接枚举 `/sys/bus/iio/devices`。
- `ui_app_lora.hpp` 仍包含 GPIO/SPI/I2C/RadioLib 实现。
- `AppRegistry.cpp`/`Launch.cpp` 仍有少量平台宏判断。

## 阶段计划

### 阶段 1：收敛新增 cp0 平台服务

目的：不改变 APPLaunch 行为，先让 `cp0_lvgl` 内部职责清晰。

动作：

1. 拆分 `ext_components/cp0_lvgl/src/cp0_app_platform.cpp`：
   - `cp0_app_process_utils.cpp`：argv 执行、capture、文件首行读取、desktop Exec 校验。
   - `cp0_app_system_info.cpp`：Ethernet、Account 信息。
   - `cp0_app_wifi_profile.cpp`：WiFi profile 查询、删除、断开 active profile。
   - `cp0_app_update.cpp`：apt update、launcher update。
   - `cp0_app_bq27220.cpp`：BQ27220 calibration。
2. 维持 `cp0_lvgl_app.h` 当前 ABI，避免 APPLaunch 二次改动。
3. 构建验证 SDL2。

### 阶段 2：命名和边界收敛

目的：把新增能力放进更合理的服务边界。

动作：

1. 评估是否新增/扩展 `hal_process`：`run_argv`、`capture_argv`。
2. 评估 `cp0_eth_info_read()` 是否改为扩展 `cp0_network_list()` 或新增 generic `cp0_network_default_info_read()`。
3. 将 WiFi profile 命名稳定为：
   - `cp0_wifi_profile_exists()`
   - `cp0_wifi_profile_forget()`
   - `cp0_wifi_profile_disconnect_active()`
4. 将 APPLaunch 专属 update 策略从 cp0_lvgl 长期接口中剥离：优先改成通用 package/service API。

### 阶段 3：迁移 Compass IIO

目的：APPLaunch 不再直接读 `/sys/bus/iio/devices`。

动作：

1. 在 `cp0_lvgl` 新增 sensor/imu/compass 数据接口。
2. 把 IIO 枚举、raw/scale 读取移动到 cp0_lvgl。
3. `ui_app_compass.hpp` 只保留 UI 和状态渲染。
4. SDL 提供 fake/stub sensor 数据。

### 阶段 4：迁移 LoRa 硬件 HAL

目的：APPLaunch 不再包含 GPIO/SPI/I2C/RadioLib HAL。

动作：

1. 在 `cp0_lvgl` 定义 `cp0_lora_*` service：init/deinit/status/send/poll/diag。
2. 移动 GPIO/SPI/I2C/PI4IO/RadioLib HAL 到 cp0_lvgl。
3. `ui_app_lora.hpp` 改为纯 UI controller，调用 `cp0_lora_*`。
4. SDL 提供不可用或模拟实现。

### 阶段 5：平台 feature 模型

目的：APPLaunch 不再用 `__linux__`/`HAL_PLATFORM_SDL` 判断产品功能。

动作：

1. 新增 `cp0_feature_available(feature)` 或枚举 API。
2. AppRegistry 用 feature gate 控制 Camera/LoRa/IP panel 等 app。
3. 移除 APPLaunch 平台宏分支。

## 验证标准

每阶段至少运行：

```sh
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -Q -j8 --implicit-deps-changed
```

并扫描 APPLaunch 平台残留：

```sh
rg -n "(/dev/|/sys/|ioctl\(|system\(|popen\(|fork\(|execvp|nmcli|RadioLib|linux/i2c|spidev|gpio|__linux__|HAL_PLATFORM_SDL)" projects/APPLaunch/main
```

## 2026-06-15 执行进展

本轮已完成：

- `cp0_app_platform.cpp` 已拆散为 process/system-info/wifi/update/BQ27220 等独立实现文件。
- `cp0_lvgl_app.h` 新增 C facade：process argv、desktop Exec 安全检查、network/account、WiFi profile、update、time、BQ27220、Compass、LoRa。
- `ui_app_compass.hpp` 已移除 IIO `/sys/bus/iio/devices` 枚举和 raw/scale 读取，只消费 `cp0_compass_read()`。
- `ui_app_lora.hpp` 已移除 Linux GPIO/SPI/I2C/RadioLib HAL、`/dev`、`/sys`、`ioctl()` 等实现，只保留 UI、键盘和状态渲染，硬件层改由 `cp0_lora_*` 提供。
- LoRa 的 GPIO/SPI/I2C/PI4IO/RadioLib 逻辑已移动到 `ext_components/cp0_lvgl/src/cp0_app_lora.cpp`。
- `RadioLib` 依赖已加入 `cp0_lvgl/SConstruct`；当前由于 SCons 静态库链接顺序限制，`APPLaunch/main/SConstruct` 仍保留直接 `RadioLib` 依赖以通过链接，后续应在构建系统层修复 transitive link order 后移除。
- WiFi profile 的旧别名接口已移除声明和实现，保留 `cp0_wifi_profile_exists()` / `cp0_wifi_profile_forget()` / `cp0_wifi_profile_disconnect_active()`。

验证：

```sh
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -Q -j8 --implicit-deps-changed
```

结果：通过。

当前 APPLaunch 平台残留扫描结果：

- 主要剩余为 POSIX 文件管理/轻量平台 include：`main.cpp`、`UILaunchPage.cpp`、`Launch.cpp`、`ui_global_hint.cpp`、`ui_app_page.hpp`、`ui_app_file.hpp`、`ui_app_camera.hpp`、`ui_app_rec.hpp`、`ui_app_console.hpp`、`ui_app_setup.hpp`。
- `AppRegistry.cpp` / `Launch.cpp` / `zero_lvgl_os.cpp` 仍有 `__linux__` / `HAL_PLATFORM_SDL` feature gate。
- `ui_app_lora.hpp` 和 `ui_app_compass.hpp` 已无直接硬件/平台访问。
- `APPLaunch/main/SConstruct` 仍直接声明 `RadioLib`，原因见上面的链接顺序说明。

后续建议：

1. 把 `cp0_app_lora.cpp`、`cp0_app_compass.cpp`、`cp0_app_bq27220.cpp` 等 Linux/硬件实现按 cp0_lvgl 原风格继续拆到平台目录或提供 SDL stub，根 `src/` 仅保留平台无关 glue。
2. 建立 `cp0_feature_available()`，替换 APPLaunch 中的 `__linux__` / `HAL_PLATFORM_SDL` app gate。
3. 将 `ui_app_file.hpp`、`ui_app_camera.hpp`、`ui_app_rec.hpp` 的目录创建/遍历/删除收敛到 `cp0_file_*`。
4. 修复构建系统 transitive static link order 后，从 APPLaunch `REQUIREMENTS` 移除 `RadioLib`。

补充验证：

```sh
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -Q -j8 --implicit-deps-changed
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -Q -j8 --implicit-deps-changed
```

结果：均通过。当前仅保留既有/迁移后的 `snprintf` 截断 warning：`cp0_app_lora.cpp` 诊断字符串和 `ui_app_setup.hpp` WiFi title。

## 2026-06-15 下一步执行：平台目录收敛

本轮继续完成：

- 将 CP0 硬件专用实现移入 `ext_components/cp0_lvgl/src/cp0/`：
  - `cp0_app_bq27220.cpp`
  - `cp0_app_compass.cpp`
  - `cp0_app_lora.cpp`
- 新增 SDL stub：`ext_components/cp0_lvgl/src/sdl/cp0_app_hardware_stub_sdl.cpp`。
  - SDL 下 `cp0_compass_read()` 返回 unavailable。
  - SDL 下 `cp0_lora_*()` 返回 unavailable/stub 信息。
  - SDL 下 `cp0_bq27220_calibrate()` 返回失败。
- 修复本轮能处理的截断 warning：
  - `cp0_app_lora.cpp` 的 SPI 候选/诊断字符串增加长度限制。
  - `ui_app_setup.hpp` 的 WiFi title 对 SSID/IP 做长度限制。

验证：

```sh
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -Q -j8 --implicit-deps-changed
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -Q -j8 --implicit-deps-changed
```

结果：均通过。

当前边界：

- `src/` 根目录仍保留平台工具/策略文件：`cp0_app_process_utils.cpp`、`cp0_app_system_info.cpp`、`cp0_app_update.cpp`、`cp0_app_wifi_profile.cpp`。
- 其中 `update` 和 `wifi_profile` 仍含 Linux 命令策略；后续若继续严格化，应拆到 `src/cp0/` 并在 `src/sdl/` 提供 stub，或者抽为更通用的 package/service/process API。

## 2026-06-15 WiFi API 收敛

本轮将 cp0 WiFi 相关接口统一到 `cp0_signal_wifi_api` / `cp0_lvgl_wifi.cpp` 风格：

- 新增 `cp0_signal_wifi_api`，签名与 audio/camera API 保持一致：
  - `std::list<std::string>` 参数
  - `std::function<void(int, std::string)>` 回调
- 新增统一实现：`ext_components/cp0_lvgl/src/cp0/cp0_lvgl_wifi.cpp`。
  - `init_wifi()` 注册 `cp0_signal_wifi_api` handler。
  - `cp0_wifi_get_status()`、`cp0_wifi_scan()`、`cp0_wifi_connect()`、`cp0_wifi_disconnect()` 均通过 signal API 转发。
  - `cp0_wifi_profile_forget()`、`cp0_wifi_profile_exists()`、`cp0_wifi_profile_disconnect_active()` 也统一到同一个 signal API。
  - 内部改用 `cp0_process_run_argv()` / `cp0_process_capture_argv()`，不再在 WiFi 实现里使用 `system()` / `popen()` / shell 拼接。
- 新增 SDL 转接：`ext_components/cp0_lvgl/src/sdl/cp0_lvgl_wifi.cpp`，与 audio 当前写法一致，直接复用 `../cp0/cp0_lvgl_wifi.cpp`。
- 删除旧的 WiFi profile 独立实现：`ext_components/cp0_lvgl/src/cp0_app_wifi_profile.cpp`。
- 从旧位置移除重复 WiFi 实现：
  - `ext_components/cp0_lvgl/src/cp0/cp0_app_settings.cpp`
  - `ext_components/cp0_lvgl/src/sdl/cp0_app_compat_sdl.cpp`
- `cp0_lvgl_init()` 在 CP0/SDL 初始化路径中增加 `init_wifi()`。

验证：

```sh
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -Q -j8 --implicit-deps-changed
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -Q -j8 --implicit-deps-changed --config=force
g++ -std=c++17 -Iext_components/cp0_lvgl/include -ISDK/github_source/eventpp/include -c ext_components/cp0_lvgl/src/cp0/cp0_lvgl_wifi.cpp -o /tmp/cp0_lvgl_wifi.o
```

结果：均通过。

## 2026-06-15 Process API 收敛

本轮将 `cp0_process_*` 相关接口统一到 `cp0_signal_process_api` / `cp0_lvgl_process.cpp` 风格：

- 新增 `cp0_signal_process_api`，签名与 audio/wifi/camera API 保持一致：
  - `std::list<std::string>` 参数
  - `std::function<void(int, std::string)>` 回调
- 新增统一实现：`ext_components/cp0_lvgl/src/cp0/cp0_lvgl_process.cpp`。
  - `init_process()` 注册 `cp0_signal_process_api` handler。
  - `cp0_process_exec_blocking()`、`cp0_process_spawn()`、`cp0_process_stop()`、`cp0_process_check_lock()`、`cp0_process_kill()` 均通过 signal API 转发。
  - `cp0_process_run_argv()`、`cp0_process_capture_argv()` 也统一到同一个 process API。
  - `cp0_system_shutdown()` / `cp0_system_reboot()` 随 process service 一起收口，避免继续留在旧 process 文件中。
  - `.desktop Exec` 安全检查和 `cp0_file_read_first_line()` 作为 process/file utility 保留在同一统一文件中。
- 新增 SDL 转接：`ext_components/cp0_lvgl/src/sdl/cp0_lvgl_process.cpp`，与 audio/wifi 写法一致，复用 `../cp0/cp0_lvgl_process.cpp`。
- 初始化路径接入：CP0/SDL 两条 `cp0_lvgl_init()` 都在 audio 后、wifi 前调用 `init_process()`，确保 WiFi/update/system-info 中的 argv API 可用。
- 删除旧实现：
  - `ext_components/cp0_lvgl/src/cp0/cp0_app_process.cpp`
  - `ext_components/cp0_lvgl/src/cp0_app_process_utils.cpp`
- SDL 旧 compat 中的 `cp0_process_*` wrapper 已移除，PTY wrapper 保留，因为它不属于本次 process API 收敛范围。

验证：

```sh
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -Q -j8 --implicit-deps-changed
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -Q -j8 --implicit-deps-changed
g++ -std=c++17 -DHAL_PLATFORM_SDL=1 -Iext_components/cp0_lvgl/include -ISDK/github_source/eventpp/include -c ext_components/cp0_lvgl/src/cp0/cp0_lvgl_process.cpp -o /tmp/cp0_lvgl_process_sdl.o
g++ -std=c++17 -Iext_components/cp0_lvgl/include -ISDK/github_source/eventpp/include -c ext_components/cp0_lvgl/src/cp0/cp0_lvgl_process.cpp -o /tmp/cp0_lvgl_process_cp0.o
```

结果：均通过。

协作说明：本轮期间工作树里已有其他人对 filesystem/screenshot/lora/bq27220 等模块的并行修改；编译过程中一度暴露的 filesystem 重复定义属于并行改动范畴，后续再次构建已通过。
