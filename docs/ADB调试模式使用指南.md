# CardputerZero ADB 调试模式（USB）使用指南

CardputerZero 可以通过 USB‑C 口对外提供标准的 **ADB（Android Debug Bridge）**
接口，和 Arduino UNO Q 一样。开启后，你可以在任意电脑上用普通的 `adb` 命令获取
root shell、推拉文件、做端口转发——**完全不需要 Wi‑Fi**。

> English version: [`adb-debug-guide.md`](adb-debug-guide.md)。

---

## 1. 这是什么（与安卓的区别）

| | CardputerZero |
|---|---|
| 传输层 | dwc2 USB 控制器上的 FunctionFS（`ffs.adb`，bulk 端点） |
| 守护进程 | Debian 官方 `adbd` 包（源码即 AOSP） |
| 主机工具 | 标准 `adb`（Android platform‑tools） |
| 开关 | **设置 → Developer → ADB**（等价于安卓的「USB 调试」总开关） |

可用：`adb shell`、`adb push`、`adb pull`、`adb forward`、`adb reverse`。
不可用（这是 Debian 不是安卓）：`adb install`、`pm`、`am`、`logcat`。

`adb push` 实测约 **38 MB/s**（约为旧版 CDC‑ACM 串口桥的 3 倍）。

---

## 2. 在设备上开启

1. 首次使用时，先按第 4 节通过已有 SSH 或设备本地终端授权至少一个主机公钥。
2. 打开 **设置**（齿轮图标）。
3. 滚动到 **Developer** 菜单，按 **OK/→** 进入。
4. 把 **ADB** 切到 **O**（开）。

> 当前版本未在设置页面开放主机公钥配对界面。如果还没有已授权主机，
> 直接开启会失败；请先使用 `cardputer-adb authorize`。

开启时后台会自动完成：

- 若未安装则安装 `adbd` 包；
- 确认 `/adb_keys` 中至少有一个已授权的主机公钥；
- 把 USB 设备名美化为 **CardputerZero**（见 §5）；
- 确保 USB 控制器处于 **peripheral（外设）** 模式；
- 启用并启动 `adbd.service`（重启后也会自动恢复）。

### 关于「重启」提示

dwc2 USB 控制器既能当 **host（主机）** 也能当 **peripheral（外设）**，ADB 需要外设
模式。如果当前不是外设模式，开关会修改 `config.txt` 并弹出 **「Reboot?」** 提示——
选 **Yes** 即可生效。重启后 ADB 会自动起来，之后不会再提示。

如果设备本来就是外设模式（出厂配置后的常态），开启会**立即生效、无需重启**。

---

## 3. 在电脑上使用

先确保有 `adb`：

- macOS：`brew install android-platform-tools`（或用 Android SDK 的 `platform-tools`）。
- Windows/Linux：安装 Android SDK platform‑tools。

插上 USB‑C 线后：

```bash
adb devices
# List of devices attached
# CardputerZero-xxxxxxxx   device

adb shell                              # 设备上的 root shell
adb push ./file.bin /home/eggfly/      # 推送文件到设备
adb pull /home/eggfly/log.txt .        # 从设备拉取文件
adb forward tcp:8080 tcp:80            # 把设备端口转发到本机
```

> macOS 首次连接会弹出系统的 **「允许配件连接？」** 对话框，点 **允许** 即可。这是
> macOS 的 USB 安全机制，和 ADB 无关，且会按设备记忆。

---

## 4. 授权 & 「允许密钥弹窗」问题

安卓上插到新电脑会弹出 **「允许 USB 调试？」** 的屏幕对话框，点*允许*来信任那台电脑的
密钥。

**Debian 自带的 `adbd` 没有这个屏幕上的逐密钥授权弹窗**——它背后没有 Android
framework，无法弹窗。所以 CardputerZero 采用更适合 Linux 设备的密钥模型：

- **设置里的开关就是安全闸门**：ADB 默认关闭，不开就没有任何监听（就像安卓的
  「USB 调试」总开关）。
- **没有默认共享公钥**。必须先明确授权至少一台主机，ADB 才能开启。
- **要信任某台电脑的密钥**，在主机上查看公钥，再通过设备本地终端或已有 SSH 连接授权：

  ```bash
  # 在你的电脑上打印公钥
  cat ~/.android/adbkey.pub
  # 在设备上（首次授权需使用本地终端或已有 SSH）
  sudo /usr/share/APPLaunch/adb/cardputer-adb authorize "<粘贴上面那一行公钥>"
  ```

> 说明：当前 Debian `adbd` 在没有 framework 授权 socket 时会打印
> `authentication not required`，即开启期间会接受任意主机连接。请把 **ADB 开关本身**
> 当作信任边界，用完记得关掉。

---

## 5. 设备名（「CardputerZero」）

`adb devices` 显示的是 *serial（序列号）*。Debian adbd 默认用 machine‑id 的 SHA‑256
派生，长这样：`c7ead0dd7fb4384c...`。CardputerZero 用更友好的 USB 字符串描述符替换：

| USB 描述符 | 值 |
|------------|-----|
| manufacturer | `M5Stack` |
| product | `CardputerZero` |
| serialnumber | `CardputerZero-<8 位十六进制>`（短、稳定、每台唯一） |

于是 `adb devices` 显示如 `CardputerZero-c7ead0dd`，macOS 配件弹窗显示
「M5Stack CardputerZero」。

要改名，编辑 `cardputer-adb-gadget`（`MANUFACTURER` / `PRODUCT` 变量）后把 ADB
开关关再开。

---

## 6. 关闭

设置 → Developer → 把 **ADB** 切到 **X**。这会立即停止并禁用
`adbd.service`、拆除 USB gadget。如果 USB 控制器当前仍处于 peripheral 模式，
脚本会返回退出码 `10`；需重启后才会恢复 host/hub 模式及板载 USB Hub。

---

## 7. 命令行工具（进阶）

开关其实调用了一个 device 端脚本，你也可以直接运行：

```bash
sudo /usr/share/APPLaunch/adb/cardputer-adb enable      # 开启（退出码 10 => 需要重启）
sudo /usr/share/APPLaunch/adb/cardputer-adb disable     # 关闭（退出码 10 => 需要重启）
     /usr/share/APPLaunch/adb/cardputer-adb status      # 查看 USB/adbd/enabled 及授权状态
sudo /usr/share/APPLaunch/adb/cardputer-adb authorize <公钥|文件>
sudo /usr/share/APPLaunch/adb/cardputer-adb revoke <64位 SHA-256 指纹>
sudo /usr/share/APPLaunch/adb/cardputer-adb clear-authorizations
sudo /usr/share/APPLaunch/adb/cardputer-adb migrate     # 包升级内部使用：移除旧版共享密钥
```

退出码：`0` 表示成功；`10` 表示配置已更新、但 USB 模式需重启后才生效；
`11` 表示尚未授权任何主机；`1` 表示错误。`status` 会输出
`peripheral`、`adbd`、`enabled`、`authorizations` 以及每个授权项的指纹和标签。

安装的文件：

| 路径 | 作用 |
|------|------|
| `/usr/share/APPLaunch/adb/cardputer-adb` | ADB 开关、状态和主机授权管理 |
| `/usr/share/APPLaunch/adb/cardputer-adb-gadget` | 品牌化 USB gadget（名字/序列号） |
| `/etc/systemd/system/adbd.service.d/cardputer.conf` | 把品牌 gadget 接入 `adbd.service` 的 drop‑in |
| `/adb_keys` | adbd 读取的授权公钥列表 |

---

## 8. 故障排查

| 现象 | 处理 |
|------|------|
| `adb devices` 为空 | 确认 ADB 开关已开；`adb kill-server` 重试；用 USB **数据线**直连 CM0（不要走 Hub）。 |
| 设备显示 `unauthorized` | 授权你的主机公钥（见 §4）。 |
| 每次开关都提示重启 | 没能进入外设模式；检查 `config.txt` 的 `[all]` 段是否有 `dtoverlay=dwc2,dr_mode=peripheral`。 |
| 和旧串口桥冲突 | 单个 USB 控制器一次只能挂一个 gadget；开启 ADB 会自动停掉旧的 PiBridge 串口 gadget。 |
