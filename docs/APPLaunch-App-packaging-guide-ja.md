# M5CardputerZero APPLaunch App パッケージングガイド

## 目次

- [1. 概要](#1-概要)
- [2. .desktop ショートカットファイル](#2-desktop-ショートカットファイル)
  - [2.1 フィールド説明](#21-フィールド説明)
  - [2.2 完全な例](#22-完全な例)
  - [2.3 インストール方法](#23-インストール方法)
- [3. Debian パッケージ構造](#3-debian-パッケージ構造)
  - [3.1 各ファイルの説明](#31-各ファイルの説明)
- [4. パッケージング手順](#4-パッケージング手順)
- [5. クイックデプロイ（ショートカット登録のみ）](#5-クイックデプロイショートカット登録のみ)
- [6. よくある質問](#6-よくある質問)

---

<!-- SECTION: overview -->
## 1. 概要

M5CardputerZero APPLaunch は、デバイス上で動作するアプリケーションランチャーです。
`/usr/share/APPLaunch/applications/` ディレクトリ内にある `.desktop` で終わるすべてのファイルをスキャンし、
選択可能な App としてメイン画面のスライドリストに追加します。APPLaunch 起動後はディレクトリ watcher により、
約 3 秒ごとに変更を確認します。そのため `.desktop` の追加、削除、変更後は通常、自動的にリストが更新されます。

そのため、**APPLaunch に新しい App を追加する方法は 2 つあります**：

| 方法 | 適した用途 |
|------|------------|
| デバイス上に `.desktop` ファイルを直接配置する | すばやい検証、一時的な追加 |
| Debian パッケージ（`.deb`）を作成してインストールする | 正式リリース、永続的なインストール |

> APPLaunch のメインプログラムは `/usr/share/APPLaunch/` にインストールされ、
> 作業ディレクトリも `/usr/share/APPLaunch/` です。
> そのため `.desktop` ファイル内の **相対パスはすべてこのディレクトリをルートとして扱います**。

<!-- SECTION: desktop_file -->
## 2. .desktop ショートカットファイル

APPLaunch は XDG Desktop Entry に似た形式の `.desktop` ファイルで App を記述します。
ファイルはデバイスの `/usr/share/APPLaunch/applications/` ディレクトリに配置する必要があり、ファイル名は
**必ず `.desktop` で終わる必要があります**（例：`myapp.desktop`）。

<!-- SECTION: desktop_fields -->
### 2.1 フィールド説明

ファイルには `[Desktop Entry]` セクションヘッダーが必要です。APPLaunch が認識するフィールドは次のとおりです：

| フィールド | 必須 | 型 | 説明 |
|------|----------|------|------|
| `Name` | **必須** | 文字列 | ランチャーリストに表示される App 名 |
| `Exec` | **必須** | 文字列 | 実行するコマンド、または実行ファイルのパス |
| `Icon` | 任意 | 文字列 | アイコンパス（`/usr/share/APPLaunch/` からの相対パス、または絶対パス） |
| `Terminal` | 任意 | `true`/`false` | 内蔵ターミナル（UISTPage）で実行するかどうか。デフォルトは `false` |
| `Sysplause` | 任意 | `true`/`false` | ターミナル実行終了後に「任意のキーで戻る」プロンプトを表示するかどうか。デフォルトは `true` |
| `Type` | 任意 | 文字列 | `Application` を固定で指定（ツールチェーン識別用。APPLaunch 自体は検証しません） |
| `TryExec` | 任意 | 文字列 | ドキュメント説明用のみ。APPLaunch はこのフィールドを解析しません |

> **注意：** フィールド解析時には key と value の両端の空白/Tab が自動的に取り除かれます。
> 空行、および `#` または `;` で始まる行はコメントとしてスキップされます。

> **Exec の安全制限：** APPLaunch は `Exec` を検証します。shell メタ文字を含むコマンドは拒否されます。
> `/` を含むパスは実行可能ファイルを指している必要があります。パスを含まないコマンドは現在、`bash`、`sh`、
> `python3`、`vim`、`vi`、`nano` などの内蔵ホワイトリストコマンドのみ許可されます。

#### `Terminal` と `Sysplause` の動作

- `Terminal=false`（デフォルト）：APPLaunch は `fork` + `execlp` で外部プログラムを直接起動します。
  プログラム実行中、ランチャーの更新は一時停止します。プログラム終了後は自動的にメイン画面へ戻ります。
  Home キーを 5 秒間長押しすると `SIGINT` を送信し、さらに 3 秒待っても終了しない場合は `SIGKILL` を送信します。
- `Terminal=true`：APPLaunch は内蔵ターミナル画面（UISTPage）でコマンドを実行し、
  キーボード入力と出力表示に対応します。
- `Sysplause=true`（デフォルト。`Terminal=true` の場合のみ有効）：コマンド終了後に
  "Press any key to return..." を表示し、ユーザーの確認を待ってからメイン画面へ戻ります。
- `Sysplause=false`：コマンド終了後、ただちにメイン画面へ戻ります。

<!-- SECTION: desktop_example -->
### 2.2 完全な例

**例 1 – ターミナル内で vim を実行（APPLaunch に同梱されるテンプレート）**

```ini
[Desktop Entry]
Name=Vim
TryExec=vim
Exec=vim
Terminal=true
Icon=share/images/email.png
Type=Application
```

**例 2 – GUI プログラムを直接起動（内蔵ターミナルを使用しない）**

```ini
[Desktop Entry]
Name=Calculator
Exec=/home/pi/M5CardputerZero-Calculator-linux-aarch64
Terminal=false
Icon=share/images/math.png
Type=Application
```

**例 3 – ターミナル内でスクリプトを実行し、終了後すぐに戻る**

```ini
[Desktop Entry]
Name=MyScript
Exec=/home/pi/my_script.sh
Terminal=true
Sysplause=false
Icon=share/images/hack.png
Type=Application
```

**例 4 – システムコマンドを使用（bash 内蔵。完全パス、または PATH から到達可能であること）**

```ini
[Desktop Entry]
Name=Python3
Exec=python3
Terminal=true
Icon=share/images/python.png
Type=Application
```

<!-- SECTION: install_desktop -->
### 2.3 インストール方法

**方法 A：デバイスへ直接コピー（SSH）**

```bash
# 在开发机上执行
scp myapp.desktop pi@<device-ip>:/tmp/
ssh pi@<device-ip> "sudo install -m 0644 /tmp/myapp.desktop /usr/share/APPLaunch/applications/"

# 通常 3 秒内会自动刷新；如需强制刷新，可重启用户服务
ssh pi@<device-ip> "systemctl --user restart APPLaunch.service"
```

**方法 B：デバイス上で直接作成**

```bash
sudo tee /usr/share/APPLaunch/applications/myapp.desktop > /dev/null << 'EOF'
[Desktop Entry]
Name=MyApp
Exec=/home/pi/myapp
Terminal=false
Icon=share/images/email.png
Type=Application
EOF

# 通常 3 秒内会自动刷新；如需强制刷新：
systemctl --user restart APPLaunch.service
```

<!-- SECTION: deb_structure -->
## 3. Debian パッケージ構造

App を Debian パッケージとして配布する場合、パッケージングディレクトリは次の固定構造に従う必要があります：

```
debian-<AppName>/
├── DEBIAN/
│   ├── control          # 包元数据
│   ├── postinst         # 安装后脚本（启动服务）
│   └── prerm            # 卸载前脚本（停止服务）
└── usr/
    └── share/
        └── APPLaunch/              # 主程序安装根目录（WorkingDirectory）
            ├── applications/
            │   └── myapp.desktop   # App 快捷方式（必须以 .desktop 结尾）
            ├── bin/
            │   └── M5CardputerZero-demo   # 主程序可执行文件
            ├── lib/
            │   └── lvgl.so         # 动态库
            └── share/
                ├── font/
                │   └── *.ttf       # 字体文件
                └── images/
                    └── *.png       # 图标 / 图片资源
```

パッケージングコマンド：
```bash
dpkg-deb -b debian-<AppName> <package_name>_<version>-<revision>_arm64.deb
# 示例：
dpkg-deb -b debian-example example_0.1-m5stack1_arm64.deb
```

<!-- SECTION: deb_files -->

### 3.1 各ファイルの説明

#### `DEBIAN/control`

パッケージのメタデータ記述ファイルです。形式は次のとおりです：

```
Package: applaunch
Version: 0.1
Architecture: arm64
Maintainer: yourname <you@example.com>
Original-Maintainer: m5stack <m5stack@m5stack.com>
Section: APPLaunch
Priority: optional
Homepage: https://www.m5stack.com
Packaged-Date: 2026-04-27 12:03:35
Description: M5CardputerZero APPLaunch
```

| フィールド | 説明 |
|------|------|
| `Package` | パッケージ名（小文字、空白なし） |
| `Version` | バージョン番号。例：`0.1` |
| `Architecture` | `arm64` を固定で指定（M5CardputerZero プラットフォーム） |
| `Maintainer` | メンテナー情報 |
| `Section` | カテゴリ。`APPLaunch` を指定 |

> `WorkingDirectory=/usr/share/APPLaunch` により、`.desktop` ファイル内の
> 相対パス（例：`share/images/email.png`）の基準ディレクトリが決まります。

#### `usr/share/APPLaunch/applications/<name>.desktop`

APPLaunch に登録される App ショートカットです。形式は第 2 節を参照してください。
ファイル名は必ず `.desktop` で終わる必要があります。

#### `usr/share/APPLaunch/share/images/*.png`

`.desktop` ファイルの `Icon` フィールドから参照されるアイコンファイルです。
参照例：`Icon=share/images/myapp.png`

#### `usr/share/APPLaunch/share/font/*.ttf`

APPLaunch UI が使用するフォントファイルです（メインプログラムと一緒に配布されます）。

<!-- SECTION: build_deb -->
## 4. パッケージング手順

---

### 4.2 手動パッケージング手順

スクリプトを使用しない場合は、次の手順で手動作業できます：

**手順 1：ディレクトリ構造を作成**

```bash
PKG=debian-myapp
mkdir -p $PKG/DEBIAN
mkdir -p $PKG/usr/share/APPLaunch/bin
mkdir -p $PKG/usr/share/APPLaunch/lib
mkdir -p $PKG/usr/share/APPLaunch/share/font
mkdir -p $PKG/usr/share/APPLaunch/share/images
mkdir -p $PKG/usr/share/APPLaunch/applications
```

**手順 2：メインプログラムとリソースファイルを配置**

```bash
cp /path/to/myapp   $PKG/usr/share/APPLaunch/bin/
cp /path/to/share/font/*.ttf             $PKG/usr/share/APPLaunch/share/font/
cp /path/to/share/images/*.png           $PKG/usr/share/APPLaunch/share/images/
touch $PKG/usr/share/APPLaunch/lib/lvgl.so   # 占位，实际为动态库
```

**手順 3：App ショートカットを追加**

```bash
cat > $PKG/usr/share/APPLaunch/applications/myapp.desktop << 'EOF'
[Desktop Entry]
Name=MyApp
Exec=/usr/share/APPLaunch/bin/myapp
Terminal=false
Icon=share/images/myapp.png
Type=Application
EOF
```

**手順 4：制御ファイルを記述**

```bash
cat > $PKG/DEBIAN/control << 'EOF'
Package: myapp
Version: 0.1
Architecture: arm64
Maintainer: yourname <you@example.com>
Section: APPLaunch
Priority: optional
Homepage: https://www.m5stack.com
Description: M5CardputerZero MyApp
EOF
```



**手順 7：実行権限を設定**

```bash
chmod 755 $PKG/usr/share/APPLaunch/bin/myapp
```

**手順 8：パッケージを作成**

```bash
dpkg-deb -b $PKG myapp_0.1-m5stack1_arm64.deb
```

**手順 9：デバイスへデプロイ**

```bash
scp myapp_0.1-m5stack1_arm64.deb pi@<device-ip>:/tmp/
ssh pi@<device-ip> "sudo dpkg -i /tmp/myapp_0.1-m5stack1_arm64.deb"
```

---

<!-- SECTION: quick_deploy -->
## 5. クイックデプロイ（ショートカット登録のみ）

App の実行ファイルがすでにデバイス上に存在し、APPLaunch に入口を追加するだけでよい場合は、
完全なパッケージング手順を省略して、次のように直接操作できます：

```bash
# 1. SSH 登录设备
ssh pi@<device-ip>

# 2. 在 applications 目录创建 .desktop 文件
sudo tee /usr/share/APPLaunch/applications/myapp.desktop > /dev/null << 'EOF'
[Desktop Entry]
Name=MyApp
Exec=/home/pi/myapp
Terminal=false
Icon=share/images/email.png
Type=Application
EOF

# 3. 可选：上传自定义图标（普通用户通常没有 /usr/share 写权限，先传 /tmp 再安装）
# （在开发机执行）
scp myapp_icon.png pi@<device-ip>:/tmp/
ssh pi@<device-ip> "sudo install -m 0644 /tmp/myapp_icon.png /usr/share/APPLaunch/share/images/"

# 4. 通常 3 秒内会自动刷新；如需强制刷新：
systemctl --user restart APPLaunch.service
```

> **ヒント：** APPLaunch 起動後は約 3 秒ごとに `applications/` ディレクトリの変更を確認します。
> そのため `.desktop` の追加、削除、変更後も通常はサービスを再起動する必要はありません。

---

<!-- SECTION: faq -->
## 6. よくある質問

**Q1：`.desktop` ファイルを配置したのに、App がリストに表示されません。**

- ファイル名が `.desktop` で終わっているか確認してください（例：`myapp.desktop`。`myapp.desktop.temple` は不可）。
- ファイルに `[Desktop Entry]` セクションヘッダーがあり、`Name` と `Exec` フィールドがどちらも空でないことを確認してください。
- ディレクトリ watcher による自動更新を待つか、`systemctl --user restart APPLaunch.service` を実行してから確認してください。
- ログを確認してください：`journalctl --user -u APPLaunch.service -f`

**Q2：アイコンが空白、またはデフォルトアイコンで表示されます。**

- `Icon` フィールドのパスが正しいことを確認してください。相対パスは `/usr/share/APPLaunch/` をルートとします。
  たとえば `Icon=share/images/myapp.png` は実際のパス
  `/usr/share/APPLaunch/share/images/myapp.png` に対応します。
- 絶対パスも使用できます。例：`Icon=/home/pi/myapp_icon.png`。

**Q3：`Terminal=true` の App を実行したあと、キーボードが反応しません。**

- デバイスのキーボードドライバーが正常に動作していることを確認してください。CLI（bash）で入力をテストできます。
- プログラムが特殊なターミナル環境（例：`ncurses`）を必要としていないか確認してください。APPLaunch の内蔵ターミナルは単純な
  擬似端末（pty）ですが、基本的な I/O は正常に動作します。

**Q4：実行中の外部 App（`Terminal=false`）を強制終了するには？**

- Home キーを 5 秒間長押しすると、APPLaunch が App に `SIGINT` を送信します。
- App が 3 秒以内に `SIGINT` に応答しない場合、APPLaunch は `SIGKILL` を送信して強制終了します。

**Q5：インストール済みの APPLaunch deb パッケージをアンインストールするには？**

```bash
sudo dpkg -r applaunch
```

**Q6：パッケージング時に `dpkg-deb: error: failed to open package info file` が出ます。**

- `DEBIAN/control` ファイルの形式を確認し、フィールド名の直後に `:` と空白が続いていること、
  さらにファイル末尾に改行があることを確認してください。
- `DEBIAN/postinst` と `DEBIAN/prerm` ファイルの権限が `755` であることを確認してください。

**Q7：`.deb` パッケージの命名規則は？**

```
{package_name}_{version}-{revision}_{architecture}.deb
# 示例：
applaunch_0.1-m5stack1_arm64.deb
```

| 部分 | 説明 |
|------|------|
| `package_name` | `DEBIAN/control` の `Package` フィールドと一致。小文字 |
| `version` | ソフトウェアバージョン。例：`0.1` |
| `revision` | パッケージングリビジョン。例：`m5stack1` |
| `architecture` | `arm64` 固定 |
