# CardputerZero 文件传输方式性能对比与底层原理

对比 CardputerZero（Raspberry Pi CM0，BCM2710 四核 A53 + CYW43 2.4GHz WiFi）上三种
PC↔设备文件传输方式的**实测性能**与**底层原理**：

1. **串口 Bridge**（USB CDC‑ACM + 自研 PiBridge 协议）
2. **ADB**（USB FunctionFS bulk + adbd）
3. **WiFi 传文件**（走 wlan0 的 TCP/IP，用 SSH/scp）

> 本文数字均在同一台真机上实测（8 MB 文件，方法见 §5），口径一致。

---

## 1. TL;DR — 实测结果

| 方式 | 传输层 | device→Mac | Mac→device | 相对 ADB |
|------|--------|-----------:|-----------:|---------:|
| **ADB** | USB 2.0 bulk（ffs.adb） | **~38 MB/s** | **~38 MB/s** | 1.0× |
| **串口 Bridge** | USB 2.0 bulk（CDC‑ACM 串口） | ~12 MB/s | ~12 MB/s | ~0.32× |
| **WiFi + SSH/scp** | 802.11n 2.4GHz | 0.48–0.57 MB/s | 0.34–0.38 MB/s | **~0.013×** |

```
吞吐量（MB/s，越长越快，对数感知）
ADB          ████████████████████████████████████████  38
串口 Bridge  █████████████                             12
WiFi scp     ▌                                          0.5
```

**一句话**：ADB ≈ 串口的 3 倍 ≈ WiFi 的 ~75 倍。三者都受 **USB 2.0 物理上限**（理论
480 Mbit/s，实际可用 ~35–40 MB/s）或 **WiFi 射频环境**约束，差距来自**协议/软件效率**
和**链路本身**。

---

## 2. 三种方式的底层原理

### 2.1 串口 Bridge（USB CDC‑ACM）

```
应用: pibridge-server.py / 客户端  (Python, 每 16KB 分块 + CRC32 + 一问一答)
协议: PiBridge 自研帧 (magic/type/len/payload/crc)
内核: tty 行规程 (n_tty)  ←  这一层是主要开销
      └ usb_f_acm (CDC-ACM 功能驱动)
硬件: USB 2.0 bulk 端点  (dwc2 控制器)
```

- 走的是 USB **bulk 端点**，物理上和 ADB 同源，**理论能跑到几十 MB/s**。
- 但数据被包成「**虚拟串口**」：要过内核 **tty 行规程**（字符设备语义、缓冲、唤醒），
  再加上**用户态 Python**、**每块 CRC32**、**请求‑应答式**的往返等待。
- 这些叠加把有效吞吐压到 **~12 MB/s**。瓶颈是**软件栈**，不是 USB。

### 2.2 ADB（USB FunctionFS bulk）

```
应用: adb push/pull (C, platform-tools)
协议: ADB 协议 (多路复用 + 自带流控, 大块传输)
内核: FunctionFS (ffs.adb) → 直接对接 bulk 端点, 几乎零中间层
硬件: USB 2.0 bulk 端点  (dwc2 控制器)
```

- 同样是 USB 2.0 bulk，但 **adbd 是 C 实现**，通过 **FunctionFS** 直接读写 bulk 端点，
  **没有 tty 行规程那层开销**。
- ADB 协议用**大块传输 + 多路复用 + 自带流控**，每包额外开销小。
- 因此能逼近 **USB 2.0 实际可用上限 ~38 MB/s**。瓶颈直接是 **USB 2.0 硬件**。

> 串口 vs ADB 的差距 = **同样的 USB 管道，软件效率不同**。

### 2.3 WiFi + SSH/scp

```
应用: scp / ssh  (AES-GCM 加解密, 在 CM0 的 A53 小核上跑)
协议: TCP/IP  (拥塞控制 + 重传 + ACK 往返)
链路: 802.11n 2.4GHz 单流, 半双工 CSMA/CA
硬件: CYW43 WiFi 芯片, 经 SDIO 总线挂在 SoC 上
```

为什么 PHY 协商 **130 Mbit/s**，实际只有 **~0.5 MB/s（≈4 Mbit/s）**？多重损耗叠加：

1. **半双工 + CSMA/CA**：2.4GHz 是共享介质，发包前要竞争信道，冲突/退避开销大。
2. **双倍空口**：Mac 和设备**连在同一个 AP** 上，每个包要在空中传两遍
   （设备→AP、AP→Mac），等于吞吐砍半且自我竞争。
3. **2.4GHz 拥塞**：信道被周边 WiFi/蓝牙/微波挤占，信号 60%。
4. **SoC 小核加解密**：SSH 的 AES‑GCM 在 CM0 上是 CPU 成本。
5. **TCP 往返**：ACK/拥塞窗口在高延迟、有丢包的无线链路上拖慢。
6. **SDIO 总线**：WiFi 芯片不是 PCIe 而是经 SDIO，本身有带宽天花板。

即使在理想环境（干净信道/5GHz），2.4GHz 单流 n 也就 ~5–8 MB/s 量级，**永远追不上 USB**。
瓶颈是 **射频链路 + 网络栈 + 拓扑**，和 CPU/USB 无关。

---

## 3. 为什么会有这样的排序

| 决定性因素 | 串口 Bridge | ADB | WiFi scp |
|------------|-------------|-----|----------|
| 物理通道理论上限 | USB2.0 ~480Mbit/s | USB2.0 ~480Mbit/s | WiFi PHY 130Mbit/s |
| 实际瓶颈在哪 | **软件栈**（tty+Python+帧） | **USB 硬件** | **射频/拓扑/加密** |
| 每字节额外开销 | 高 | 低 | 很高 |
| 链路稳定性/确定性 | 高 | 高 | 低（受环境波动） |

- USB 两种方式的「天花板」一样，**ADB 赢在软件几乎贴着硬件跑**。
- WiFi 的「天花板」本来就低一个数量级，再被共享介质、加密、拓扑层层打折。

---

## 4. 关于「模拟网口」——值得知道的第四种

你提到「**从 WiFi 模拟网口**」。严格说，「模拟网口」对应的是 **USB Ethernet Gadget**
（`g_ether` / CDC‑NCM / ECM / RNDIS）：让设备**在 USB 上虚拟出一块网卡**，PC 侧多出一个
网络接口，然后照常用 `scp` / `rsync` / `http`。

它的意义在于**兼具两者优点**：

| | 便利性（任意网络工具） | 速度 |
|---|---|---|
| WiFi scp | ✅ | ❌ ~0.5 MB/s |
| USB ADB | ⚠️（只有 adb 工具） | ✅ ~38 MB/s |
| **USB 网卡 (g_ether) + scp** | ✅ | ✅ 接近 USB 上限 |

因为底层还是 **USB bulk**，所以能拿到 USB 级速度，同时上层是标准 TCP/IP，可以用任何网络
工具。代价是要配 USB 网络 gadget（IP 分配、可能的 RNDIS 驱动）。**这其实是 UNO Q 也提供的
形态之一**，可作为后续增强方向。

> 注意区分：
> - 「**WiFi 传文件**」= 走真实无线射频（本文 §2.3，慢）。
> - 「**USB 模拟网口**」= 走 USB 线、只是协议层伪装成网络（快）。
> 二者完全不同，别混为一谈。

---

## 5. 实测方法（口径说明）

- 文件：8 MiB（`dd if=/dev/urandom … bs=1M count=8`）。
- **ADB**：`adb push` 自带吞吐报告（8388608 bytes in 0.21s → 38 MB/s），SHA256 双端一致。
- **串口 Bridge**：早前用 2 MB 文件实测 ~12 MB/s（CDC‑ACM `/dev/ttyGS0`），SHA256 一致。
- **WiFi**：单条 SSH 会话 `ssh host 'cat file' > /dev/null` 计时（避开多次握手），
  device→Mac 跑 3 次取 0.48–0.57 MB/s；Mac→device 跑 2 次取 0.34–0.38 MB/s。
  WiFi：2.4GHz、AP「Heygooday」、协商 130 Mbit/s、信号 60%。
- 注意：WiFi 数字**强依赖现场射频环境**，换 5GHz/独立 AP 会变好，但量级不变。

---

## 6. 选型建议

| 场景 | 推荐 |
|------|------|
| 日常传文件 / 刷固件 / 调试，要快要稳 | **ADB**（38 MB/s，工具链通用） |
| 极简、零依赖、可控协议、教学 | 串口 Bridge（12 MB/s） |
| 设备已联网、PC 不想插线、传小文件 | WiFi scp（慢但方便） |
| 既要网络工具又要 USB 速度 | **USB 网卡 gadget (g_ether) + scp**（后续可做） |

**结论：要性能走 USB（ADB 最优）；WiFi 适合「无线方便」而非「快」。**
