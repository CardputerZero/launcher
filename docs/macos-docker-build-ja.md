# macOS Docker ビルド & デプロイガイド

macOS (Apple Silicon) 上で Docker のネイティブ arm64 環境を使って APPLaunch をビルドします。

## 前提条件

1. Lima (Docker 実行環境) をインストールします：
```bash
brew install lima
limactl start default
docker context use lima-default
```

2. Docker が arm64 ネイティブであることを確認します：
```bash
docker info | grep Architecture
# 应输出: aarch64
```

3. 事前キャッシュ済みイメージをビルドします（一度だけ実行。以後のビルドではパッケージの再インストールは不要）：
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

## 実行ファイルをビルド

事前ビルド済みイメージを使用します（数秒程度）：
```bash
docker run --rm -v $(git rev-parse --show-toplevel):/src -w /src/projects/APPLaunch \
  cardputer-build bash -c "CardputerZero=y CONFIG_REPO_AUTOMATION=y scons -j4"
```

事前ビルド済みイメージを使用しない場合（初回は apt install が必要。約 2〜3 分）：
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

成果物のパス：`projects/APPLaunch/dist/M5CardputerZero-APPLaunch`

## deb パッケージをビルド

ビルド + deb パッケージング（1 コマンド）：
```bash
docker run --rm -v $(git rev-parse --show-toplevel):/src -w /src/projects/APPLaunch \
  cardputer-build bash -c "CardputerZero=y CONFIG_REPO_AUTOMATION=y scons -j4 && cd ../.. && python3 scripts/debian_packager.py"
```

成果物のパス：`projects/APPLaunch/tools/applaunch_*.deb`（約 15MB）

## 実行ファイルをデプロイ

バイナリを直接 scp でデバイスへ転送し、サービスを再起動します（高速な反復開発向け）：
```bash
scp projects/APPLaunch/dist/M5CardputerZero-APPLaunch pi@192.168.50.150:/tmp/
ssh pi@192.168.50.150 "echo pi | sudo -S install -m 0755 /tmp/M5CardputerZero-APPLaunch /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch && systemctl --user restart APPLaunch.service"
```

または scons push を使用します（paramiko/scp が必要）：
```bash
pip3 install paramiko scp --break-system-packages
python3 -m SCons push
```

scons push は `setup.ini` 内のデバイス IP と認証情報を読み取り、MD5 を自動比較して差分だけをプッシュします。

## deb パッケージをデプロイ

```bash
scp projects/APPLaunch/tools/applaunch_*.deb pi@192.168.50.150:/tmp/
ssh pi@192.168.50.150 "echo pi | sudo -S dpkg -i /tmp/applaunch_*.deb && systemctl --user status APPLaunch.service --no-pager"
```

deb パッケージはリソースファイル一式（フォント、画像、systemd user service など）をインストールするため、正式リリースに適しています。

## ワンコマンドビルド + デプロイ

```bash
docker run --rm -v $(git rev-parse --show-toplevel):/src -w /src/projects/APPLaunch \
  cardputer-build bash -c "CardputerZero=y CONFIG_REPO_AUTOMATION=y scons -j4" && \
scp projects/APPLaunch/dist/M5CardputerZero-APPLaunch pi@192.168.50.150:/tmp/ && \
ssh pi@192.168.50.150 "echo pi | sudo -S install -m 0755 /tmp/M5CardputerZero-APPLaunch /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch && systemctl --user restart APPLaunch.service"
```

## 注意事項

- Lima VM は aarch64 ネイティブのため、QEMU エミュレーションは不要で、ビルド速度は実機に近くなります
- 初回ビルドでは GitHub から lvgl ソースコード zip（約 100MB）をダウンロードし、その後は `SDK/github_source/` にキャッシュされます
- Docker volume mount はローカルの `build/` と `dist/` に直接書き込むため、追加のコピーは不要です
- `setup.ini` でデバイス IP を設定します（デフォルトは 192.168.50.150）
