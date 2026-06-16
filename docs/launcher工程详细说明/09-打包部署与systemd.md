# 09 - 打包部署与 systemd

本章说明 APPLaunch 如何从 `dist` 目录打包为 Debian `.deb`，如何部署到 M5CardputerZero，如何通过 systemd 自启动，以及如何验证和排查部署问题。

所有命令默认从仓库根目录开始：

```bash
cd /home/nihao/w2T/github/launcher
```

## 1. 部署形态总览

APPLaunch 的设备端运行依赖两类文件：

1. 主程序：`M5CardputerZero-APPLaunch`。
2. 运行时资源树：`APPLaunch/`，包含应用描述、字体、图片、音频、脚本和可选子应用。

正式安装后的目标路径是：

```text
/usr/share/APPLaunch/
├── applications/
├── bin/
│   ├── M5CardputerZero-APPLaunch
│   ├── M5CardputerZero-AppStore              # 如果 dist/bin 中存在则打包
│   ├── M5CardputerZero-Calculator            # 如果 dist/bin 中存在则打包
│   └── appstore.py                           # 如果 dist/bin 中存在则打包
├── lib/
├── share/
│   ├── font/
│   └── images/
└── cache -> /var/cache/APPLaunch             # postinst 创建
```

systemd 服务文件安装到：

```text
/usr/lib/systemd/user/APPLaunch.service
```

服务启动命令是：

```text
/usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
```

工作目录是：

```text
/usr/share/APPLaunch
```

当前包把 APPLaunch 安装为 UID 1000 用户的 systemd user service。手动检查服务时，可以以该用户登录后执行 `systemctl --user ...`，或在 `runuser`/SSH 自动化中设置 `XDG_RUNTIME_DIR=/run/user/1000`。

## 2. 打包前必须先完成设备目标构建

`.deb` 应该使用 arm64 设备产物，而不是 Linux SDL2 x86_64 仿真产物。

推荐在 Linux x86_64 开发机交叉编译：

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
file dist/M5CardputerZero-APPLaunch
```

`file` 结果应包含：

```text
ARM aarch64
```

如果看到 `x86-64`，说明你打包的是 SDL2 本机产物，不能安装到设备作为正式 launcher。

设备端原生编译也可以用于打包：

```bash
cd /home/pi/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=config_defaults.mk
scons -j2
file dist/M5CardputerZero-APPLaunch
```

## 3. `debian_packager.py` 通用打包脚本说明

仓库级打包脚本位于：

```text
scripts/debian_packager.py
```

它替代原来 APPLaunch 项目内的打包脚本，便于 `projects/` 下其他项目复用同一套 Debian 打包流程。APPLaunch 仍然是默认目标，所以不带参数运行时仍会打包 APPLaunch。

关键默认值和参数：

| 参数 / 默认值 | 值 | 说明 |
| --- | --- | --- |
| `--project` | `APPLaunch` | `projects/` 下的项目名，也可以传项目路径 |
| `--package-name` | `applaunch` | Debian 包名 |
| `--app-name` | `APPLaunch` | 安装到 `/usr/share` 下的应用名，也是 systemd 服务名 |
| `--bin-name` | `M5CardputerZero-APPLaunch` | 主可执行文件名 |
| `--src` / `--src-folder` | `dist` | 构建输出目录，相对项目目录解析 |
| `--app-tree` | 自动 | 运行时资源树，默认依次查找 `<project>/<app-name>`、`<src>/<app-name>` |
| `--output-dir` | `<project>/tools` | `.deb` 输出目录 |
| `--work-dir` | 输出目录 | 打包临时目录所在目录 |
| `--builder` | `auto` | 有 `dpkg-deb` 时使用 `dpkg-deb`，否则使用纯 Python 写包器 |

APPLaunch 默认生成的包文件名格式：

```text
applaunch_0.2.1-m5stack1_arm64.deb
```

## 4. `.deb` 包目录结构

使用默认 APPLaunch 参数运行脚本后，会在 `projects/APPLaunch/tools` 下生成临时目录：

```text
projects/APPLaunch/tools/debian-APPLaunch/
├── DEBIAN/
│   ├── control
│   ├── postinst
│   └── prerm
└── usr/
    ├── lib/
    │   └── systemd/
    │       └── user/
    │           └── APPLaunch.service
    └── share/
        └── APPLaunch/
            ├── applications/
            ├── bin/
            │   └── M5CardputerZero-APPLaunch
            ├── lib/
            └── share/
                ├── audio/
                ├── font/
                └── images/
```

APPLaunch 最终 `.deb` 文件位于：

```text
projects/APPLaunch/tools/applaunch_0.2.1-m5stack1_arm64.deb
```

## 5. 打包命令

### 5.1 安装打包工具

`debian_packager.py` 只依赖 Python 也可以生成 `.deb`。如果系统安装了 `dpkg-deb`，默认 `--builder auto` 会优先使用它。

Linux 开发机：

```bash
sudo apt update
sudo apt install -y dpkg-dev fakeroot
```

macOS 可以直接使用 Python 写包器，也可以通过 Homebrew 安装 `dpkg`：

```bash
brew install dpkg
```

### 5.2 执行 APPLaunch 打包

从仓库根目录运行：

```bash
python3 scripts/debian_packager.py
```

等价的完整参数写法：

```bash
python3 scripts/debian_packager.py build \
  --project APPLaunch \
  --package-name applaunch \
  --app-name APPLaunch \
  --bin-name M5CardputerZero-APPLaunch
```

成功时会看到类似：

```text
Creating Debian package applaunch_0.2.1-m5stack1_arm64.deb ...
Staged package tree: .../projects/APPLaunch/tools/debian-APPLaunch
Debian package created: .../projects/APPLaunch/tools/applaunch_0.2.1-m5stack1_arm64.deb
Builder: dpkg-deb
```

### 5.3 指定自定义版本

```bash
python3 scripts/debian_packager.py build --version 0.2.2 --revision m5stack2
```

给其他项目复用时，需要覆盖项目元数据和主程序名：

```bash
python3 scripts/debian_packager.py build \
  --project Calculator \
  --package-name calculator \
  --app-name Calculator \
  --bin-name M5CardputerZero-Calculator \
  --src dist \
  --app-tree share
```

其中 `--app-tree` 应指向需要安装为 `/usr/share/<app-name>` 的资源树。

### 5.4 清理打包产物

脚本支持：

```bash
python3 scripts/debian_packager.py clean
python3 scripts/debian_packager.py distclean
```

差异：

| 命令 | 行为 |
| --- | --- |
| `clean` | 删除 `projects/APPLaunch/tools` 下默认 APPLaunch 的 `*.deb` 和 `debian-APPLaunch` |
| `distclean` | 在 `clean` 基础上额外删除 `projects/APPLaunch/tools` 下旧版 `m5stack_*` 输出 |

同样可以给清理命令传 `--project`、`--project-dir`、`--app-name` 和 `--output-dir`，用于非默认项目。

## 6. 打包脚本复制规则

### 6.1 主程序查找

脚本从 `src_folder` 查找主程序；默认 APPLaunch 目标下，`src_folder` 是相对 `projects/APPLaunch` 的 `dist`。

查找顺序：

1. `projects/APPLaunch/dist/M5CardputerZero-APPLaunch`
2. `projects/APPLaunch/dist/bin/M5CardputerZero-APPLaunch`

如果两个位置都不存在，会抛出：

```text
PackError: binary M5CardputerZero-APPLaunch not found in <project>/dist or <project>/dist/bin
```

### 6.2 附加应用和后端

脚本会尝试包含以下可选文件：

```text
projects/APPLaunch/dist/bin/M5CardputerZero-AppStore
projects/APPLaunch/dist/bin/appstore.py
projects/APPLaunch/dist/bin/M5CardputerZero-Calculator
```

如果存在则复制到：

```text
/usr/share/APPLaunch/bin/
```

其中非 `.py` 文件会设置为 `0755`。

### 6.3 资源树复制

脚本优先复制源码中的资源树：

```text
projects/APPLaunch/APPLaunch
```

目标是包内：

```text
usr/share/APPLaunch
```

如果源码资源树不存在，则尝试使用：

```text
projects/APPLaunch/dist/APPLaunch
```

这意味着打包时通常不只依赖 `dist/APPLaunch`，也会把工程源码目录中的 `APPLaunch/` 资源树复制进去。

### 6.4 AppStore 图片补充

如果存在：

```text
projects/AppStore/share/images
```

脚本会把以下图片复制到包内 `usr/share/APPLaunch/share/images`：

```text
store_wordmark.png
store_arrow_*.png
```

## 7. Debian 控制脚本

### 7.1 `DEBIAN/control`

打包脚本生成的 control 包含：

```text
Package: applaunch
Version: 0.2.1
Architecture: arm64
Maintainer: dianjixz <dianjixz@m5stack.com>
Original-Maintainer: m5stack <m5stack@m5stack.com>
Section: APPLaunch
Priority: optional
Homepage: https://www.m5stack.com
Packaged-Date: <打包时间>
Description: M5CardputerZero APPLaunch
```

重要点：

- `Architecture` 固定为 `arm64`。
- 脚本不自动声明 `Depends`，因此依赖库需要由基础镜像提供，或在后续版本中补充依赖声明。

### 7.2 `DEBIAN/postinst`

安装后脚本执行：

```sh
mkdir -p /var/cache/APPLaunch
ln -sfn /var/cache/APPLaunch /usr/share/APPLaunch/cache
APP_UID=1000
APP_USER="$(getent passwd "$APP_UID" | cut -d: -f1)"
loginctl enable-linger "$APP_USER" || true
systemctl start "user@$APP_UID.service" || true
runuser -u "$APP_USER" -- env XDG_RUNTIME_DIR="/run/user/$APP_UID" \
  DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$APP_UID/bus" \
  systemctl --user enable APPLaunch.service || true
runuser -u "$APP_USER" -- env XDG_RUNTIME_DIR="/run/user/$APP_UID" \
  DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$APP_UID/bus" \
  systemctl --user restart APPLaunch.service || true
exit 0
```

作用：

- 创建可写缓存目录 `/var/cache/APPLaunch`。
- 在只读/系统资源目录下建立 `cache` 软链接。
- 为 UID 1000 用户启用 linger，并启用/启动 systemd user service。

注意：当前通用打包脚本使用 `ln -sfn`，重复安装时可以安全刷新缓存链接。

### 7.3 `DEBIAN/prerm`

卸载前脚本执行：

```sh
APP_UID=1000
APP_USER="$(getent passwd "$APP_UID" | cut -d: -f1)"
runuser -u "$APP_USER" -- env XDG_RUNTIME_DIR="/run/user/$APP_UID" \
  DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$APP_UID/bus" \
  systemctl --user stop APPLaunch.service || true
runuser -u "$APP_USER" -- env XDG_RUNTIME_DIR="/run/user/$APP_UID" \
  DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$APP_UID/bus" \
  systemctl --user disable APPLaunch.service || true
rm -rf /var/cache/APPLaunch
exit 0
```

作用：

- 停止服务。
- 禁用开机自启动。
- 删除缓存目录。

注意：卸载会删除 `/var/cache/APPLaunch`，其中若存有运行时缓存或应用商店缓存，会一并清除。

## 8. systemd 服务文件

脚本生成：

```ini
[Unit]
Description=APPLaunch Service
After=pipewire-pulse.service
Wants=pipewire-pulse.service

[Service]
ExecStart=/usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
WorkingDirectory=/usr/share/APPLaunch
Restart=always
RestartSec=1
StartLimitInterval=0

[Install]
WantedBy=default.target
```

字段说明：

| 字段 | 说明 |
| --- | --- |
| `ExecStart` | 启动 APPLaunch 主程序 |
| `WorkingDirectory` | 设置当前目录为 `/usr/share/APPLaunch`，方便相对路径访问 |
| `Restart=always` | 进程退出后总是重启 |
| `RestartSec=1` | 退出 1 秒后重启 |
| `StartLimitInterval=0` | 关闭默认启动频率限制，避免频繁崩溃后 systemd 停止重启 |
| `After` / `Wants` | 在可用时等待 PipeWire PulseAudio 支持 |
| `WantedBy=default.target` | enable 后随用户默认 target 启动 |

当前包安装的是 `/usr/lib/systemd/user` 下的用户服务，不是 root 身份的 system service。`postinst` 会为 UID 1000 用户启用该服务；framebuffer、evdev、GPIO、音频和相机访问权限需由系统镜像的用户/用户组规则提供。

## 9. 安装到设备

### 9.1 复制 `.deb` 到设备

假设设备 IP 是 `192.168.28.177`，用户名是 `pi`：

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch/tools
scp applaunch_0.2.1-m5stack1_arm64.deb pi@192.168.28.177:/home/pi/
```

### 9.2 在设备上安装

```bash
ssh pi@192.168.28.177
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
```

如果安装过程中提示缺少依赖，先修复依赖：

```bash
sudo apt-get -f install
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
```

### 9.3 覆盖安装

再次安装同名或更高版本包：

```bash
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
```

如果服务正在运行，`postinst` 会尝试 enable/start。为了减少安装期间 framebuffer 或输入设备占用问题，可以手动先停服务：

```bash
systemctl --user stop APPLaunch.service || true
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
systemctl --user restart APPLaunch.service
```

## 10. 使用 `scons push` 快速部署

除了 `.deb`，工程还支持通过 `setup.ini` 上传 `dist` 目录。

配置文件：

```text
projects/APPLaunch/setup.ini
```

默认内容示例：

```ini
[ssh]
local_file_path = dist
remote_file_path = /home/pi/dist
remote_host = 192.168.28.177
remote_port = 22
username = pi
password = pi
; before_cmd = 'echo pi |  sudo -S systemctl --user stop APPLaunch.service'
; after_cmd = 'echo pi |  sudo -S systemctl --user stop APPLaunch.service; echo pi |  sudo -S cp /home/pi/dist/M5CardputerZero-APPLaunch /usr/share/APPLaunch/bin ; echo pi |  sudo -S systemctl --user start APPLaunch.service'
```

执行：

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons push
```

`SDK/tools/scons/push.py` 会：

1. 读取 `setup.ini`。
2. 遍历 `local_file_path` 下所有文件。
3. 计算本地 MD5。
4. 通过 SSH 获取远端文件 MD5。
5. 只上传有变化的文件。
6. 可选执行 `before_cmd` 和 `after_cmd`。

适用场景：

- 开发阶段快速替换 `dist`。
- 快速上传单次编译结果。
- 不需要测试 Debian 安装脚本时。

不适用场景：

- 验证正式安装路径。
- 验证 `postinst`、`prerm`。
- 验证 systemd enable/install 行为。
- 需要生成可分发安装包。

## 11. 手动部署方式

当不想使用 `.deb`，也不想使用 `scons push`，可以手动复制。

在开发机上传：

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scp dist/M5CardputerZero-APPLaunch pi@192.168.28.177:/home/pi/
scp -r dist/APPLaunch pi@192.168.28.177:/home/pi/APPLaunch-new
```

在设备上安装：

```bash
systemctl --user stop APPLaunch.service || true
sudo mkdir -p /usr/share/APPLaunch/bin
sudo install -m 0755 /home/pi/M5CardputerZero-APPLaunch /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
sudo rsync -a --delete /home/pi/APPLaunch-new/ /usr/share/APPLaunch/
sudo mkdir -p /var/cache/APPLaunch
sudo ln -sfn /var/cache/APPLaunch /usr/share/APPLaunch/cache
systemctl --user daemon-reload
systemctl --user restart APPLaunch.service
```

如果服务文件尚未安装，可以手动创建 `/usr/lib/systemd/user/APPLaunch.service`，内容参考第 8 节。

## 12. 部署验证命令

### 12.1 包状态

```bash
dpkg -l | grep applaunch
dpkg -s applaunch
```

查看包安装了哪些文件：

```bash
dpkg -L applaunch
```

查看 `.deb` 包内容但不安装：

```bash
dpkg-deb -c applaunch_0.2.1-m5stack1_arm64.deb
```

查看 `.deb` 元信息：

```bash
dpkg-deb -I applaunch_0.2.1-m5stack1_arm64.deb
```

### 12.2 文件和权限

```bash
ls -l /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
file /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
ls -ld /usr/share/APPLaunch
ls -l /usr/share/APPLaunch/cache
ls -l /var/cache/APPLaunch
find /usr/share/APPLaunch/share/images -maxdepth 1 -type f | head
find /usr/share/APPLaunch/share/font -maxdepth 1 -type f | head
```

期望：

- 主程序有执行权限。
- 主程序架构为 `ARM aarch64`。
- `/usr/share/APPLaunch/cache` 指向 `/var/cache/APPLaunch`。
- 图片和字体资源存在。

### 12.3 动态库依赖

在设备上：

```bash
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
```

检查是否有缺失：

```bash
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch | grep 'not found' || true
```

如果缺少库，需要安装对应系统包，或补充打包规则把私有库放入 `/usr/share/APPLaunch/lib` 并配置运行时搜索路径。

### 12.4 systemd 状态

```bash
systemctl --user status APPLaunch.service --no-pager
systemctl --user is-enabled APPLaunch.service
systemctl --user is-active APPLaunch.service
```

查看日志：

```bash
journalctl --user -u APPLaunch.service -b --no-pager
journalctl --user -u APPLaunch.service -b -f
```

重启：

```bash
systemctl --user restart APPLaunch.service
```

停止：

```bash
systemctl --user stop APPLaunch.service
```

开机自启动：

```bash
systemctl --user enable APPLaunch.service
```

取消开机自启动：

```bash
systemctl --user disable APPLaunch.service
```

重新读取服务文件：

```bash
systemctl --user daemon-reload
```

### 12.5 手动前台运行

排查 systemd 前，建议先前台运行：

```bash
systemctl --user stop APPLaunch.service || true
cd /usr/share/APPLaunch
sudo ./bin/M5CardputerZero-APPLaunch
```

这样可以直接看到标准输出和崩溃信息。如果前台运行正常但 systemd 不正常，再检查服务文件、权限和工作目录。

### 12.6 framebuffer 和输入设备

检查 framebuffer：

```bash
ls -l /dev/fb*
cat /sys/class/graphics/fb0/name 2>/dev/null || true
```

检查输入设备：

```bash
ls -l /dev/input/
cat /proc/bus/input/devices
```

检查当前谁占用 framebuffer 或输入设备：

```bash
sudo fuser -v /dev/fb0 2>/dev/null || true
sudo fuser -v /dev/input/event* 2>/dev/null || true
```

如果另一个图形程序正在运行，APPLaunch 可能无法正确显示或读取输入。

## 13. 卸载和回滚

### 13.1 卸载

```bash
sudo dpkg -r applaunch
```

这会触发 `prerm`：停止服务、disable 服务、删除 `/var/cache/APPLaunch`。

如果要同时清理配置文件：

```bash
sudo dpkg -P applaunch
```

### 13.2 安装旧包回滚

```bash
systemctl --user stop APPLaunch.service || true
sudo dpkg -i /home/pi/applaunch_旧版本-m5stack1_arm64.deb
systemctl --user restart APPLaunch.service
```

验证：

```bash
dpkg -s applaunch | grep Version
systemctl --user status APPLaunch.service --no-pager
```

### 13.3 临时禁用 launcher

```bash
systemctl --user disable --now APPLaunch.service
```

恢复：

```bash
systemctl --user enable --now APPLaunch.service
```

## 14. 常见部署错误

### 14.1 安装时报 `package architecture (arm64) does not match system`

原因：设备系统不是 arm64，或在 x86_64 开发机上直接安装了 arm64 包。

处理：

```bash
uname -m
dpkg --print-architecture
```

`.deb` 应安装在 M5CardputerZero 设备上，而不是 Linux x86_64 开发机上。

### 14.2 设备运行时报 `Exec format error`

原因：主程序架构错误。常见情况是把 Linux SDL2 x86_64 产物打进了 arm64 包。

检查：

```bash
file /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
```

正确处理：重新交叉编译：

```bash
cd projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
```

然后重新打包安装。

### 14.3 服务反复重启

检查：

```bash
systemctl --user status APPLaunch.service --no-pager
journalctl --user -u APPLaunch.service -b --no-pager | tail -n 100
```

常见原因：

- 缺少动态库。
- 资源路径不存在。
- framebuffer 或输入设备不可用。
- 程序启动即崩溃。
- 安装的是错误架构产物。

进一步检查：

```bash
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch | grep 'not found' || true
ls /usr/share/APPLaunch/share/images
ls /dev/fb0
```

### 14.4 `ln: failed to create symbolic link '/usr/share/APPLaunch/cache': File exists`

原因：旧版安装包使用非幂等的 `ln -s` 创建缓存链接，目标已存在。

处理：

```bash
sudo rm -rf /usr/share/APPLaunch/cache
sudo mkdir -p /var/cache/APPLaunch
sudo ln -sfn /var/cache/APPLaunch /usr/share/APPLaunch/cache
systemctl --user restart APPLaunch.service
```

当前通用打包脚本已经写入 `ln -sfn`；重新打包并安装即可持久修复。

### 14.5 `dpkg-deb: error: failed to open package info file .../DEBIAN/control`

原因：打包目录结构不完整，或脚本中途失败后残留目录异常。

处理：

```bash
cd /home/nihao/w2T/github/launcher
python3 scripts/debian_packager.py clean
python3 scripts/debian_packager.py
```

### 14.6 `Binary M5CardputerZero-APPLaunch not found in .../dist`

原因：未构建，或构建目录不是 `projects/APPLaunch/dist`。

处理：

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
ls -l dist/M5CardputerZero-APPLaunch
cd /home/nihao/w2T/github/launcher
python3 scripts/debian_packager.py
```

### 14.7 服务启动后黑屏

排查顺序：

1. 确认可执行文件能前台运行。
2. 确认 framebuffer 存在。
3. 确认没有其它进程占用显示。
4. 确认资源路径存在。
5. 查看 journal 日志。

命令：

```bash
systemctl --user stop APPLaunch.service || true
cd /usr/share/APPLaunch
sudo ./bin/M5CardputerZero-APPLaunch
ls -l /dev/fb0
sudo fuser -v /dev/fb0 2>/dev/null || true
journalctl --user -u APPLaunch.service -b --no-pager | tail -n 100
```

### 14.8 外部应用无法启动

APPLaunch 会从资源树和 `.desktop` 描述中找到外部应用。先检查：

```bash
find /usr/share/APPLaunch/applications -maxdepth 1 -type f -print
find /usr/share/APPLaunch/bin -maxdepth 1 -type f -print
```

确认外部应用有执行权限：

```bash
ls -l /usr/share/APPLaunch/bin
```

如果 `.desktop` 中的 Exec 指向不存在的路径，需要修正资源树或重新打包。

## 15. 发布前检查清单

打包前：

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
file dist/M5CardputerZero-APPLaunch
```

打包后：

```bash
cd /home/nihao/w2T/github/launcher
python3 scripts/debian_packager.py
dpkg-deb -I projects/APPLaunch/tools/applaunch_0.2.1-m5stack1_arm64.deb
dpkg-deb -c projects/APPLaunch/tools/applaunch_0.2.1-m5stack1_arm64.deb | head -n 50
```

安装后：

```bash
dpkg -s applaunch | grep -E 'Package|Version|Architecture'
systemctl --user status APPLaunch.service --no-pager
systemctl --user is-enabled APPLaunch.service
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch | grep 'not found' || true
ls -l /usr/share/APPLaunch/cache
journalctl --user -u APPLaunch.service -b --no-pager | tail -n 100
```

功能验证：

- 设备开机后 APPLaunch 自动显示首页。
- 键盘/按键输入可用。
- 首页应用轮播可切换。
- 资源图片和字体正常显示。
- 内置页面可进入和返回。
- 外部应用启动后能退出并回到 APPLaunch。
- AppStore/Calculator 等可选子应用如已打包，能从 launcher 正常启动。

## 16. 推荐部署流程

正式发布建议使用：

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
file dist/M5CardputerZero-APPLaunch
cd /home/nihao/w2T/github/launcher
python3 scripts/debian_packager.py
scp projects/APPLaunch/tools/applaunch_0.2.1-m5stack1_arm64.deb pi@192.168.28.177:/home/pi/
ssh pi@192.168.28.177 'sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb && systemctl --user status APPLaunch.service --no-pager'
```

开发阶段快速替换建议使用：

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
scons push
```

两者区别：`.deb` 验证完整安装和 systemd 生命周期；`scons push` 更快，但不能替代正式打包验证。
