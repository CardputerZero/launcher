# APPLaunch 本地自动化 RPC

`cp0_lvgl` 提供可选的 ZeroMQ 自动化接口，用于设备测试、按键注入和截图。接口默认只监听 loopback，不是远程管理 API。

## 启用条件

- `CP0_LVGL_USE_ZMQ_RPC=y`：编译 ZeroMQ RPC 实现。
- `CP0_LVGL_INIT_RPC=y`：在运行时初始化 RPC 服务。

两个开关必须同时生效。具体构建 profile 以生成的 `build/config/global_config.mk` 为准。

## 端点

| 用途 | 默认值 | 环境变量 |
| --- | --- | --- |
| 请求/响应 | `tcp://127.0.0.1:5557` | `CP0_ZMQ_RPC_ENDPOINT` |
| 按键发布 | `tcp://127.0.0.1:5558` | `CP0_ZMQ_KEY_ENDPOINT` |

Linux framebuffer 截图默认读取 `/dev/fb0`，可用 `APPLAUNCH_LINUX_FBDEV_DEVICE` 覆盖。当前截图实现支持 16 bpp 和 32 bpp framebuffer。

## 命令行客户端

从仓库根目录运行：

```bash
python3 scripts/cp0_rpc.py ping
python3 scripts/cp0_rpc.py tap 28
python3 scripts/cp0_rpc.py key 28 1 0
python3 scripts/cp0_rpc.py text 'hello'
python3 scripts/cp0_rpc.py password 'secret'
python3 scripts/cp0_rpc.py hold 28 1
python3 scripts/cp0_rpc.py screenshot screenshot.ppm
```

`key` 参数依次为 Linux evdev key code、状态（`0` release、`1` press、`2` repeat）和 modifier bitmask。客户端可通过全局选项 `--endpoint` 与 `--timeout`（毫秒）覆盖连接参数。密码会进入进程参数列表，不应在多用户主机上传入敏感值。

## 协议

| 请求 | 成功响应 |
| --- | --- |
| `ping` | `OK pong` |
| `key <code> <state> <mods>` | `OK key` |
| `text <utf8>` | `OK text` |
| `password <value>` | `OK password` |
| `screenshot` | multipart：第一帧 `OK image/x-portable-pixmap`，第二帧为 PPM 数据 |

失败响应以 `ERR ` 开头；未知命令返回 `ERR unknown command`。调用方必须检查响应前缀。

代码事实源：`ext_components/cp0_lvgl/src/cp0/cp0_lvgl_rpc.cpp`、`ext_components/cp0_lvgl/Kconfig` 和 `scripts/cp0_rpc.py`。
