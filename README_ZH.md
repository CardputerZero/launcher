# M5CardputerZero-UserDemo

[English](./README.md)

基于 [M5Stack_Linux_Libs](https://github.com/m5stack/M5Stack_Linux_Libs) SDK 开发的 M5CardputerZero 应用集合。该项目展示了如何在 M5CardputerZero（AArch64 Linux）设备上使用 **LVGL 9.5** 构建图形界面应用。

仓库包含两个主要项目：
- **UserDemo** — 基础用户演示程序，展示状态栏和简单 UI
- **APPLaunch** — 应用启动器，提供多应用导航、LoRa 通信、音频播放等丰富功能

UI 界面由 **SquareLine Studio 1.5.0** 生成，支持 SDL2 仿真模式（本机调试）和 Linux Framebuffer 模式（设备运行）两种显示后端。

---

## 项目结构

```
M5CardputerZero-UserDemo/
├── SDK/                        # M5Stack_Linux_Libs SDK（git submodule）
│   ├── components/             # 组件库（lvgl_component、DeviceDriver 等）
│   ├── examples/               # SDK 示例程序
│   └── tools/                  # 编译工具链脚本（SCons）
├── projects/
│   ├── UserDemo/               # 基础用户演示项目
│   ├── APPLaunch/              # 应用启动器项目（核心）
│   │   ├── SConstruct          # 项目顶层编译脚本
│   │   ├── config_defaults.mk  # 默认编译配置（Linux Framebuffer 模式）
│   │   ├── darwin_config_defaults.mk  # macOS 编译配置
│   │   ├── setup.ini           # SSH 部署配置
│   │   └── main/               # 主程序源码
│   │       ├── SConstruct      # 组件编译脚本
│   │       ├── src/
│   │       │   └── main.cpp    # 程序入口
│   │       ├── include/        # 公共头文件（battery、keyboard_input 等）
│   │       ├── hal/            # 硬件抽象层
│   │       │   ├── sdl/        # SDL2 平台实现（PC 调试）
│   │       │   └── linux/      # Linux 平台实现（设备端）
│   │       └── ui/             # UI 代码
│   │           ├── ui.h / ui.c # UI 初始化
│   │           ├── screens/    # 屏幕定义
│   │           ├── components/ # 自定义组件（应用页面、启动器等）
│   │           ├── widgets/    # 自定义控件（轮播组件等）
│   │           ├── Animation/  # 动画效果
│   │           ├── fonts/      # 字体资源
│   │           └── images/     # 图片资源
│   ├── Calculator/             # 计算器
│   ├── AppStore/               # 应用商店
│   └── HelloWorld/             # Hello World 示例
├── ext_components/             # 外部组件（Miniaudio 等）
├── doc/                        # 文档资源
├── README.md
└── README_ZH.md
```

---

## 功能特性

### 通用特性

- 基于 LVGL 9.5 的图形界面
- 支持两种显示后端：
  - **SDL2**：用于 PC 端仿真调试（默认编译模式）
  - **Linux Framebuffer（ST7789V）**：用于 M5CardputerZero 设备端运行
- evdev 键盘/触摸输入支持（设备端）
- 使用 SCons + Kconfig 构建系统，可通过 `scons menuconfig` 灵活配置

### APPLaunch 特性

- 应用启动器界面，支持多应用导航（轮播翻页）
- 内置应用页面：Stock、Music、Camera、LoRa、SSH、GPIO、MIDI、Console、Mesh 等
- LoRa 通信（基于 RadioLib SX1262）
- 音频播放（基于 Miniaudio）
- 电池状态监控与显示
- 全局快捷键提示（ESC / Shift / SYM）
- 应用进程锁定管理（长按 Home 键强制关闭应用）
- 多线程任务池（C-Thread-Pool）
- 支持 macOS 交叉编译

### UserDemo 特性

- 状态栏显示：M5Stack Zero Logo、时钟时间、电量百分比
- 主内容区：应用名称 + 页面内容占位

---

## 环境准备

### 依赖安装（仅需执行一次）

```bash
sudo apt update
sudo apt install python3 python3-pip libffi-dev libsdl2-dev

pip3 install parse scons requests tqdm
pip3 install setuptools-rust paramiko scp
```

> Python 版本需 ≥ 3.8

### 克隆项目（含子模块）

```bash
git clone --recursive https://github.com/dianjixz/M5CardputerZero-UserDemo.git
cd M5CardputerZero-UserDemo
```

如果已克隆但未初始化子模块：

```bash
git submodule update --init --recursive
```

---

## 编译

### APPLaunch 项目

APPLaunch 是主要的应用启动器项目，支持三种编译模式：

#### 方式一：SDL2 仿真模式（Linux x86 PC 调试，默认）

在 x86_64 主机上默认使用 SDL2 后端，直接编译即可在 PC 上运行：

```bash
cd projects/APPLaunch
scons -j$(nproc)
```

编译产物位于 `projects/APPLaunch/dist/` 目录下，可执行文件名为 `M5CardputerZero-APPLaunch`。

#### 方式二：交叉编译（AArch64，部署到 M5CardputerZero 设备）

需要安装 AArch64 交叉编译工具链：

```bash
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

通过环境变量启用交叉编译，SConstruct 会自动切换为 Framebuffer 后端并下载 BSP 静态库：

```bash
cd projects/APPLaunch
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j$(nproc)
```

#### 方式三：macOS 交叉编译

在 macOS 上交叉编译到 AArch64，需安装 `aarch64-linux-gnu-` 工具链（如通过 Homebrew）：

```bash
cd projects/APPLaunch
export CONFIG_DEFAULT_FILE=darwin_config_defaults.mk
scons -j$(nproc)
```

#### 自定义编译配置

可通过 `CONFIG_DEFAULT_FILE` 环境变量指定不同的编译配置文件：

```bash
export CONFIG_DEFAULT_FILE=custom_config_defaults.mk
scons -j$(nproc)
```

### UserDemo 项目

#### 方式一：SDL2 仿真模式（PC 本机调试，默认）

`projects/UserDemo/SConstruct` 中默认启用 SDL2 后端，直接编译即可在 PC 上运行：

```bash
cd projects/UserDemo
scons -j$(nproc)
```

编译产物位于 `projects/UserDemo/build/` 目录下，可执行文件名为 `M5CardputerZero-UserDemo`。

#### 方式二：交叉编译（AArch64，部署到 M5CardputerZero 设备）

需要安装 AArch64 交叉编译工具链：

```bash
# 方法1：通过 apt 安装
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

```

修改 `projects/UserDemo/SConstruct`，将 SDL2 后端切换为 Framebuffer 后端：

```python
# 1、
# 直接编译为x86的sdl后端，可在x86主机上模拟运行

# 2、
export CardputerZero=y 
#交叉编译到 CardputerZero

# 3、
# 在 CardputerZero 上编译时，会直接编译到 CardputerZero 可用的程序
```

然后编译：

```bash
cd projects/UserDemo
scons -j$(nproc)
```

### 配置管理命令

```bash
# 查看/修改编译配置（图形化菜单）
scons menuconfig

# 清理编译产物
scons -c

# 彻底清理（含配置缓存）
scons distclean

# 详细编译输出
scons verbose
```

---

## 运行

### APPLaunch

#### PC 端（SDL2 仿真）

```bash
cd projects/APPLaunch
./dist/M5CardputerZero-APPLaunch
```

#### M5CardputerZero 设备端

通过 SSH 部署（配置见 `projects/APPLaunch/setup.ini`）：

```bash
# 编译后自动部署到设备（需要配置 setup.ini 中的设备 IP 和密码）
# 部署后会自动重启 APPLaunch.service 服务
```

或手动推送：

```bash
scp -r projects/APPLaunch/dist user@<device_ip>:/home/user/
```

在设备上运行（使用 Framebuffer）：

```bash
# 自动检测 ST7789V Framebuffer 设备
./dist/M5CardputerZero-APPLaunch

# 或手动指定 Framebuffer 设备
export LV_LINUX_FBDEV_DEVICE=/dev/fb0
./dist/M5CardputerZero-APPLaunch

# 指定键盘输入设备
export LV_LINUX_KEYBOARD_DEVICE=/dev/input/by-path/platform-3f804000.i2c-event
./dist/M5CardputerZero-APPLaunch
```

### UserDemo

#### PC 端（SDL2 仿真）

```bash
cd projects/UserDemo
./dist/M5CardputerZero-UserDemo
```

#### M5CardputerZero 设备端

将编译产物推送到设备：

```bash
scp -r projects/UserDemo/dist user@<device_ip>:/home/user/
```

在设备上运行（使用 Framebuffer）：

```bash
# 自动检测 ST7789V Framebuffer 设备
./dist/M5CardputerZero-UserDemo

# 或手动指定 Framebuffer 设备
export LV_LINUX_FBDEV_DEVICE=/dev/fb0
./dist/M5CardputerZero-UserDemo

# 指定键盘输入设备
export LV_LINUX_KEYBOARD_DEVICE=/dev/input/by-path/platform-3f804000.i2c-event
./dist/M5CardputerZero-UserDemo
```

---

## UI 界面说明

界面由 SquareLine Studio 1.5.0 设计生成，分辨率为 **320×170**（ST7789V 屏幕）。

### APPLaunch 界面

| 区域 | 内容 |
|------|------|
| 顶部状态栏 | Logo、时钟时间、电量百分比、WiFi 信息 |
| 主内容区 | 应用轮播页面（左右翻页导航） |
| 全局提示 | ESC / Shift / SYM 快捷键提示覆盖层 |

内置应用页面包括：Stock、Music、Camera、LoRa、SSH、GPIO、MIDI、Console、Mesh、Gallery、Email、File、Hack、Rec、Setup、Store、UnitEnv、IpPanel、LovyanGFX、HikePod、Tank Battle 等。

### UserDemo 界面

| 区域 | 内容 |
|------|------|
| 左上角 | M5Stack Zero Logo 图标 |
| 右上角（时钟） | 时钟图标 + 当前时间标签 |
| 右上角（电量） | 电量图标 + 电量百分比标签 |
| 顶部中央 | 应用名称（APPName） |
| 主内容区 | 应用页面内容（320×143 区域） |

如需修改 UI，可用 SquareLine Studio 打开项目后重新生成 `projects/APPLaunch/main/ui/` 或 `projects/UserDemo/main/ui/` 目录下的代码。

---

## 相关资源

- [M5Stack_Linux_Libs SDK](https://github.com/m5stack/M5Stack_Linux_Libs)
- [LVGL 文档](https://docs.lvgl.io/)
- [SquareLine Studio](https://squareline.io/)
- [M5CardputerZero 产品页](https://docs.m5stack.com/)
