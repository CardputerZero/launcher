# macOS Docker 编译 & 部署指南

在 macOS (Apple Silicon) 上使用 Docker 原生 arm64 编译 APPLaunch。

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

3. 构建预缓存镜像（一次性，之后编译不用再装包）：
```bash
docker build --platform linux/arm64 -t cardputer-build -f - . <<'EOF'
FROM --platform=linux/arm64 ubuntu:24.04
RUN apt-get update && apt-get install -y \
    gcc g++ python3 python3-pip scons \
    libfreetype-dev libpng-dev libjpeg-dev \
    libinput-dev libxkbcommon-dev libudev-dev libcamera-dev pip && \
    pip install parse requests tqdm --break-system-packages && \
    rm -rf /var/lib/apt/lists/*
EOF
```

## 构建可执行文件

使用预构建镜像（秒级）：
```bash
docker run --rm -v $(git rev-parse --show-toplevel):/src -w /src/projects/APPLaunch \
  cardputer-build bash -c "CardputerZero=y CONFIG_REPO_AUTOMATION=y scons -j4"
```

不使用预构建镜像（首次需要 apt install，约 2-3 分钟）：
```bash
docker run --rm --platform linux/arm64 \
  -v $(git rev-parse --show-toplevel):/src \
  -w /src/projects/APPLaunch \
  ubuntu:24.04 bash -c "
    apt-get update -qq &&
    apt-get install -y -qq gcc g++ python3 python3-pip scons \
      libfreetype-dev libpng-dev libjpeg-dev \
      libinput-dev libxkbcommon-dev libudev-dev libcamera-dev pip >/dev/null 2>&1 &&
    pip install parse requests tqdm --break-system-packages -q &&
    rm -rf build dist &&
    CardputerZero=y CONFIG_REPO_AUTOMATION=y scons -j4
  "
```

产物路径：`projects/APPLaunch/dist/M5CardputerZero-APPLaunch`

## 构建 deb 包

编译 + 打包 deb（一条命令）：
```bash
docker run --rm -v $(git rev-parse --show-toplevel):/src -w /src/projects/APPLaunch \
  cardputer-build bash -c "CardputerZero=y CONFIG_REPO_AUTOMATION=y scons -j4 && cd ../.. && python3 scripts/debian_packager.py"
```

产物路径：`projects/APPLaunch/tools/applaunch_*.deb`（约 15MB）

## 部署可执行文件

直接 scp 二进制到设备并重启服务（快速迭代）：
```bash
scp projects/APPLaunch/dist/M5CardputerZero-APPLaunch pi@192.168.50.150:/tmp/
ssh pi@192.168.50.150 "echo pi | sudo -S install -m 0755 /tmp/M5CardputerZero-APPLaunch /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch && systemctl --user restart APPLaunch.service"
```

或使用 scons push（需要 paramiko/scp）：
```bash
pip3 install paramiko scp --break-system-packages
python3 -m SCons push
```

scons push 会读取 `setup.ini` 中的设备 IP 和凭据，自动比较 MD5 后增量推送。

## 部署 deb 包

```bash
scp projects/APPLaunch/tools/applaunch_*.deb pi@192.168.50.150:/tmp/
ssh pi@192.168.50.150 "echo pi | sudo -S dpkg -i /tmp/applaunch_*.deb && systemctl --user status APPLaunch.service --no-pager"
```

deb 包会安装完整的资源文件（字体、图片、systemd user service 等），适合正式发布。

## 一键编译 + 部署

```bash
docker run --rm -v $(git rev-parse --show-toplevel):/src -w /src/projects/APPLaunch \
  cardputer-build bash -c "CardputerZero=y CONFIG_REPO_AUTOMATION=y scons -j4" && \
scp projects/APPLaunch/dist/M5CardputerZero-APPLaunch pi@192.168.50.150:/tmp/ && \
ssh pi@192.168.50.150 "echo pi | sudo -S install -m 0755 /tmp/M5CardputerZero-APPLaunch /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch && systemctl --user restart APPLaunch.service"
```

## 注意事项

- Lima VM 是 aarch64 原生，无需 QEMU 模拟，编译速度接近真机
- 首次编译会从 GitHub 下载 lvgl 源码 zip（约 100MB），之后缓存在 `SDK/github_source/`
- Docker volume mount 直接写入本地 `build/` 和 `dist/`，不需要额外拷贝
- `setup.ini` 中配置设备 IP（默认 192.168.50.150）
