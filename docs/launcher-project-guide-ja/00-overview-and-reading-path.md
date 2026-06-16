# 00 - Overview and Reading Path

`launcher` は M5CardputerZero 向けのアプリケーションプロジェクト群です。中核となるプロジェクトは `projects/APPLaunch` です。APPLaunch はデバイス上で動作するメインランチャーで、起動後に LVGL を初期化し、ホームカルーセルを表示し、ステータスバーを表示し、組み込みページまたは外部アプリケーションを起動します。また、設定、端末、音楽、録音、カメラ、LoRa などの機能も提供します。

## 1. Documentation Goals

このドキュメントセットは、次の疑問に答えることを目的としています。

- このリポジトリの各ディレクトリは何を担当しているか。
- APPLaunch プロセスはどのように開始し、メインループはどこにあるか。
- ホームカルーセル UI はどのように構成され、更新されるか。
- 組み込みページと外部アプリケーションは、ランチャーへどのように統一的に登録されるか。
- 動的な `.desktop` アプリケーションはどのようにスキャンされ、起動されるか。
- リソース、フォント、画像、音声のパスはどのように解決されるか。
- SDL2 シミュレーション、デバイス上のネイティブビルド、クロスコンパイルはどのように機能するか。
- プロジェクトを `.deb` としてパッケージ化し、systemd で自動起動するにはどうするか。
- ページ、外部アプリケーション、リソースを追加するにはどうするか。
- 黒画面、リソース欠落、戻れない外部アプリケーションをどう調査するか。

## 2. Project in One Sentence

APPLaunch は、小さな LVGL ベースのデスクトップ環境として理解できます。

```text
Linux device / SDL2 simulation
        |
        v
cp0_lvgl platform adaptation layer
        |
        v
LVGL 9.5 UI framework
        |
        v
APPLaunch home, status bar, carousel, application manager
        |
        +--> Built-in page AppPage
        +--> PTY ST terminal application UISTPage
        +--> External independent process cp0_process_exec_blocking()
```

## 3. Recommended Reading Order

このプロジェクトが初めての場合は、次の順序で読んでください。

1. `01-project-layout-and-module-responsibilities.md`: ディレクトリ構造の全体像をつかみます。
2. `02-runtime-framework-and-boot-flow.md`: `main()` からホーム画面までの経路を理解します。
3. `03-ui-framework-and-home-carousel.md`: ホーム UI とカルーセルカードを理解します。
4. `04-application-model-and-launch-mechanism.md`: アプリケーションリストと起動方式を理解します。
5. `08-build-and-compilation-guide.md`: プロジェクトをビルドして実行します。
6. `10-extension-development-guide.md`: ページまたはアプリケーションを追加します。

特定の作業だけを行いたい場合:

| Task | Read |
| --- | --- |
| SDL2 版をローカルでビルドして実行する | `08-build-and-compilation-guide.md` |
| デバイス向けにクロスコンパイルする | `08-build-and-compilation-guide.md` |
| `.deb` をパッケージ化する | `09-packaging-deployment-and-systemd.md` |
| ホームカードのレイアウトを変更する | `03-ui-framework-and-home-carousel.md` |
| 組み込みページを追加する | `10-extension-development-guide.md` |
| `.desktop` 外部アプリケーションを追加する | `04-application-model-and-launch-mechanism.md`, `10-extension-development-guide.md` |
| 黒画面を調査する | `11-debugging-and-troubleshooting.md` |
| 機能のエントリファイルを探す | `12-common-modification-entry-points.md` |

## 4. Key Project Paths

| Path | Description |
| --- | --- |
| `projects/APPLaunch` | メインランチャープロジェクト |
| `projects/APPLaunch/main/src/main.cpp` | APPLaunch のエントリポイントと LVGL メインループ |
| `projects/APPLaunch/main/ui/launch.cpp` | アプリケーションリスト、起動ロジック、ステータスバー更新 |
| `projects/APPLaunch/main/ui/ui_launch_page.cpp` | ホーム UI、カルーセル、ホームキー処理 |
| `projects/APPLaunch/main/ui/page_app` | 組み込みページの実装 |
| `projects/APPLaunch/APPLaunch` | 実行環境へパッケージされるリソースツリー |
| `ext_components/cp0_lvgl` | ファイル、プロセス、入力、システムインターフェースをラップするプラットフォーム適応レイヤー |
| `scripts/debian_packager.py` | Debian パッケージビルドスクリプト |

## 5. Terminology

- **APPLaunch**: ランチャープロジェクト、またはランチャープロセス。
- **Home screen**: ステータスバーとアプリケーションカルーセルを持つ APPLaunch のメイン画面。
- **Built-in page**: `UISetupPage` など、APPLaunch プロセスにコンパイルされるページクラス。
- **Terminal application**: `bash` のように、`UISTPage` + PTY を通して APPLaunch 内で実行されるコマンド。
- **External application**: 独立した実行可能プログラム。起動時、APPLaunch は自身の LVGL レンダリングを一時停止し、外部プログラムの終了を待ちます。
- **Resource tree**: `APPLaunch/share/images`、`APPLaunch/share/audio`、`APPLaunch/share/font` などの実行時ファイル。
- **On-device**: M5CardputerZero 上の AArch64 Linux 環境。
- **SDL2 mode**: 開発マシン上の SDL2 ウィンドウでシミュレーション実行するモード。
