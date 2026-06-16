# Launcher プロジェクトガイド

このドキュメントセットは `launcher` リポジトリ、特に `projects/APPLaunch` ランチャープロジェクトを中心に、アーキテクチャ、ソースフレームワーク、ビルドフロー、実行時の動作、パッケージング、デプロイ、拡張ポイント、トラブルシューティングを説明します。

推奨する読み進め方:

1. [00 - Overview and Reading Path](00-overview-and-reading-path.md)
2. [01 - Project Layout and Module Responsibilities](01-project-layout-and-module-responsibilities.md)
3. [02 - Runtime Framework and Boot Flow](02-runtime-framework-and-boot-flow.md)
4. [03 - UI Framework and Home Carousel](03-ui-framework-and-home-carousel.md)
5. [04 - Application Model and Launch Mechanism](04-application-model-and-launch-mechanism.md)
6. [05 - Built-in Page Framework](05-built-in-page-framework.md)
7. [06 - Resources and Configuration](06-resources-and-configuration.md)
8. [07 - Input System and Key Mapping](07-input-system-and-key-mapping.md)
9. [08 - Build and Compilation Guide](08-build-and-compilation-guide.md)
10. [09 - Packaging, Deployment, and systemd](09-packaging-deployment-and-systemd.md)
11. [10 - Extension Development Guide](10-extension-development-guide.md)
12. [11 - Debugging and Troubleshooting](11-debugging-and-troubleshooting.md)
13. [12 - Common Modification Entry Points](12-common-modification-entry-points.md)

## Quick Links

- ビルドして実行するには: [08 - Build and Compilation Guide](08-build-and-compilation-guide.md) を参照してください。
- アプリの起動方法を理解するには: [04 - Application Model and Launch Mechanism](04-application-model-and-launch-mechanism.md) を参照してください。
- ホーム UI を変更するには: [03 - UI Framework and Home Carousel](03-ui-framework-and-home-carousel.md) を参照してください。
- 組み込みページを追加するには: [10 - Extension Development Guide](10-extension-development-guide.md) を参照してください。
- 黒画面や起動失敗をデバッグするには: [11 - Debugging and Troubleshooting](11-debugging-and-troubleshooting.md) を参照してください。

English version: [Launcher Project Guide](../launcher-project-guide.md) | 中文版: [Launcher 工程详细说明](../launcher工程详细说明.md)
