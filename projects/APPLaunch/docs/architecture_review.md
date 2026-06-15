# APPLaunch 架构审查报告

日期：2026-06-15  
范围：`projects/APPLaunch`  
方式：主线程梳理 + 3 个并行 agent 分别审查 UI/应用层、平台/构建层、质量/生命周期。

## 1. 总体判断

APPLaunch 当前更像一个快速演进后的产品型单体：功能能跑起来，但应用注册、平台能力、页面 UI、硬件访问、构建适配、资源路径之间互相穿透。

最主要的不和谐不是单点代码风格问题，而是三条架构边界不清：

1. **应用模型边界不清**：Launcher 列表、Settings 开关、动态 `.desktop` 应用、页面工厂、资源图标各自维护状态。
2. **平台能力边界不清**：UI 页面中直接出现 `system()`、`popen()`、`/dev`、`/sys`、GPIO/SPI/I2C、平台宏和硬编码 Linux 路径。
3. **生命周期边界不清**：LVGL 对象、timer、watcher、PTY、动画、页面对象多处裸持有，部分清理依赖路径或进程退出。

建议优先修复 P0/P1 的一致性、安全和生命周期问题，再做大规模模块拆分。

## 2. 代码结构概览

- `projects/APPLaunch/SConstruct`：项目级构建入口，选择默认配置、SDK 路径、交叉编译 static lib/sysroot 等。
- `projects/APPLaunch/main/SConstruct`：组件构建入口，运行 `main/ui/generate_page_app_includes.py`，编译 `src/*.c*` 和 `ui` 目录。
- `projects/APPLaunch/main/src/main.cpp`：运行时入口，初始化 LVGL/CP0，注册键盘事件，进入 `APPLaunch_lock()` + `lv_timer_handler()` 主循环。
- `projects/APPLaunch/main/ui/ui.cpp`：全局 UI 根对象 `std::unique_ptr<zero_lvgl_os> home`，`launcher_ui::init()` 创建并启动。
- `projects/APPLaunch/main/ui/zero_lvgl_os.cpp`：应用层组装器，创建主题、字体缓存、`Launch`、`UILaunchPage`，决定启动 GIF 或首页。
- `projects/APPLaunch/main/ui/Launch.*`：应用列表、应用启动、`.desktop` 扫描、目录 watch、回主页逻辑。
- `projects/APPLaunch/main/ui/UILaunchPage.*`：首页 UI、轮播控件、键盘/点击事件、启动动画、页面切换动画。
- `projects/APPLaunch/main/ui/ui_app_page.hpp`：应用页基类和通用 top/content/bottom layout。
- `projects/APPLaunch/main/ui/page_app/*.hpp`：内置页面，多数是 header-only 千行级实现。
- `projects/APPLaunch/APPLaunch/share`：运行时资源目录，包含 images/audio/font。
- `projects/APPLaunch/dist.bak`：备份资源/产物副本，体积和资源来源容易造成歧义。

## 3. P0 高优先级问题

### 3.1 应用模型不一致

- `app::Exec` 字段在 `projects/APPLaunch/main/ui/Launch.h` 中声明，但构造函数没有赋值。
- `projects/APPLaunch/main/ui/Launch.cpp` 中动态应用去重依赖 `it.Exec == app_exec`，但 `Exec` 实际为空或默认值，导致去重逻辑基本失效。
- `Exec` 被移动进 lambda 捕获后没有保存在模型对象中，后续无法用于诊断、去重、刷新、权限检查。

建议：建立明确的 `AppDescriptor`，至少包含：

```cpp
name
icon
exec
launch_type // InternalPage / Terminal / ExternalProcess
register_app // appending function/factory for built-in apps
source      // Builtin / DesktopFile / Store
config_key
default_enabled
always_on
required_features
```

### 3.2 Settings 与 Launcher 列表不一致

- `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp` 维护一份 `app_keys/app_labels/always_on`。
- `projects/APPLaunch/main/ui/Launch.cpp` 维护另一份实际 Launcher app 列表。
- Settings 中存在 `Music`，但 Launcher 中没有对应入口。
- `Compass` 在 Launcher 中存在，但没有统一受配置控制。
- `Python/Store/CLI/Game/Setting` 的固定/可配置语义与 Settings 展示不完全一致。

建议：Settings 不再维护独立 app 清单，只从 `AppRegistry` 读取 `configurable` 项。

### 3.3 配置变更不会刷新主页模型

- Settings 保存 `app_*` 后没有通知 `Launch`。
- `Launch::applications_reload()` 当前只保留固定段并重扫 `.desktop`，不会重新应用内置 app enable/disable。
- 用户在设置页切换 app 后，主页行为可能不变或需要重启。

建议：保存配置后发布事件或回调，`Launch` 执行：

1. 重建内置 app 列表。
2. 重扫 `.desktop` app。
3. 规范化当前 selection。
4. 刷新 carousel。

### 3.4 外部命令执行边界弱

- `.desktop` 的 `Exec` 从应用目录读取后直接用于终端或外部进程执行。
- Setup 页面大量使用 `system()` / `popen()` 拼接 shell。
- SSID、配置字符串、更新命令等可能进入 shell 命令。
- 阻塞式外部命令会卡住 UI 线程或让恢复路径依赖顺序执行。

相关文件：

- `projects/APPLaunch/main/ui/Launch.cpp`
- `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp`
- `projects/APPLaunch/main/ui/page_app/ui_app_console.hpp`

建议：

- 引入统一进程 API，使用 argv 形式执行，避免 shell 拼接。
- 对 `.desktop Exec` 做 allowlist、owner/permission 校验、路径规范化、最大文件大小/条目数量限制。
- 对 Setup 的网络、时间、更新等动作抽到 platform service。
- 外部命令统一返回结构化错误，并显示到 UI hint/诊断页面。

### 3.5 对象生命周期存在环和裸资源

- `zero_lvgl_os` 持有 `Launch` 和 `UILaunchPage`。
- `Launch` 持有 `std::shared_ptr<UILaunchPage>`。
- `UILaunchPage` 持有 `std::shared_ptr<Launch>`。
- 这形成 `shared_ptr` 环，`Launch::~Launch()` 中 watcher/timer 清理理论上不可达。

相关文件：

- `projects/APPLaunch/main/ui/zero_lvgl_os.cpp`
- `projects/APPLaunch/main/ui/Launch.h`
- `projects/APPLaunch/main/ui/UILaunchPage.h`
- `projects/APPLaunch/main/ui/Launch.cpp`

建议：

- 由 `zero_lvgl_os` 唯一拥有 model/controller/view。
- 子对象之间使用裸观察指针或 `weak_ptr`。
- 明确析构顺序：先停 timer/watch/async，再释放页面，再释放 model。

## 4. P1 中高优先级问题

### 4.1 `Launch` 职责过宽

`Launch` 当前同时承担：

- 内置 app 注册。
- `.desktop` 解析。
- 动态 app 去重。
- 外部进程启动。
- 内部页面启动。
- 当前 selection 状态。
- 目录 watch。
- 回主页和 UI 刷新协调。

建议拆分：

- `AppRegistry`：内置/动态 app 加载、过滤、排序、去重。
- `AppLauncher`：外部进程、终端、内部页面启动。
- `LauncherController`：selection、reload、home navigation。
- `UILaunchPage`：纯 view + input adapter。

### 4.2 平台模型不统一

- 构建层使用 `CONFIG_APPLAUNCH_*`。
- 源码层同时使用 `__linux__`、`HAL_PLATFORM_SDL`、`_WIN32`。
- 交叉编译时宿主 OS 宏、目标平台宏、后端宏容易混淆。

建议建立语义平台宏：

```cpp
APPLAUNCH_TARGET_CP0
APPLAUNCH_BACKEND_SDL
APPLAUNCH_HAS_LORA
APPLAUNCH_HAS_CAMERA
APPLAUNCH_HAS_SYSTEM_UPDATE
APPLAUNCH_HAS_WIFI_CONFIG
```

源码只消费这些产品语义宏，不直接用宿主 OS 判断功能可用性。

### 4.3 构建时生成源码文件

- `projects/APPLaunch/main/SConstruct` 每次构建运行 `generate_page_app_includes.py`。
- 脚本写回 `projects/APPLaunch/main/ui/page_app.h`。
- 构建副作用会造成脏工作区、IDE/静态分析不稳定、并行构建竞争。
- 调用端没有检查 return code/stderr，失败时可能继续使用旧文件。

建议二选一：

1. 将 `page_app.h` 作为源码手动维护，禁止构建改写。
2. 生成到 `build/generated/include`，加入 include path，并使用 `subprocess.run(..., check=True)`。

### 4.4 构建脚本承担依赖下载

- `projects/APPLaunch/SConstruct` 在交叉编译时检查并下载 static BSP 包。
- 构建和依赖获取耦合，影响离线构建、CI 可重复性、供应链校验。

建议：

- 新增 `tools/fetch_sysroot.py` 或 SDK bootstrap 步骤。
- 下载逻辑带 checksum、版本文件、离线缓存。
- SCons 只校验 sysroot 是否存在和版本是否匹配。

### 4.5 平台配置重复严重

多个 `*config_defaults.mk` 中 LVGL、font、freetype、thread、asset path 配置大段重复，仅少量后端/toolchain/NEON 差异。

建议改成：

- `config/base_lvgl.mk`
- `config/base_cp0.mk`
- `config/backend_sdl.mk`
- `config/backend_fbdev.mk`
- `config/host_linux.mk`
- `config/host_windows.mk`
- `config/target_aarch64_cp0.mk`

再由目标配置组合 overlay。

### 4.6 SDK 组件内部被应用构建脚本修改

`main/SConstruct` 直接过滤 `lv_sdl_keyboard.c`，并向 `lvgl_component` 追加 include/definitions。这让 APPLaunch 知道 SDK 组件内部结构，SDK 升级容易破。

建议：

- SDK 暴露正式配置项或 component hook。
- APPLaunch 不直接修改 SDK component 内部 `SRCS/DEFINITIONS`。

## 5. P2 中优先级问题

### 5.1 页面实现 header-only 且体量过大

典型文件：

- `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp`
- `projects/APPLaunch/main/ui/page_app/ui_app_lora.hpp`
- `projects/APPLaunch/main/ui/page_app/ui_app_rec.hpp`
- `projects/APPLaunch/main/ui/page_app/ui_app_console.hpp`

问题：

- UI、状态机、硬件访问、命令执行混在一个文件。
- 编译边界大，改一个页面牵动 `page_app.h` 聚合 include。
- 难以 mock 硬件服务，难以写单元测试。

建议：

- `.hpp` 只保留类声明和小型 inline。
- 复杂实现迁到 `.cpp`。
- 硬件访问迁到 service/platform 层。
- UI 只订阅状态和派发 intent。

### 5.2 首页轮播状态和 LVGL 对象数组强耦合

`UILaunchPage` 使用一个 enum 同时索引 card/title/dot，并在动画中旋转 LVGL 对象数组。UI 对象顺序就是状态机，维护难度高。

建议：

- 抽象 `CarouselModel`：维护当前 index、visible slots、pending switch。
- `UILaunchPage` 只根据 model 渲染 slots。
- 动画层只处理 view transition，不修改应用选择状态。

### 5.3 全局/静态 UI 状态较多

典型状态：

- `ui.cpp` 的全局 `home`。
- `UILaunchPage.cpp` 的 `active_launch_page`。
- `UILaunchPage.cpp` 的全局 `home_input_group`。
- `ui_loading.cpp` / `ui_global_hint.cpp` 的静态对象。

建议：

- 统一归入 `LauncherRuntime` 或 `UiContext`。
- 明确 owner、init、shutdown。
- 测试或热重启时可以完整重置。

### 5.4 页面 timer/动画取消不统一

- 多数页面手动删除 timer，但一致性不足。
- `UIIpPanelPage` 等页面存在析构不清理 timer 的风险。
- 动画回调捕获裸 `lv_obj_t*` 和 lambda，页面删除时缺少统一取消 token。

建议：

- 页面或模块在自身内部直接持有并释放 timer/event/animation 资源。
- 页面析构统一取消回调。
- 回调校验 generation/token，而不是只依赖裸指针有效。

### 5.5 资源命名和路径语义不统一

资源目录中存在大小写、拼写、尺寸后缀混杂：

- `game_100.png` / `gmae.png`
- `e_mail_100.png` / `email.png`
- `unitENV.png` / `unitenv_100.png`
- `compass_needle_80.png`

静态图标使用 `cp0_file_path("xxx.png")`，动态 `.desktop` 的 `Icon` 直接保存字符串，两者路径语义不同。

建议：

- 建立资源 manifest 或 `resources.hpp`。
- 统一命名规则：`snake_case[_size].png`。
- `.desktop Icon` 统一解析为绝对路径或基于 `cp0_file_path` 的资源相对路径。

### 5.6 状态栏和基础布局重复

`UIAppTopBar` 和 `home_base` 各自实现 top/status 区域，时间/WiFi/电池样式和刷新逻辑分叉。

建议：

- 提取单一 `TopBar` 组件。
- 首页和 App 页通过参数配置 logo/title/颜色。
- 电池、时间、WiFi 刷新逻辑复用。

## 6. 安全与可靠性关注点

- `.desktop` 文件解析缺少 owner/permission/大小/数量限制。
- `Exec` 缺少 allowlist 和路径规范化。
- Console 命令按空格拆分，不支持引用/转义，语义与 shell 不一致。
- Setup 页面 shell 拼接风险高。
- `launch_Exec()` 关闭 LVGL timer/input 后没有 RAII guard；异常或卡死时恢复边界弱。
- 主循环无限轮询，无退出信号、无健康状态、无顶层错误隔离。
- 字体未初始化时 `launcher_fonts()` 直接 `abort()`，生产恢复性弱。
- 启动 GIF 对象 pause/load home 后缺少显式删除或空指针清理。

## 7. 测试与可观测性缺口

当前未看到 `projects/APPLaunch` 下存在系统性的 test/spec。

建议优先补最小测试层：

1. `.desktop` parser：合法/非法 key、重复 Exec、Icon 路径、Terminal/Sysplause 解析。
2. `AppRegistry`：平台 feature filter、Settings enable/disable、always-on 规则。
3. Console tokenizer：空命令、参数、路径、错误反馈。
4. Setup 命令构造：SSID 转义、禁止 shell 注入。
5. 页面生命周期：timer 创建/析构、reload、go home。
6. Launch reload：动态应用增删、selection 保持、carousel 刷新。

可观测性建议：

- 统一 `LauncherErrorCode`。
- 所有 process/file/hardware 失败进入同一日志路径。
- UI 增加最近 N 条错误诊断页面。
- 清理 `SLOG*`、`fprintf(stderr)`、`perror` 混用。

## 8. 建议落地路线

### 第一阶段：修一致性和安全边界

1. 修复 `app::Exec` 未赋值问题。
2. 引入 `AppDescriptor` / `AppRegistry`。
3. Settings 和 Launcher 共用同一份 app registry。
4. Settings 保存后通知 Launcher 重建列表。
5. `.desktop Exec` 做基础 allowlist/权限/路径检查。
6. Setup 中高风险 `system()/popen()` 先替换为 argv API 或 service wrapper。

### 第二阶段：修生命周期和构建副作用

1. 打破 `Launch` / `UILaunchPage` 的 `shared_ptr` 环。
2. 目录 watcher/timer 由 `Launch` 内部直接管理，PTY 由 `UIConsolePage` 自行管理。
3. `launch_Exec()` 在外部进程返回后直接恢复 LVGL timer/input/display flag。
4. `page_app.h` 生成到 build 目录，或改为手动维护。
5. `generate_page_app_includes.py` 失败即中断构建。

### 第三阶段：平台和页面架构重构

1. 建立语义平台宏和 feature set。
2. 配置文件改 base + overlay。
3. BSP/sysroot 下载移出 SCons 主构建。
4. 大页面拆分为 model/service/view/controller。
5. LoRa、Camera、Setup 等硬件/系统能力迁到 platform service。

### 第四阶段：资源、布局、测试完善

1. 资源 manifest 化，统一命名。
2. 首页和 App 页共用 `TopBar`。
3. 建立 SDL2 CI 构建和核心单元测试。
4. 增加诊断页面和错误历史。
5. 补充“资源所有权图”“页面生命周期图”“平台能力矩阵”。

## 9. 首批建议修改文件

优先处理：

- `projects/APPLaunch/main/ui/Launch.h`
- `projects/APPLaunch/main/ui/Launch.cpp`
- `projects/APPLaunch/main/ui/page_app/ui_app_setup.hpp`
- `projects/APPLaunch/main/ui/page_app.h`
- `projects/APPLaunch/main/SConstruct`
- `projects/APPLaunch/SConstruct`
- `projects/APPLaunch/main/ui/UILaunchPage.h`
- `projects/APPLaunch/main/ui/UILaunchPage.cpp`

## 10. 结论

APPLaunch 当前最大收益的第一刀不是全面重写，而是先把应用模型统一：`AppRegistry + AppDescriptor + Settings/Launcher 同源 + 配置变更刷新主页`。这一刀可以直接消除用户可见的不一致，也为后续平台拆分、测试和生命周期治理建立核心支点。

## 2026-06-15 zero_lvgl_os 启动职责整理

本轮对 `zero_lvgl_os` 的启动职责做了语义拆分：

- 构造函数继续只负责对象装配：display/theme/fonts、`Launch`、`UILaunchPage`、二者引用关系。
- `start()` 负责运行期 UI 启动，内部拆成：
  - `build_launcher_home()`：创建 LVGL screen、绑定 `Launch` UI、初始化输入 group。
  - `show_initial_screen()`：根据启动动画配置加载首页或启动 GIF。
- 原 `creat_display()` 更正为 `create_display()`。
- 原 `create_launcher_home()` 移除，避免名字误导为单纯构建函数；现在创建和显示职责分开。

验证：

```sh
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -Q -j8 --implicit-deps-changed
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -Q -j8 --implicit-deps-changed
```

结果：均通过。
