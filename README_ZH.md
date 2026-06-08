# launcher

[English](./README.md)

基于 [M5Stack_Linux_Libs](https://github.com/m5stack/M5Stack_Linux_Libs) SDK 开发的 M5CardputerZero 应用集合。该项目展示了如何在 M5CardputerZero（AArch64 Linux）设备上使用 **LVGL 9.5** 构建图形界面应用。

仓库包含两个主要项目，并提供若干示例/辅助项目：
- **HelloWorld** — 基础用户演示程序，展示状态栏和简单 UI
- **APPLaunch** — 应用启动器，提供多应用导航、LoRa 通信、音频播放等丰富功能

UI 界面由 **SquareLine Studio 1.5.0** 生成，支持 SDL2 仿真模式（本机调试）和 Linux Framebuffer 模式（设备运行）两种显示后端。

---

## 项目结构

```
launcher/
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

## 编译程序

项目主要在 Linux 环境下开发，同时兼容跨平台构建。下面是详细编译说明。

### Linux
#### 依赖安装（仅需执行一次）

```bash
sudo apt update
sudo apt install python3 python3-pip libffi-dev libsdl2-dev

pip3 install parse scons requests tqdm
pip3 install setuptools-rust paramiko scp
```

> Python 版本需 ≥ 3.8

#### 克隆项目（含子模块）

```bash
git clone --recursive https://github.com/CardputerZero/launcher.git
cd launcher
```

如果已克隆但未初始化子模块：

```bash
git submodule update --init --recursive
```
进入 `HelloWorld` 进行编译：

#### 设备上编译
```bash
# 进入工程目录
cd projects/HelloWorld
# 清理环境
scons distclean
# 启动 8 线程编译
scons -j8
```

#### 编译 SDL2 模拟器
```bash
# 进入工程目录
cd projects/HelloWorld
# 加载配置项
export CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk
# 清理环境
scons distclean
# 启动 8 线程编译
scons -j8
```
#### SDL2 模拟运行
```bash
cd dist/
./HelloWorld
```


#### 交叉编译
```bash
# 安装交叉编译工具
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
# 进入工程目录
cd projects/HelloWorld
# 加载配置项
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
# 清理环境
scons distclean
# 启动 8 线程编译
scons -j8
```


### macOS
#### 依赖安装（仅需执行一次）

```bash
python3 -m venv launcher-python-venv
source launcher-python-venv/bin/activate
pip3 install parse scons requests tqdm
pip3 install setuptools-rust paramiko scp
```

> Python 版本需 ≥ 3.8

#### 克隆项目（含子模块）

```bash
git clone --recursive https://github.com/CardputerZero/launcher.git
cd launcher
```

如果已克隆但未初始化子模块：

```bash
git submodule update --init --recursive
```
进入 `HelloWorld` 进行编译：


#### 交叉编译
```bash
# 安装交叉编译工具
brew tap messense/macos-cross-toolchains
brew install aarch64-unknown-linux-gnu
# 进入工程目录
cd projects/HelloWorld
# 加载配置项
export CONFIG_DEFAULT_FILE=mac_cross_cp0_config_defaults.mk
# 清理环境
scons distclean
# 启动 8 线程编译
scons -j8
```



### 配置管理命令

```bash
# 查看/修改编译配置（图形化菜单）
scons menuconfig

# 清理编译产物
scons -c

# 彻底清理（含配置缓存）
scons distclean

# 配置 setup.ini 中的主机 IP 和操作命令后，可使用 scons push 将文件传输到设备并执行指定命令。
scons push
```

---

## APPLaunch
该程序是运行在 CardputerZero 设备上的桌面 UI 程序，为 LCD 小屏幕提供基础操作界面。

### 编译
参考 HelloWorld 的编译

### 打包
```bash
# brew install dpkg # mac 需要安装 dpkg 工具
cd projects/APPLaunch/tools
./llm_pack.py
```
该命令会生成一个 DEB 安装包。将安装包通过 `scp` 传输到设备后，可使用 `sudo dpkg -i ./***.deb` 进行安装。

### 运行

#### 自动运行
使用 `dpkg` 安装 DEB 包后，程序会通过 `systemd` 服务 `APPLaunch.service` 自动启动。

安装后使用 `systemd` 命令进行查看和操作：
```bash
pi@pi:~ $ sudo systemctl status APPLaunch.service 
● APPLaunch.service - APPLaunch Service
     Loaded: loaded (/usr/lib/systemd/system/APPLaunch.service; enabled; preset: enabled)
     Active: active (running) since Mon 2026-06-08 15:58:19 CST; 23min ago
 Invocation: aa5c4e3ca94742deb2fe0dc67467e670
   Main PID: 664 (M5CardputerZero)
      Tasks: 7 (limit: 448)
        CPU: 1min 19.265s
     CGroup: /system.slice/APPLaunch.service
             └─664 /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch

Jun 08 15:58:19 pi systemd[1]: Started APPLaunch.service - APPLaunch Service.
```

#### 手动运行
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


### UI 界面说明

界面由 SquareLine Studio 1.5.0 设计生成，分辨率为 **320×170**（ST7789V 屏幕）。

### APPLaunch 界面

| 区域 | 内容 |
|------|------|
| 顶部状态栏 | Logo、时钟时间、电量百分比、WiFi 信息 |
| 主内容区 | 应用轮播页面（左右翻页导航） |
| 全局提示 | ESC / Shift / SYM 快捷键提示覆盖层 |

内置应用页面包括：Stock、Music、Camera、LoRa、SSH、GPIO、MIDI、Console、Mesh、Gallery、Email、File、Hack、Rec、Setup、Store、UnitEnv、IpPanel、LovyanGFX、HikePod、Tank Battle 等。


## 相关资源

- [M5Stack_Linux_Libs SDK](https://github.com/m5stack/M5Stack_Linux_Libs)
- [LVGL 文档](https://docs.lvgl.io/)
- [SquareLine Studio](https://squareline.io/)
- [M5CardputerZero 产品页](https://docs.m5stack.com/)
