# macOS Docker 编译 & 部署指南

在 macOS (Apple Silicon) 上使用 Docker 原生 arm64 编译 APPLauncher，然后通过 scons push 部署到设备。

## 前置条件

1. 安装 Lima (Docker 运行环境)：
```bash
brew install lima
limactl start default
docker context use lima-default
```

2. 验证 Docker 是 arm64 原生：
```bash
docker info | grep Architecture
# 应输出: aarch64
```

## 一键编译

```bash
cd projects/APPLaunch

docker run --rm --platform linux/arm64 \
  -v $(git rev-parse --show-toplevel):/src \
  -w /src/projects/APPLaunch \
  ubuntu:24.04 bash -c "
    apt-get update -qq &&
    apt-get install -y -qq gcc g++ python3 python3-pip scons \
      libfreetype-dev libpng-dev libjpeg-dev \
      libinput-dev libxkbcommon-dev libudev-dev pip >/dev/null 2>&1 &&
    pip install parse requests tqdm --break-system-packages -q &&
    rm -rf build dist &&
    export CardputerZero=y &&
    export CONFIG_REPO_AUTOMATION=y &&
    scons -j4
  "
```

编译产物在 `dist/M5CardputerZero-APPLaunch`。

## 部署到设备

### 方式一：scons push（需要 paramiko/scp）

1. 编辑 `setup.ini`，确认 IP 地址：
```ini
[ssh]
remote_host = 192.168.50.150
username = pi
password = pi
```

2. 安装 Python 依赖并推送：
```bash
pip3 install paramiko scp --break-system-packages
python3 -m SCons push
```

### 方式二：手动 scp

```bash
scp dist/M5CardputerZero-APPLaunch pi@192.168.50.150:/tmp/
ssh pi@192.168.50.150 "echo pi | sudo -S cp /tmp/M5CardputerZero-APPLaunch /usr/share/APPLaunch/bin/ && echo pi | sudo -S systemctl restart APPLaunch"
```

## 加速：使用预构建 Docker 镜像

每次 `apt install` 比较慢。可以构建一次基础镜像：

```bash
docker build -t cardputer-build -f - . <<'EOF'
FROM --platform=linux/arm64 ubuntu:24.04
RUN apt-get update && apt-get install -y \
    gcc g++ python3 python3-pip scons \
    libfreetype-dev libpng-dev libjpeg-dev \
    libinput-dev libxkbcommon-dev libudev-dev pip && \
    pip install parse requests tqdm --break-system-packages && \
    rm -rf /var/lib/apt/lists/*
EOF
```

之后编译只需：
```bash
docker run --rm -v $(git rev-parse --show-toplevel):/src -w /src/projects/APPLaunch \
  cardputer-build bash -c "rm -rf build dist && CardputerZero=y CONFIG_REPO_AUTOMATION=y scons -j4"
```

## 注意事项

- Lima VM 是 aarch64 原生，无需 QEMU 模拟，编译速度接近真机
- `libcamera-dev` 在 Ubuntu 24.04 上的版本可能与项目代码 ABI 不兼容，如遇链接错误可暂时跳过 camera 功能
- 首次编译会从 GitHub 下载 lvgl 源码 zip（约 100MB），之后缓存在 `SDK/github_source/`
- Docker volume mount 会直接写入本地 `build/` 和 `dist/`，不需要额外拷贝
