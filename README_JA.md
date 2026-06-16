# launcher

[English](./README.md) | [中文](./README_ZH.md)

[M5Stack_Linux_Libs](https://github.com/m5stack/M5Stack_Linux_Libs) SDK を使用して開発された M5CardputerZero アプリケーション集です。このプロジェクトは、M5CardputerZero（AArch64 Linux）デバイス上で **LVGL 9.5** を使ったグラフィカル UI アプリケーションを構築する方法を示します。

このリポジトリには、2 つの主要プロジェクトと、いくつかのサンプル／補助プロジェクトが含まれています。
- **HelloWorld** - ステータスバーとシンプルな UI を表示する基本的なユーザーデモ
- **APPLaunch** - 複数アプリのナビゲーション、LoRa 通信、オーディオ再生などの豊富な機能を備えたアプリケーションランチャー

UI は **SquareLine Studio 1.5.0** によって生成され、ローカルデバッグ用の SDL2 シミュレーションモードと、デバイス上で実行する Linux Framebuffer モードの 2 つの表示バックエンドをサポートします。

---

## プロジェクト構成

```
launcher/
├── SDK/                        # M5Stack_Linux_Libs SDK (git submodule)
│   ├── components/             # Component libraries (lvgl_component, DeviceDriver, etc.)
│   ├── examples/               # SDK examples
│   └── tools/                  # Build toolchain scripts (SCons)
├── projects/
│   ├── UserDemo/               # Basic user demo project
│   ├── APPLaunch/              # Application launcher project (core)
│   │   ├── SConstruct          # Top-level project build script
│   │   ├── config_defaults.mk  # Default build config (Linux Framebuffer mode)
│   │   ├── darwin_config_defaults.mk  # macOS build config
│   │   ├── win_x86_sdl2_config_defaults.mk  # Windows SDL2 simulator config
│   │   ├── win_x86_cross_config_defaults.mk # Windows-to-device cross config
│   │   ├── setup.ini           # SSH deployment config
│   │   └── main/               # Main application source
│   │       ├── SConstruct      # Component build script
│   │       ├── src/
│   │       │   └── main.cpp    # Program entry point
│   │       └── ui/             # UI code
│   │           ├── ui.h / ui.cpp          # UI initialization
│   │           ├── launch.cpp / launch.h  # App list and launch logic
│   │           ├── app_registry.*         # Built-in app enable/disable registry
│   │           ├── ui_launch_page.*       # Home carousel UI
│   │           ├── launcher_ui_runtime.*  # LVGL runtime/home bootstrap
│   │           ├── animation/             # Animation effects
│   │           └── page_app/              # Built-in app pages
│   ├── Calculator/             # Calculator
│   ├── AppStore/               # App store
│   └── HelloWorld/             # Hello World example
├── ext_components/             # External components (Miniaudio, RadioLib, etc.)
├── docs/                       # Project documentation
├── scripts/                    # Repository helper tools
├── README.md
├── README_ZH.md
└── README_JA.md
```

---

## 機能

### 全般的な機能

- LVGL 9.5 ベースのグラフィカル UI
- 2 つの表示バックエンドをサポートします。
  - **SDL2**: PC 側でのシミュレーションとデバッグ（デフォルトのビルドモード）
  - **Linux Framebuffer (ST7789V)**: M5CardputerZero デバイス上で実行
- evdev キーボード／タッチ入力のサポート（デバイス側）
- SCons + Kconfig ビルドシステム。`scons menuconfig` による柔軟な設定が可能

### APPLaunch の機能

- 複数アプリのナビゲーションに対応したアプリケーションランチャー UI（カルーセル形式のページ切り替え）
- 組み込みアプリページ: CLI/ST ターミナル、Game、Setting、Compass、IP Panel、File、SSH、Mesh、Rec、Camera、LoRa、Tank Battle、および Python、Store、Math などのコマンド／外部エントリ
- LoRa 通信（RadioLib SX1262 ベース）
- オーディオ再生（Miniaudio ベース）
- バッテリー状態の監視と表示
- グローバルショートカットキーのヒント（ESC / Shift / SYM）
- アプリケーションプロセスのロック管理（Home 長押しでアプリを強制終了）
- マルチスレッドタスクプール（C-Thread-Pool）
- macOS クロスコンパイル対応

### UserDemo の機能

- ステータスバー表示: M5Stack Zero ロゴ、時計時刻、バッテリー残量
- メインコンテンツ領域: アプリ名 + ページ内容のプレースホルダー

---

## ビルド

このプロジェクトは主に Linux 環境で開発されており、クロスプラットフォームビルドにも対応しています。詳細なビルド手順は以下のとおりです。

### Linux

#### 依存関係のインストール（初回のみ必要）

```bash
sudo apt update
sudo apt install python3 python3-pip libffi-dev libsdl2-dev

pip3 install parse scons requests tqdm
pip3 install setuptools-rust paramiko scp
```

> Python のバージョンは 3.8 以上である必要があります。

#### リポジトリのクローン（サブモジュールを含む）

```bash
git clone --recursive https://github.com/CardputerZero/launcher.git
cd launcher
```

リポジトリをすでにクローン済みで、サブモジュールがまだ初期化されていない場合:

```bash
git submodule update --init --recursive
```

ビルドするには `HelloWorld` に移動します。

#### デバイス上でのビルド

```bash
# Enter the project directory
cd projects/HelloWorld
# Clean the environment
scons distclean
# Build with 8 threads
scons -j8
```

#### SDL2 シミュレーターのビルド

```bash
# Enter the project directory
cd projects/HelloWorld
# Load the configuration
export CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk
# Clean the environment
scons distclean
# Build with 8 threads
scons -j8
```

#### SDL2 シミュレーターの実行

```bash
cd dist/
./HelloWorld
```

#### クロスコンパイル

```bash
# Install the cross-compilation toolchain
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
# Enter the project directory
cd projects/HelloWorld
# Load the configuration
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
# Clean the environment
scons distclean
# Build with 8 threads
scons -j8
```

### macOS

#### 依存関係のインストール（初回のみ必要）

```bash
python3 -m venv launcher-python-venv
source launcher-python-venv/bin/activate
pip3 install parse scons requests tqdm
pip3 install setuptools-rust paramiko scp
```

> Python のバージョンは 3.8 以上である必要があります。

#### リポジトリのクローン（サブモジュールを含む）

```bash
git clone --recursive https://github.com/CardputerZero/launcher.git
cd launcher
```

リポジトリをすでにクローン済みで、サブモジュールがまだ初期化されていない場合:

```bash
git submodule update --init --recursive
```

ビルドするには `HelloWorld` に移動します。

#### クロスコンパイル

```bash
# Install the cross-compilation toolchain
brew tap messense/macos-cross-toolchains
brew install aarch64-unknown-linux-gnu
# Enter the project directory
cd projects/HelloWorld
# Load the configuration
export CONFIG_DEFAULT_FILE=mac_cross_cp0_config_defaults.mk
# Clean the environment
scons distclean
# Build with 8 threads
scons -j8
```

### Windows

Windows ビルドでは、`projects/APPLaunch` 配下の同じ SCons エントリポイントを使用します。ネイティブ SDL2 シミュレーターをビルドするには、MSYS2 MinGW シェルを使用し、`gcc`、`g++`、`pkg-config`、SDL2、FreeType が `PATH` で利用できるようにしてください。

#### 依存関係のインストール（MSYS2 UCRT64 の例）

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-SDL2 \
  mingw-w64-ucrt-x86_64-freetype \
  mingw-w64-ucrt-x86_64-python-pip

python -m pip install parse scons requests tqdm setuptools-rust paramiko scp
```

#### APPLaunch SDL2 シミュレーターのビルド

```bash
cd projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=win_x86_sdl2_config_defaults.mk
scons -j8
cd dist
./M5CardputerZero-APPLaunch.exe
```

#### デバイス向け APPLaunch のクロスコンパイル

SysGCC Raspberry64 Windows AArch64 Linux クロスツールチェーンを `https://sysprogs.com/getfile/2542/raspberry64-gcc14.2.0.exe` からインストールし、`D:\app\SysGCC\bin` にインストールされていない場合は、`projects/APPLaunch/win_x86_cross_config_defaults.mk` の `CONFIG_TOOLCHAIN_PATH` を更新してください。

```bash
cd projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=win_x86_cross_config_defaults.mk
scons -j8
```

クロスビルドの出力は `dist/M5CardputerZero-APPLaunch` で、CardputerZero デバイス上で実行できます。初回のクロスビルド時には、SDK sysroot パッケージが `SDK/github_source/static_lib_v0.0.4` にダウンロードされる場合があります。

### 設定管理コマンド

```bash
# View/modify build configuration (graphical menu)
scons menuconfig

# Clean build artifacts
scons -c

# Full clean (including configuration cache)
scons distclean

# After configuring the host IP and operation command in setup.ini,
# use scons push to transfer files to the device and execute the specified command.
scons push
```

---

## APPLaunch

このプログラムは、CardputerZero デバイス上で動作するデスクトップ UI アプリケーションです。小型 LCD 画面向けの基本操作インターフェースを提供します。

### ビルド

デバイスビルド、SDL2 シミュレータービルド、クロスコンパイルには、上記のプラットフォーム別ビルドコマンドを `cd projects/APPLaunch` とともに使用してください。APPLaunch は、`linux_x86_sdl2_config_defaults.mk`、`linux_x86_cross_cp0_config_defaults.mk`、`mac_cross_cp0_config_defaults.mk`、`win_x86_sdl2_config_defaults.mk`、`win_x86_cross_config_defaults.mk` などの独自設定ファイルを提供しています。

### パッケージ

```bash
# Optional: install dpkg to use dpkg-deb; otherwise the Python builder is used.
python3 scripts/debian_packager.py
```

このコマンドは DEB インストールパッケージを生成します。`scp` でパッケージをデバイスに転送した後、`sudo dpkg -i ./***.deb` でインストールします。

### 実行

#### 自動実行

`dpkg` で DEB パッケージをインストールすると、プログラムは `APPLaunch.service` systemd user service によって自動的に起動されます。

インストール後は、`systemd` コマンドで状態の確認と制御ができます。

```bash
pi@pi:~ $ systemctl --user status APPLaunch.service
● APPLaunch.service - APPLaunch Service
     Loaded: loaded (/usr/lib/systemd/user/APPLaunch.service; enabled; preset: enabled)
     Active: active (running) since Mon 2026-06-08 15:58:19 CST; 23min ago
 Invocation: aa5c4e3ca94742deb2fe0dc67467e670
   Main PID: 664 (M5CardputerZero)
      Tasks: 7 (limit: 448)
        CPU: 1min 19.265s
     CGroup: /user.slice/user-1000.slice/user@1000.service/app.slice/APPLaunch.service
             └─664 /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch

Jun 08 15:58:19 pi systemd[664]: Started APPLaunch.service - APPLaunch Service.
```

#### 手動実行

デバイス上で実行します（Framebuffer を使用）。

```bash
# Auto-detect the ST7789V Framebuffer device
./dist/M5CardputerZero-APPLaunch

# Or manually specify the Framebuffer device
export LV_LINUX_FBDEV_DEVICE=/dev/fb0
./dist/M5CardputerZero-APPLaunch

# Specify the keyboard input device
export LV_LINUX_KEYBOARD_DEVICE=/dev/input/by-path/platform-3f804000.i2c-event
./dist/M5CardputerZero-APPLaunch
```

### UI の説明

UI は SquareLine Studio 1.5.0 によって設計・生成されており、解像度は **320x170**（ST7789V 画面）です。

### APPLaunch UI

| 領域 | 内容 |
|------|---------|
| 上部ステータスバー | ロゴ、時計時刻、バッテリー残量、WiFi 情報 |
| メインコンテンツ領域 | アプリケーションのカルーセルページ（左右のページナビゲーション） |
| グローバルヒント | ESC / Shift / SYM ショートカットヒントのオーバーレイ |

組み込みアプリページには、CLI/ST ターミナル、Game、Setting、Compass、IP Panel、File、SSH、Mesh、Rec、Camera、LoRa、Tank Battle が含まれます。APPLaunch は、Python、Store、Math などのコマンドまたは外部プロセスのエントリも表示できます。

## 関連リソース

- [日本語プロジェクトガイド](./docs/launcher-project-guide-ja.md)
- [日本語 APPLaunch App パッケージングガイド](./docs/APPLaunch-App-packaging-guide-ja.md)
- [日本語 macOS Docker ビルドガイド](./docs/macos-docker-build-ja.md)
- [M5Stack_Linux_Libs SDK](https://github.com/m5stack/M5Stack_Linux_Libs)
- [LVGL Documentation](https://docs.lvgl.io/)
- [SquareLine Studio](https://squareline.io/)
- [M5CardputerZero Product Page](https://docs.m5stack.com/)
