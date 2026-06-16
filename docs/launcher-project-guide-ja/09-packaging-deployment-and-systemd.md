# 09 - パッケージング、配備、systemd

この章では、APPLaunch を `dist` ディレクトリから Debian `.deb` としてパッケージ化する方法、M5CardputerZero へ配備する方法、systemd 自動起動を設定する方法、配備問題の確認とトラブルシュート方法を説明します。

特に記載がない限り、すべてのコマンドはリポジトリルートから開始するものとします。

```bash
cd /home/nihao/w2T/github/launcher
```

## 1. 配備形態の概要

APPLaunch はデバイス上で 2 種類のファイルに依存します。

1. メインプログラム: `M5CardputerZero-APPLaunch`。
2. 実行時リソースツリー: `APPLaunch/`。アプリケーション記述子、フォント、画像、音声、スクリプト、任意のサブアプリケーションを含みます。

正式インストール後のターゲットパスは次のとおりです。

```text
/usr/share/APPLaunch/
├── applications/
├── bin/
│   ├── M5CardputerZero-APPLaunch
│   ├── M5CardputerZero-AppStore              # packaged if it exists in dist/bin
│   ├── M5CardputerZero-Calculator            # packaged if it exists in dist/bin
│   └── appstore.py                           # packaged if it exists in dist/bin
├── lib/
├── share/
│   ├── font/
│   └── images/
└── cache -> /var/cache/APPLaunch             # created by postinst
```

systemd サービスファイルは次へインストールされます。

```text
/usr/lib/systemd/user/APPLaunch.service
```

サービス起動コマンド:

```text
/usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
```

作業ディレクトリ:

```text
/usr/share/APPLaunch
```

パッケージは APPLaunch を UID 1000 ユーザーの systemd user service としてインストールします。サービスを手動で確認する場合は、そのユーザーでログインして `systemctl --user ...` を実行するか、`runuser`/SSH 自動化経由では `XDG_RUNTIME_DIR=/run/user/1000` を設定してください。

## 2. パッケージング前にデバイスターゲットをビルドする

`.deb` には Linux SDL2 x86_64 シミュレーション成果物ではなく、arm64 デバイス成果物を使う必要があります。

Linux x86_64 開発マシンで推奨されるクロスコンパイル:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
file dist/M5CardputerZero-APPLaunch
```

`file` の結果には次が含まれている必要があります。

```text
ARM aarch64
```

`x86-64` と表示される場合、SDL2 ホスト成果物をパッケージ化しており、正式なランチャーとしてデバイスへインストールできません。

デバイス上のネイティブビルドもパッケージングに使用できます。

```bash
cd /home/pi/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=config_defaults.mk
scons -j2
file dist/M5CardputerZero-APPLaunch
```

## 3. `debian_packager.py` 共通パッケージングスクリプト

リポジトリレベルのパッケージングスクリプトは次にあります。

```text
scripts/debian_packager.py
```

これは以前の APPLaunch ローカルパッケージングスクリプトを置き換えるもので、`projects/` 配下の他プロジェクトでも同じ Debian パッケージングフローを再利用できます。APPLaunch は引き続きデフォルトターゲットなので、引数なしでスクリプトを実行すると APPLaunch がパッケージ化されます。

主なデフォルトとオプション:

| Option / Default | Value | Description |
| --- | --- | --- |
| `--project` | `APPLaunch` | `projects/` 配下のプロジェクト名、またはプロジェクトパス |
| `--package-name` | `applaunch` | Debian パッケージ名 |
| `--app-name` | `APPLaunch` | インストールされるアプリケーション名と systemd サービス名 |
| `--bin-name` | `M5CardputerZero-APPLaunch` | メイン実行ファイル名 |
| `--src` / `--src-folder` | `dist` | ビルド出力ディレクトリ。プロジェクトディレクトリ相対で解決 |
| `--app-tree` | auto | 実行時リソースツリー。既定は `<project>/<app-name>`、次に `<src>/<app-name>` |
| `--output-dir` | `<project>/tools` | 生成される `.deb` の出力ディレクトリ |
| `--work-dir` | output directory | ステージングディレクトリの親 |
| `--builder` | `auto` | 利用可能なら `dpkg-deb`、なければ pure Python writer を使用 |

生成される APPLaunch パッケージファイル名の形式:

```text
applaunch_0.2.1-m5stack1_arm64.deb
```

## 4. `.deb` パッケージディレクトリ構造

デフォルト APPLaunch オプションでスクリプトを実行すると、ステージングディレクトリは `projects/APPLaunch/tools` 配下に生成されます。

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

最終的な APPLaunch `.deb` ファイルは次にあります。

```text
projects/APPLaunch/tools/applaunch_0.2.1-m5stack1_arm64.deb
```

## 5. パッケージングコマンド

### 5.1 パッケージングツールのインストール

`debian_packager.py` は Python だけでも `.deb` ファイルをビルドできます。`dpkg-deb` がインストールされている場合、既定の `--builder auto` 経路では自動的にそれを使用します。

Linux 開発マシン:

```bash
sudo apt update
sudo apt install -y dpkg-dev fakeroot
```

macOS では Python builder または Homebrew `dpkg` を使えます。

```bash
brew install dpkg
```

### 5.2 APPLaunch パッケージングを実行

リポジトリルートから実行:

```bash
python3 scripts/debian_packager.py
```

同等の明示的コマンド:

```bash
python3 scripts/debian_packager.py build \
  --project APPLaunch \
  --package-name applaunch \
  --app-name APPLaunch \
  --bin-name M5CardputerZero-APPLaunch
```

成功すると、次のような出力が表示されます。

```text
Creating Debian package applaunch_0.2.1-m5stack1_arm64.deb ...
Staged package tree: .../projects/APPLaunch/tools/debian-APPLaunch
Debian package created: .../projects/APPLaunch/tools/applaunch_0.2.1-m5stack1_arm64.deb
Builder: dpkg-deb
```

### 5.3 カスタムバージョンを指定

```bash
python3 scripts/debian_packager.py build --version 0.2.2 --revision m5stack2
```

別プロジェクトの場合は、プロジェクトメタデータと実行ファイル名を上書きします。

```bash
python3 scripts/debian_packager.py build \
  --project Calculator \
  --package-name calculator \
  --app-name Calculator \
  --bin-name M5CardputerZero-Calculator \
  --src dist \
  --app-tree share
```

`--app-tree` は、パッケージ内で `/usr/share/<app-name>` になるべきリソースツリーに合わせて調整してください。

### 5.4 パッケージング成果物のクリーン

スクリプトは次をサポートします。

```bash
python3 scripts/debian_packager.py clean
python3 scripts/debian_packager.py distclean
```

違い:

| Command | Behavior |
| --- | --- |
| `clean` | `projects/APPLaunch/tools` 配下のデフォルト APPLaunch `*.deb` ファイルと `debian-APPLaunch` を削除 |
| `distclean` | `clean` に加えて、`projects/APPLaunch/tools` 配下のレガシー `m5stack_*` 出力も削除 |

同じ clean コマンドは、非デフォルトプロジェクト向けに `--project`、`--project-dir`、`--app-name`、`--output-dir` も受け付けます。

## 6. パッケージングスクリプトのコピー規則

### 6.1 メインプログラムの検索

スクリプトは `src_folder` 配下でメインプログラムを探します。デフォルト APPLaunch ターゲットでは、これは `projects/APPLaunch` からの相対 `dist` です。

検索順序:

1. `projects/APPLaunch/dist/M5CardputerZero-APPLaunch`
2. `projects/APPLaunch/dist/bin/M5CardputerZero-APPLaunch`

どちらも存在しない場合、次を送出します。

```text
PackError: binary M5CardputerZero-APPLaunch not found in <project>/dist or <project>/dist/bin
```

### 6.2 追加アプリとバックエンド

スクリプトは次の任意ファイルを含めようとします。

```text
projects/APPLaunch/dist/bin/M5CardputerZero-AppStore
projects/APPLaunch/dist/bin/appstore.py
projects/APPLaunch/dist/bin/M5CardputerZero-Calculator
```

存在する場合、次へコピーされます。

```text
/usr/share/APPLaunch/bin/
```

`.py` 以外のファイルは `0755` に設定されます。

### 6.3 リソースツリーコピー

スクリプトはソース側リソースツリーを優先してコピーします。

```text
projects/APPLaunch/APPLaunch
```

パッケージ内のターゲット:

```text
usr/share/APPLaunch
```

ソースリソースツリーが存在しない場合は、次を試します。

```text
projects/APPLaunch/dist/APPLaunch
```

つまり、パッケージングは通常 `dist/APPLaunch` だけに依存せず、プロジェクトソースディレクトリの `APPLaunch/` リソースツリーもコピーします。

### 6.4 AppStore 画像の追加

次のディレクトリが存在する場合:

```text
projects/AppStore/share/images
```

スクリプトは次の画像をパッケージ内の `usr/share/APPLaunch/share/images` へコピーします。

```text
store_wordmark.png
store_arrow_*.png
```

## 7. Debian 制御スクリプト

### 7.1 `DEBIAN/control`

生成される control ファイルには次が含まれます。

```text
Package: applaunch
Version: 0.2.1
Architecture: arm64
Maintainer: dianjixz <dianjixz@m5stack.com>
Original-Maintainer: m5stack <m5stack@m5stack.com>
Section: APPLaunch
Priority: optional
Homepage: https://www.m5stack.com
Packaged-Date: <packaging time>
Description: M5CardputerZero APPLaunch
```

重要点:

- `Architecture` は `arm64` 固定です。
- スクリプトは `Depends` を自動宣言しないため、依存ライブラリはベースイメージに含めるか、将来版で明示する必要があります。

### 7.2 `DEBIAN/postinst`

インストール後スクリプトは次を実行します。

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

目的:

- 書き込み可能なキャッシュディレクトリ `/var/cache/APPLaunch` を作成します。
- 読み取り専用/システムリソースディレクトリ配下に `cache` シンボリックリンクを作成します。
- UID 1000 ユーザーの lingering を有効にし、systemd user service を有効化して起動します。

注意: 現在の共通 packager は `ln -sfn` を使うため、繰り返しインストールしてもキャッシュリンクを安全に更新できます。

### 7.3 `DEBIAN/prerm`

削除前スクリプトは次を実行します。

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

目的:

- サービスを停止します。
- 起動時の自動開始を無効化します。
- キャッシュディレクトリを削除します。

注意: アンインストールすると `/var/cache/APPLaunch` が削除されます。そこに保存された実行時キャッシュや app-store キャッシュも削除されます。

## 8. systemd サービスファイル

スクリプトは次を生成します。

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

フィールド説明:

| Field | Description |
| --- | --- |
| `ExecStart` | APPLaunch メインプログラムを起動 |
| `WorkingDirectory` | カレントディレクトリを `/usr/share/APPLaunch` に設定し、相対パスアクセスを扱いやすくする |
| `Restart=always` | プロセス終了後は常に再起動 |
| `RestartSec=1` | 終了 1 秒後に再起動 |
| `StartLimitInterval=0` | 頻繁なクラッシュ後に systemd が再起動を止めないよう、既定の起動レート制限を無効化 |
| `After` / `Wants` | 利用可能な場合は PipeWire PulseAudio サポートの後に起動 |
| `WantedBy=default.target` | ユーザーの既定 systemd target でサービスを有効化 |

現在のパッケージは root 所有の system service ではなく、`/usr/lib/systemd/user` 配下の user service をインストールします。`postinst` により UID 1000 ユーザー向けに有効化されるため、framebuffer、evdev、GPIO、audio、camera へのデバイス権限は、イメージのユーザー/グループ規則で提供されている必要があります。

## 9. デバイスへのインストール

### 9.1 `.deb` をデバイスへコピー

デバイス IP が `192.168.28.177`、ユーザー名が `pi` だとします。

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch/tools
scp applaunch_0.2.1-m5stack1_arm64.deb pi@192.168.28.177:/home/pi/
```

### 9.2 デバイスでインストール

```bash
ssh pi@192.168.28.177
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
```

インストーラが依存関係不足を報告した場合は、先に依存関係を修正します。

```bash
sudo apt-get -f install
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
```

### 9.3 上書きインストール

同じパッケージ名またはより高いバージョンを再度インストールします。

```bash
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
```

サービスが実行中の場合、`postinst` は有効化/起動を試みます。インストール中の framebuffer または入力デバイス競合を減らすには、先にサービスを手動停止できます。

```bash
systemctl --user stop APPLaunch.service || true
sudo dpkg -i /home/pi/applaunch_0.2.1-m5stack1_arm64.deb
systemctl --user restart APPLaunch.service
```

## 10. `scons push` による簡易配備

`.deb` に加えて、プロジェクトは `setup.ini` 経由で `dist` ディレクトリをアップロードすることもサポートします。

設定ファイル:

```text
projects/APPLaunch/setup.ini
```

デフォルト内容の例:

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

実行:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons push
```

`SDK/tools/scons/push.py` は次を行います。

1. `setup.ini` を読みます。
2. `local_file_path` 配下のすべてのファイルを反復します。
3. ローカル MD5 ハッシュを計算します。
4. SSH 経由でリモートファイルの MD5 ハッシュを取得します。
5. 変更されたファイルだけをアップロードします。
6. 任意で `before_cmd` と `after_cmd` を実行します。

適している用途:

- 開発中に `dist` を素早く置き換える。
- 単一のビルド結果を素早くアップロードする。
- Debian インストールスクリプトをテストする必要がない場合。

適していない用途:

- 正式なインストールパスの検証。
- `postinst` と `prerm` の検証。
- systemd の enable/install 挙動の検証。
- 配布可能なインストールパッケージの生成。

## 11. 手動配備

`.deb` や `scons push` を使いたくない場合、ファイルを手動でコピーできます。

開発マシンからアップロード:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scp dist/M5CardputerZero-APPLaunch pi@192.168.28.177:/home/pi/
scp -r dist/APPLaunch pi@192.168.28.177:/home/pi/APPLaunch-new
```

デバイスでインストール:

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

サービスファイルがまだインストールされていない場合は、Section 8 の内容を参考に `/usr/lib/systemd/user/APPLaunch.service` を手動作成してください。

## 12. 配備確認コマンド

### 12.1 パッケージ状態

```bash
dpkg -l | grep applaunch
dpkg -s applaunch
```

パッケージがインストールしたファイルを一覧:

```bash
dpkg -L applaunch
```

インストールせずに `.deb` パッケージ内容を確認:

```bash
dpkg-deb -c applaunch_0.2.1-m5stack1_arm64.deb
```

`.deb` メタデータを確認:

```bash
dpkg-deb -I applaunch_0.2.1-m5stack1_arm64.deb
```

### 12.2 ファイルと権限

```bash
ls -l /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
file /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
ls -ld /usr/share/APPLaunch
ls -l /usr/share/APPLaunch/cache
ls -l /var/cache/APPLaunch
find /usr/share/APPLaunch/share/images -maxdepth 1 -type f | head
find /usr/share/APPLaunch/share/font -maxdepth 1 -type f | head
```

期待値:

- メインプログラムに実行権限がある。
- メインプログラムのアーキテクチャが `ARM aarch64`。
- `/usr/share/APPLaunch/cache` が `/var/cache/APPLaunch` を指している。
- 画像とフォントリソースが存在する。

### 12.3 動的ライブラリ依存関係

デバイス上で:

```bash
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
```

不足依存を確認:

```bash
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch | grep 'not found' || true
```

ライブラリが不足している場合は、対応するシステムパッケージをインストールするか、パッケージング規則を拡張して private ライブラリを `/usr/share/APPLaunch/lib` に配置し、実行時検索パスを設定します。

### 12.4 systemd 状態

```bash
systemctl --user status APPLaunch.service --no-pager
systemctl --user is-enabled APPLaunch.service
systemctl --user is-active APPLaunch.service
```

ログ表示:

```bash
journalctl --user -u APPLaunch.service -b --no-pager
journalctl --user -u APPLaunch.service -b -f
```

再起動:

```bash
systemctl --user restart APPLaunch.service
```

停止:

```bash
systemctl --user stop APPLaunch.service
```

起動時自動開始を有効化:

```bash
systemctl --user enable APPLaunch.service
```

起動時自動開始を無効化:

```bash
systemctl --user disable APPLaunch.service
```

サービスファイルを再読み込み:

```bash
systemctl --user daemon-reload
```

### 12.5 手動フォアグラウンド実行

systemd をトラブルシュートする前に、まずフォアグラウンドで実行します。

```bash
systemctl --user stop APPLaunch.service || true
cd /usr/share/APPLaunch
sudo ./bin/M5CardputerZero-APPLaunch
```

標準出力とクラッシュメッセージを直接確認できます。フォアグラウンド実行は動くが systemd では動かない場合、サービスファイル、権限、作業ディレクトリを確認してください。

### 12.6 framebuffer と入力デバイス

framebuffer を確認:

```bash
ls -l /dev/fb*
cat /sys/class/graphics/fb0/name 2>/dev/null || true
```

入力デバイスを確認:

```bash
ls -l /dev/input/
cat /proc/bus/input/devices
```

framebuffer または入力デバイスを保持しているプロセスを確認:

```bash
sudo fuser -v /dev/fb0 2>/dev/null || true
sudo fuser -v /dev/input/event* 2>/dev/null || true
```

別のグラフィックスプログラムが動作中の場合、APPLaunch が正しく表示または入力読み取りできないことがあります。

## 13. アンインストールとロールバック

### 13.1 アンインストール

```bash
sudo dpkg -r applaunch
```

これにより `prerm` が実行され、サービス停止、無効化、`/var/cache/APPLaunch` 削除が行われます。

設定ファイルもクリーンする場合:

```bash
sudo dpkg -P applaunch
```

### 13.2 古いパッケージをインストールしてロールバック

```bash
systemctl --user stop APPLaunch.service || true
sudo dpkg -i /home/pi/applaunch_old-version-m5stack1_arm64.deb
systemctl --user restart APPLaunch.service
```

確認:

```bash
dpkg -s applaunch | grep Version
systemctl --user status APPLaunch.service --no-pager
```

### 13.3 ランチャーを一時的に無効化

```bash
systemctl --user disable --now APPLaunch.service
```

復元:

```bash
systemctl --user enable --now APPLaunch.service
```

## 14. よくある配備エラー

### 14.1 インストールエラー: `package architecture (arm64) does not match system`

原因: デバイスシステムが arm64 ではない、または arm64 パッケージを x86_64 開発マシンへ直接インストールしています。

修正:

```bash
uname -m
dpkg --print-architecture
```

`.deb` は Linux x86_64 開発マシンではなく、M5CardputerZero デバイスにインストールしてください。

### 14.2 実行時エラー: `Exec format error`

原因: メインプログラムのアーキテクチャが間違っています。よくある例は、Linux SDL2 x86_64 成果物を arm64 パッケージへ入れている場合です。

確認:

```bash
file /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch
```

正しい修正: 再度クロスコンパイルします。

```bash
cd projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
```

その後、再パッケージ化して再インストールします。

### 14.3 サービスが再起動し続ける

確認:

```bash
systemctl --user status APPLaunch.service --no-pager
journalctl --user -u APPLaunch.service -b --no-pager | tail -n 100
```

よくある原因:

- 動的ライブラリ不足。
- リソースパスが存在しない。
- framebuffer または入力デバイスが利用できない。
- プログラムが起動直後にクラッシュする。
- インストールされた成果物のアーキテクチャが間違っている。

追加確認:

```bash
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch | grep 'not found' || true
ls /usr/share/APPLaunch/share/images
ls /dev/fb0
```

### 14.4 `ln: failed to create symbolic link '/usr/share/APPLaunch/cache': File exists`

原因: 古いパッケージが非冪等な `ln -s` でキャッシュリンクを作成しており、ターゲットが既に存在しています。

修正:

```bash
sudo rm -rf /usr/share/APPLaunch/cache
sudo mkdir -p /var/cache/APPLaunch
sudo ln -sfn /var/cache/APPLaunch /usr/share/APPLaunch/cache
systemctl --user restart APPLaunch.service
```

現在の共通 packager は既に `ln -sfn` を書き込みます。パッケージを再ビルドして再インストールすると修正を永続化できます。

### 14.5 `dpkg-deb: error: failed to open package info file .../DEBIAN/control`

原因: パッケージングディレクトリ構造が不完全、またはスクリプトが途中で失敗して異常なディレクトリを残しています。

修正:

```bash
cd /home/nihao/w2T/github/launcher
python3 scripts/debian_packager.py clean
python3 scripts/debian_packager.py
```

### 14.6 `Binary M5CardputerZero-APPLaunch not found in .../dist`

原因: プロジェクトがビルドされていない、またはビルドディレクトリが `projects/APPLaunch/dist` ではありません。

修正:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
ls -l dist/M5CardputerZero-APPLaunch
cd /home/nihao/w2T/github/launcher
python3 scripts/debian_packager.py
```

### 14.7 サービス起動後に黒画面

調査順序:

1. 実行ファイルをフォアグラウンドで起動できるか確認します。
2. framebuffer が存在するか確認します。
3. 他プロセスがディスプレイを占有していないか確認します。
4. リソースパスが存在するか確認します。
5. journal ログを確認します。

コマンド:

```bash
systemctl --user stop APPLaunch.service || true
cd /usr/share/APPLaunch
sudo ./bin/M5CardputerZero-APPLaunch
ls -l /dev/fb0
sudo fuser -v /dev/fb0 2>/dev/null || true
journalctl --user -u APPLaunch.service -b --no-pager | tail -n 100
```

### 14.8 外部アプリが起動できない

APPLaunch はリソースツリーと `.desktop` 記述子から外部アプリを探します。まず確認:

```bash
find /usr/share/APPLaunch/applications -maxdepth 1 -type f -print
find /usr/share/APPLaunch/bin -maxdepth 1 -type f -print
```

外部アプリに実行権限があるか確認:

```bash
ls -l /usr/share/APPLaunch/bin
```

`.desktop` ファイル内の `Exec` が存在しないパスを指す場合、リソースツリーを修正するか再パッケージしてください。

## 15. リリース前チェックリスト

パッケージング前:

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
scons distclean
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
file dist/M5CardputerZero-APPLaunch
```

パッケージング後:

```bash
cd /home/nihao/w2T/github/launcher
python3 scripts/debian_packager.py
dpkg-deb -I projects/APPLaunch/tools/applaunch_0.2.1-m5stack1_arm64.deb
dpkg-deb -c projects/APPLaunch/tools/applaunch_0.2.1-m5stack1_arm64.deb | head -n 50
```

インストール後:

```bash
dpkg -s applaunch | grep -E 'Package|Version|Architecture'
systemctl --user status APPLaunch.service --no-pager
systemctl --user is-enabled APPLaunch.service
ldd /usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch | grep 'not found' || true
ls -l /usr/share/APPLaunch/cache
journalctl --user -u APPLaunch.service -b --no-pager | tail -n 100
```

機能確認:

- デバイス起動後、APPLaunch が自動的にホーム画面を表示する。
- キーボード/ボタン入力が動作する。
- ホーム画面のアプリケーションカルーセルで項目を切り替えられる。
- リソース画像とフォントが正しく表示される。
- 組み込みページに入って戻れる。
- 外部アプリを起動した後、終了して APPLaunch へ戻れる。
- AppStore/Calculator など任意のサブアプリケーションがパッケージされている場合、ランチャーから正常に起動できる。

## 16. 推奨配備フロー

正式リリースでは次を使います。

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

開発中の高速置き換えでは次を使います。

```bash
cd /home/nihao/w2T/github/launcher/projects/APPLaunch
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons -j8
scons push
```

違い: `.deb` は完全なインストールと systemd ライフサイクルを検証できます。`scons push` は高速ですが、正式なパッケージング検証の代替にはなりません。
