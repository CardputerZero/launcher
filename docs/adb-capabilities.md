# ADB 能做什么：功能优势、完整能力、与安卓的差距

本文回答三件事：
1. ADB 相比串口 Bridge / WiFi 在**功能上**强在哪；
2. ADB **到底能有哪些功能**（完整清单），在 CardputerZero(Debian) 上哪些能用；
3. 和**安卓**还差哪些、为什么；
4. 附：我们为 `adb shell` 补上的**自动补全 + 完善终端**。

---

## 1. ADB 的功能优势（vs 串口 / WiFi）

| 能力 | 串口 Bridge | WiFi(scp/ssh) | **ADB** |
|------|:-----------:|:-------------:|:-------:|
| 传文件 push/pull | ✅(自研) | ✅ | ✅ 原生 |
| 远程 shell | ❌ | ✅(ssh) | ✅ `adb shell` |
| **一条链路多路复用**（同时 shell+传输+转发） | ❌ | ⚠️(多条 TCP) | ✅ 协议内建 |
| **端口转发** `forward`/`reverse` | ❌ | ⚠️(需 ssh -L/-R) | ✅ 原生 |
| 设备发现/状态机 `wait-for-device` | ❌ | ❌ | ✅ |
| 多设备管理 `-s <serial>` | ❌ | ⚠️ | ✅ |
| 二进制安全直出 `exec-out` | ⚠️ | ✅ | ✅ |
| 退出码透传 | ⚠️ | ✅ | ✅ |
| 主机工具通用性 | ❌(自研脚本) | ✅ | ✅(人人有 adb) |
| 免网络（纯 USB） | ✅ | ❌ | ✅ |
| 速度 | ~12 MB/s | ~0.5 MB/s | **~38 MB/s** |

**三个最实用的杀手锏：**

- **多路复用**：插一根线，同时开 shell、推文件、跑端口转发，互不打架。
- **端口转发**（最被低估）：把设备/主机端口隧道到对面，纯 USB、无需 WiFi：

  ```bash
  # 把设备的 22 端口转发到本机 2222 -> 直接 USB 上 ssh 进设备（不走 wifi）
  adb forward tcp:2222 tcp:22
  ssh -p 2222 eggfly@localhost

  # 设备访问本机的服务（反向）：设备上的 localhost:8000 == 你电脑的 8000
  adb reverse tcp:8000 tcp:8000
  ```

- **设备状态机**：`adb wait-for-device`、`get-state` 让脚本/CI 能可靠地「等设备就绪再继续」。

---

## 2. ADB 完整能力清单 & 在本设备(Debian)上的可用性

ADB 的命令大致分四类。下表标注在 **CardputerZero（Debian，非安卓）** 上是否可用：

### 2.1 传输层 / 设备管理 — ✅ 基本全可用

| 命令 | 作用 | 本设备 |
|------|------|:------:|
| `adb devices [-l]` | 列设备 | ✅ |
| `adb -s <serial> …` | 指定设备 | ✅ |
| `adb get-state` / `get-serialno` | 状态/序列号 | ✅ |
| `adb wait-for-device` | 等待就绪 | ✅ |
| `adb kill-server` / `start-server` | 主机端守护进程 | ✅ |
| `adb reconnect` | 重连 | ✅ |

### 2.2 Shell / 文件 — ✅ 全可用

| 命令 | 作用 | 本设备 |
|------|------|:------:|
| `adb shell` | 交互式 shell（已支持补全，见 §4） | ✅ root |
| `adb shell <cmd>` | 一次性命令（带退出码） | ✅ |
| `adb exec-out <cmd>` | 二进制安全输出（如导出文件流） | ✅ |
| `adb push / pull` | 推/拉文件、目录（递归） | ✅ |
| `adb sync` | 同步目录 | ⚠️(面向 Android 分区，一般用 push) |

### 2.3 网络 / 端口 — ✅ 可用

| 命令 | 作用 | 本设备 |
|------|------|:------:|
| `adb forward` | 主机端口 → 设备端口 | ✅ |
| `adb reverse` | 设备端口 → 主机端口 | ✅ |
| `adb tcpip <port>` | 切到 TCP 模式（ADB over WiFi） | ✅ |
| `adb connect <ip:port>` | 通过网络连 adbd | ✅ |

### 2.4 安卓专属 — ❌ 本设备不可用

| 命令 | 作用 | 为什么用不了 |
|------|------|--------------|
| `adb install` / `uninstall` | 装/卸 APK | 没有 Android 包管理器(PackageManager) |
| `adb shell pm / am` | 包管理 / 启动 Activity | 没有 Android framework(system_server) |
| `adb logcat` | 安卓日志环形缓冲 | 没有 Android logd（用 `journalctl` 代替） |
| `adb bugreport` / `dumpsys` | 系统诊断 | 依赖 Android 服务 |
| `adb shell screencap/screenrecord` | 截屏/录屏 | 安卓二进制（可用 `fbgrab`/ffmpeg 抓 framebuffer 代替） |
| `adb sideload` | recovery 刷机 | 没有安卓 recovery |
| `adb backup` / `restore` | 应用数据备份 | 安卓机制 |
| `adb root` / `unroot` / `remount` | 重启 adbd 提权 / 重挂分区 | 本来就是 root；分区是普通 Linux fs |
| `adb reboot bootloader/recovery` | 进 fastboot/recovery | 树莓派无这些模式（普通 `adb reboot` ✅） |
| `adb jdwp` | Java 调试 | 安卓 Dalvik/ART 专用 |

> 一句话：**传输、shell、文件、端口转发**这套「通用 Linux 远程」能力全有；
> **依赖 Android framework / 分区 / 日志系统 / 包管理**的命令没有。

---

## 3. 和安卓到底差哪些（根因）

ADB 协议本身（传输、流复用、shell、forward）是**与平台无关**的，这部分两边一样。差距全来自
**安卓系统服务**，CardputerZero 是 Debian，没有这些：

| 缺的东西 | 导致哪些 adb 功能没有 |
|----------|------------------------|
| Android framework（`system_server`、PackageManager、ActivityManager） | `install` / `pm` / `am` / `dumpsys` |
| Android 日志系统（`logd`/`logcat`） | `logcat`（用 `journalctl -f` 代替） |
| Android 属性系统 + framework 鉴权 socket | **屏幕上的「允许 USB 调试」弹窗**（详见 adb-debug-guide §4） |
| Android 分区布局 + fastboot/recovery | `sideload` / `reboot bootloader` / `remount` |
| ART/Dalvik 运行时 | `jdwp` / 应用级调试 |
| 一批安卓专用二进制 | `screencap` / `screenrecord` / `bugreport` |

反过来说：CardputerZero 是**完整的 Debian**，上面那些「缺失」大多有等价的 Linux 原生替代
（`apt` 代替 `install`，`journalctl` 代替 `logcat`，`systemctl` 代替 `am/pm`，
`ssh -p` over `adb forward` 代替很多东西）。

---

## 4. 我们补上的：`adb shell` 自动补全 + 完善终端

**问题**：adbd 硬编码执行 `/bin/sh`（在 Debian 上是 **dash**），dash 没有 readline，所以
`adb shell` 进去**没有 Tab 补全、方向键变成 `^[[A` 这种乱码、没有历史**。

**原理**：交互式 `adb shell` 启动的是**登录 shell**（`-/bin/sh`），会读 `~/.profile`；而
`adb shell <命令>` 走的是 `sh -c`（**不**读 `.profile`）。所以我们在 `/root/.profile` 里，
仅对**交互式登录 shell** 做 `exec /bin/bash`，命令模式不受影响、脚本仍跑高速 dash。

`cardputer-adb enable` 会自动配置（无需手动）：

- 确保安装 `bash` + `bash-completion`；
- 向 `/root/.profile` 注入：交互式时 `exec /bin/bash`；
- 向 `/root/.bashrc` 注入：`TERM` 兜底、加载 bash-completion、历史设置、彩色提示符。

**实测**（真机 PTY 驱动）：

```text
root@CardputerZero:/#                # 彩色提示符，readline 已激活
$ echo BASH=$BASH  ->  BASH=/bin/bash
$ ls -d /etc/hostnam<Tab>  ->  ls -d /etc/hostname     # ✅ Tab 补全
$ <↑>  ->  召回上一条命令 echo MARKER_ABC               # ✅ 方向键/历史
```

要进一步增强（可选）：装 `tmux`、把默认编辑器设为 `nano/vim`、或在 `.bashrc` 里加常用 alias。
