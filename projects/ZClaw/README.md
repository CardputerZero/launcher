# CardputerZero ZClaw

ZClaw is an LVGL chat client and setup frontend for running
[ZeroClaw](https://github.com/zeroclaw-labs/zeroclaw) on CardputerZero. It can
bootstrap a local ZeroClaw gateway, configure a model provider, pair the UI,
send chat requests, and answer tool approval prompts from the device keyboard.

See [Usage Guide](docs/usage.md) for the complete interaction flow and
[Configuration Notes](docs/configuration.md) for build profiles, runtime
integration, timeouts, and persistence details.

## Current Features

- Uses a keyboard-first layout designed for the 320×170 display.
- Checks internet connectivity before opening the application.
- Provides first-run Quickstart for OpenAI, OpenRouter, Anthropic, Ollama,
  DeepSeek, and custom OpenAI-compatible endpoints.
- Downloads the pinned ZeroClaw `v0.8.2` AArch64 release when the local binary
  is missing, installs/starts its service, creates the selected agent, and
  configures the local gateway on `127.0.0.1:42617`.
- Generates and submits a pairing code automatically during Quickstart, with a
  manual pairing-code flow available under Authorization settings.
- Uses authenticated WebSocket chat after pairing, including interactive tool
  approvals (`Yes`, `Always`, or `No`). Before pairing, chat falls back to the
  configured webhook endpoint.
- Stores multiple provider profiles and exposes setup, authorization, provider,
  agent, and transport status from the settings panel.

Quickstart requires internet access for the ZeroClaw download and hosted model
providers. Ollama uses `http://127.0.0.1:11434` by default and does not require
an API key.

## Keyboard Controls

- `Enter` opens the chat editor, confirms a selection, or submits text.
- `Shift+Enter` inserts a newline in the text editor.
- `Tab` opens or closes ZClaw Settings.
- Arrow keys navigate settings, scroll chat, move the text cursor, or select an
  approval response. `Z`, `X`, `C`, and `F` also map to left, down, right, and
  up outside the text editor.
- `Esc` or `Backspace` returns from settings; during an approval prompt it
  denies the request.
- `Delete` removes the provider currently open in Provider Detail.
- `Y`, `A`, and `N` answer an approval prompt with approve, always allow, and
  deny respectively.

## Runtime Files

ZClaw keeps its state under `$HOME/.zeroclaw`:

```text
bin/zeroclaw          downloaded ZeroClaw executable
config.toml           ZeroClaw gateway, agent, and provider configuration
zclaw_providers.tsv   provider profiles managed by the UI
zclaw_ui.tsv          webhook, agent, pairing token, and setup state
```

The UI configuration files may contain provider API keys, webhook secrets, and
the gateway bearer token. Protect the user's home directory accordingly.

Packaged image assets are installed below `APPLaunch/share/images`; the build
also stages `AlibabaPuHuiTi-3-55-Regular.ttf` from the APPLaunch project.

## Build

Run builds from this directory. When changing configuration profiles, clean the
existing generated configuration first.

Linux SDL2 simulator:

```bash
cd projects/ZClaw
scons distclean
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j8
```

Linux AArch64 cross-build for CardputerZero:

```bash
cd projects/ZClaw
scons distclean
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8
```

Setting `CardputerZero=y` selects the Linux cross-build profile automatically.
On macOS, use `mac_cross_cp0_config_defaults.mk`. The legacy
`config_defaults.mk` is a base LVGL profile and does not explicitly select an
SDL or framebuffer display driver.

The build depends on the repository `SDK`, `ext_components`, and APPLaunch font
asset. `main/SConstruct` fetches `cpp-httplib` and links OpenSSL support for
HTTPS and secure WebSocket connections.

## Configuration Notes

`main/Kconfig` currently exposes `COMPILE_FOR_WEB`, `COMPILE_FOR_WIN32`, and
`COMPILE_FOR_LINUX`. These symbols are retained as build options but are not
currently referenced by the ZClaw application sources; the active backend is
selected by the LVGL options in the chosen `*_config_defaults.mk` profile.

The Linux SDL2 profile enables the simulator and loads assets relative to the
built executable. The CardputerZero profiles enable Linux framebuffer and
evdev input and use `/usr/share/APPLaunch/` as the LVGL POSIX asset root.

## Repository Layout

- `main/ui/zclaw_app.cpp` - LVGL screens, keyboard handling, setup, provider
  management, chat presentation, and approval UI
- `main/ui/zclaw_client.cpp` - ZeroClaw installation/configuration, pairing,
  webhook/WebSocket transport, and approval protocol
- `main/src/` - application entry point and LVGL platform loop
- `APPLaunch/` and `assets/` - packaged ZClaw image assets
- `SConstruct`, `main/SConstruct`, and `*_config_defaults.mk` - project build
  definitions and platform profiles

Generated `build/` and `dist/` contents are build artifacts rather than source.
