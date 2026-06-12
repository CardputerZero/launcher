# M5CardputerZero APPLaunch App 打包指南

## 目录

- [1. 概述](#1-概述)
- [2. .desktop 快捷方式文件](#2-desktop-快捷方式文件)
  - [2.1 字段说明](#21-字段说明)
  - [2.2 完整示例](#22-完整示例)
  - [2.3 安装方式](#23-安装方式)
- [3. Debian 软件包结构](#3-debian-软件包结构)
  - [3.1 各文件说明](#31-各文件说明)
- [4. 打包步骤](#4-打包步骤)
- [5. 快速部署（仅注册快捷方式）](#5-快速部署仅注册快捷方式)
- [6. 常见问题](#6-常见问题)

---

<!-- SECTION: overview -->
## 1. 概述

M5CardputerZero APPLaunch 是运行在设备上的应用启动器。它在启动时会扫描目录
`/usr/share/APPLaunch/applications/` 下所有以 `.desktop` 结尾的文件，并将其
作为可选 App 添加到主界面的滑动列表中。

因此，**为 APPLaunch 添加一个新 App 有两种方式**：

| 方式 | 适合场景 |
|------|----------|
| 直接在设备上放置 `.desktop` 文件 | 快速验证、临时添加 |
| 制作 Debian 软件包（`.deb`）并安装 | 正式发布、持久安装 |

> APPLaunch 主程序安装在 `/usr/share/APPLaunch/`，
> 工作目录也为 `/usr/share/APPLaunch/`，
> 因此 `.desktop` 文件中的 **相对路径均以该目录为根**。

<!-- SECTION: desktop_file -->
## 2. .desktop 快捷方式文件

APPLaunch 使用类 XDG Desktop Entry 格式的 `.desktop` 文件来描述一个 App。
文件必须放置于设备的 `/usr/share/APPLaunch/applications/` 目录下，且文件名
**必须以 `.desktop` 结尾**（例如 `myapp.desktop`）。

<!-- SECTION: desktop_fields -->
### 2.1 字段说明

文件须包含 `[Desktop Entry]` 节头，APPLaunch 识别的字段如下：

| 字段 | 是否必填 | 类型 | 说明 |
|------|----------|------|------|
| `Name` | **必填** | 字符串 | App 在启动器列表中显示的名称 |
| `Exec` | **必填** | 字符串 | 要执行的命令或可执行文件路径 |
| `Icon` | 可选 | 字符串 | 图标路径（相对于 `/usr/share/APPLaunch/`，或绝对路径） |
| `Terminal` | 可选 | `true`/`false` | 是否在内置终端（UIConsolePage）中运行，默认 `false` |
| `Sysplause` | 可选 | `true`/`false` | 终端运行结束后是否显示"按任意键返回"提示，默认 `true` |
| `Type` | 可选 | 字符串 | 固定填写 `Application`（供工具链识别，APPLaunch 本身不校验） |
| `TryExec` | 可选 | 字符串 | 仅用于文档说明，APPLaunch 不解析此字段 |

> **注意：** 字段解析时会自动去除 key 和 value 两端的空格/Tab；
> 空行和以 `#` 或 `;` 开头的行作为注释被跳过。

#### `Terminal` 与 `Sysplause` 的行为说明

- `Terminal=false`（默认）：APPLaunch 通过 `fork` + `execlp` 直接启动外部程序，
  程序运行期间启动器暂停刷新；程序退出后自动返回主界面。
  长按 Home 键 5 秒可发送 `SIGINT`，再等待 3 秒未退出则发送 `SIGKILL`。
- `Terminal=true`：APPLaunch 在内置终端界面（UIConsolePage）中运行命令，
  支持键盘输入/输出显示。
- `Sysplause=true`（默认，仅在 `Terminal=true` 时生效）：命令结束后显示
  "Press any key to return..."，等待用户确认再返回主界面。
- `Sysplause=false`：命令结束后立即返回主界面。

<!-- SECTION: desktop_example -->
### 2.2 完整示例

**示例 1 – 在终端中运行 vim（随 APPLaunch 一同提供的模板）**

```ini
[Desktop Entry]
Name=Vim
TryExec=vim
Exec=vim
Terminal=true
Icon=share/images/email.png
Type=Application
```

**示例 2 – 直接启动一个 GUI 程序（不使用内置终端）**

```ini
[Desktop Entry]
Name=Calculator
Exec=/home/pi/M5CardputerZero-Calculator-linux-aarch64
Terminal=false
Icon=share/images/math.png
Type=Application
```

**示例 3 – 在终端中运行脚本，结束后立即返回**

```ini
[Desktop Entry]
Name=MyScript
Exec=/home/pi/my_script.sh
Terminal=true
Sysplause=false
Icon=share/images/hack.png
Type=Application
```

**示例 4 – 使用系统命令（bash 内置，需完整路径或 PATH 可达）**

```ini
[Desktop Entry]
Name=Python3
Exec=python3
Terminal=true
Icon=share/images/python.png
Type=Application
```

<!-- SECTION: install_desktop -->
### 2.3 安装方式

**方式 A：直接拷贝到设备（SSH）**

```bash
# 在开发机上执行
scp myapp.desktop pi@<device-ip>:/usr/share/APPLaunch/applications/

# 重启 APPLaunch 服务使其生效
ssh pi@<device-ip> "sudo systemctl restart APPLaunch.service"
```

**方式 B：在设备上直接创建**

```bash
sudo tee /usr/share/APPLaunch/applications/myapp.desktop > /dev/null << 'EOF'
[Desktop Entry]
Name=MyApp
Exec=/home/pi/myapp
Terminal=false
Icon=share/images/email.png
Type=Application
EOF

sudo systemctl restart APPLaunch.service
```

<!-- SECTION: deb_structure -->
## 3. Debian 软件包结构

若需将 App 以 Debian 软件包形式发布，打包目录需遵循以下固定结构：

```
debian-<AppName>/
├── DEBIAN/
│   ├── control          # 包元数据
│   ├── postinst         # 安装后脚本（启动服务）
│   └── prerm            # 卸载前脚本（停止服务）
└── usr/
    └── share/
        └── APPLaunch/              # 主程序安装根目录（WorkingDirectory）
            ├── applications/
            │   └── myapp.desktop   # App 快捷方式（必须以 .desktop 结尾）
            ├── bin/
            │   └── M5CardputerZero-demo   # 主程序可执行文件
            ├── lib/
            │   └── lvgl.so         # 动态库
            └── share/
                ├── font/
                │   └── *.ttf       # 字体文件
                └── images/
                    └── *.png       # 图标 / 图片资源
```

打包命令：
```bash
dpkg-deb -b debian-<AppName> <package_name>_<version>-<revision>_arm64.deb
# 示例：
dpkg-deb -b debian-example example_0.1-m5stack1_arm64.deb
```

<!-- SECTION: deb_files -->

### 3.1 各文件说明

#### `DEBIAN/control`

包的元数据描述文件，格式如下：

```
Package: applaunch
Version: 0.1
Architecture: arm64
Maintainer: yourname <you@example.com>
Original-Maintainer: m5stack <m5stack@m5stack.com>
Section: APPLaunch
Priority: optional
Homepage: https://www.m5stack.com
Packaged-Date: 2026-04-27 12:03:35
Description: M5CardputerZero APPLaunch
```

| 字段 | 说明 |
|------|------|
| `Package` | 包名（小写，无空格） |
| `Version` | 版本号，如 `0.1` |
| `Architecture` | 固定填 `arm64`（M5CardputerZero 平台） |
| `Maintainer` | 维护者信息 |
| `Section` | 分类，填 `APPLaunch` |

> `WorkingDirectory=/usr/share/APPLaunch` 决定了 `.desktop` 文件中
> 相对路径（如 `share/images/email.png`）的基准目录。

#### `usr/share/APPLaunch/applications/<name>.desktop`

注册到 APPLaunch 的 App 快捷方式，格式见第 2 节。
文件名必须以 `.desktop` 结尾。

#### `usr/share/APPLaunch/share/images/*.png`

供 `.desktop` 文件 `Icon` 字段引用的图标文件。
引用示例：`Icon=share/images/myapp.png`

#### `usr/share/APPLaunch/share/font/*.ttf`

APPLaunch UI 使用的字体文件（随主程序一起发布）。

<!-- SECTION: build_deb -->
## 4. 打包步骤

---

### 4.2 手动打包步骤

若不使用脚本，可按以下步骤手动操作：

**步骤 1：创建目录结构**

```bash
PKG=debian-myapp
mkdir -p $PKG/DEBIAN
mkdir -p $PKG/lib/systemd/system
mkdir -p $PKG/usr/share/APPLaunch/bin
mkdir -p $PKG/usr/share/APPLaunch/lib
mkdir -p $PKG/usr/share/APPLaunch/share/font
mkdir -p $PKG/usr/share/APPLaunch/share/images
mkdir -p $PKG/usr/share/APPLaunch/applications
```

**步骤 2：放入主程序和资源文件**

```bash
cp /path/to/myapp   $PKG/usr/share/APPLaunch/bin/
cp /path/to/share/font/*.ttf             $PKG/usr/share/APPLaunch/share/font/
cp /path/to/share/images/*.png           $PKG/usr/share/APPLaunch/share/images/
touch $PKG/usr/share/APPLaunch/lib/lvgl.so   # 占位，实际为动态库
```

**步骤 3：添加 App 快捷方式**

```bash
cat > $PKG/usr/share/APPLaunch/applications/myapp.desktop << 'EOF'
[Desktop Entry]
Name=MyApp
Exec=/usr/share/APPLaunch/bin/myapp
Terminal=false
Icon=share/images/myapp.png
Type=Application
EOF
```

**步骤 4：编写控制文件**

```bash
cat > $PKG/DEBIAN/control << 'EOF'
Package: MyApp
Version: 0.1
Architecture: arm64
Maintainer: yourname <you@example.com>
Section: MyApp
Priority: optional
Homepage: https://www.m5stack.com
Description: M5CardputerZero MyApp
EOF
```



**步骤 7：设置可执行权限**

```bash
chmod 755 $PKG/usr/share/APPLaunch/bin/myapp
```

**步骤 8：打包**

```bash
dpkg-deb -b $PKG myapp_0.1-m5stack1_arm64.deb
```

**步骤 9：部署到设备**

```bash
scp myapp_0.1-m5stack1_arm64.deb pi@<device-ip>:/tmp/
ssh pi@<device-ip> "sudo dpkg -i /tmp/myapp_0.1-m5stack1_arm64.deb"
```

---

<!-- SECTION: quick_deploy -->
## 5. 快速部署（仅注册快捷方式）

如果 App 可执行文件已经存在于设备上，只需要在 APPLaunch 中添加一个入口，
可以跳过完整打包流程，直接操作：

```bash
# 1. SSH 登录设备
ssh pi@<device-ip>

# 2. 在 applications 目录创建 .desktop 文件
sudo tee /usr/share/APPLaunch/applications/myapp.desktop > /dev/null << 'EOF'
[Desktop Entry]
Name=MyApp
Exec=/home/pi/myapp
Terminal=false
Icon=share/images/email.png
Type=Application
EOF

# 3. 可选：上传自定义图标
# （在开发机执行）
scp myapp_icon.png pi@<device-ip>:/usr/share/APPLaunch/share/images/

# 4. 重启 APPLaunch 使新 App 生效
sudo systemctl restart APPLaunch.service
```

> **提示：** APPLaunch 每次启动时才扫描 `applications/` 目录，
> 因此也可以不重启服务，直接重启设备或重启 APPLaunch 进程来加载新 App。

---

<!-- SECTION: faq -->
## 6. 常见问题

**Q1：`.desktop` 文件放好了，但 App 没有出现在列表中。**

- 检查文件名是否以 `.desktop` 结尾（如 `myapp.desktop`，不能是 `myapp.desktop.temple`）。
- 检查文件是否包含 `[Desktop Entry]` 节头，且 `Name` 和 `Exec` 字段均不为空。
- 执行 `sudo systemctl restart APPLaunch.service` 重启服务后再观察。
- 查看日志：`sudo journalctl -u APPLaunch.service -f`

**Q2：图标显示为空白或默认图标。**

- 确认 `Icon` 字段的路径正确。相对路径以 `/usr/share/APPLaunch/` 为根，
  例如 `Icon=share/images/myapp.png` 对应实际路径
  `/usr/share/APPLaunch/share/images/myapp.png`。
- 也可以使用绝对路径，例如 `Icon=/home/pi/myapp_icon.png`。

**Q3：`Terminal=true` 的 App 运行后键盘无响应。**

- 确认设备键盘驱动正常工作，可在 CLI（bash）中测试输入。
- 检查程序是否需要特殊终端环境（如 `ncurses`），APPLaunch 内置终端为简单的
  伪终端（pty），基本 I/O 均可正常工作。

**Q4：如何强制退出正在运行的外部 App（`Terminal=false`）？**

- 长按 Home 键保持 5 秒，APPLaunch 会向 App 发送 `SIGINT`。
- 若 App 在 3 秒内未响应 `SIGINT`，APPLaunch 将发送 `SIGKILL` 强制终止。

**Q5：如何卸载已安装的 APPLaunch deb 包？**

```bash
sudo dpkg -r applaunch
```

**Q6：打包时报错 `dpkg-deb: error: failed to open package info file`。**

- 检查 `DEBIAN/control` 文件格式，确保字段名后紧跟 `:` 和空格，
  且文件末尾有换行符。
- 确认 `DEBIAN/postinst` 和 `DEBIAN/prerm` 文件权限为 `755`。

**Q7：`.deb` 包的命名规则是什么？**

```
{package_name}_{version}-{revision}_{architecture}.deb
# 示例：
applaunch_0.1-m5stack1_arm64.deb
```

| 部分 | 说明 |
|------|------|
| `package_name` | 与 `DEBIAN/control` 中 `Package` 字段一致，小写 |
| `version` | 软件版本，如 `0.1` |
| `revision` | 打包修订版本，如 `m5stack1` |
| `architecture` | 固定为 `arm64` |
