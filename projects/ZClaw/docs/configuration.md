# ZClaw Configuration and Integration Notes

This document records the build and runtime assumptions in the current project implementation.

## Build entry point

Run SCons from `projects/ZClaw`:

```sh
cd projects/ZClaw
scons -j8
```

On an x86_64 host, `SConstruct` selects `linux_x86_sdl2_config_defaults.mk` when `CONFIG_DEFAULT_FILE` is not set. Set a configuration explicitly when targeting another environment:

```sh
CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j8
CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk scons -j8
CONFIG_DEFAULT_FILE=mac_cross_cp0_config_defaults.mk scons -j8
```

Setting the environment variable `CardputerZero=y` forces `linux_x86_cross_cp0_config_defaults.mk`.

## Configuration profiles

| File | Intended use | Display/input backend | Filesystem root |
| --- | --- | --- | --- |
| `linux_x86_sdl2_config_defaults.mk` | Local x86_64 development | SDL2 | `./` |
| `linux_x86_cross_cp0_config_defaults.mk` | Linux x86_64 to AArch64 device | Linux framebuffer and evdev | `/usr/share/APPLaunch/` |
| `mac_cross_cp0_config_defaults.mk` | macOS to AArch64 Linux | Linux framebuffer and evdev | `/usr/share/APPLaunch/` |
| `config_defaults.mk` | Legacy/base profile | No display or input driver selected explicitly | `/usr/share/APPLaunch/` |

The cross profiles use an AArch64 GNU/Linux toolchain. The macOS profile expects an `aarch64-linux-gnu-` compatible prefix. Cross builds also derive a sysroot from `ext_components/cp0_lvgl/sdk_version.txt` and the matching SDK static-library package.

All profiles enable LVGL 9.5, POSIX filesystem support, FreeType, image/vector support, cp0_lvgl, Sigslot, and eventpp. The SDL2 and cross profiles additionally enable pthreads, Miniaudio, and RadioLib; the device cross profiles also enable NEON software drawing.

`config_defaults.mk` is not a complete runnable target by itself unless another
configuration layer selects a display and input backend.

## Kconfig scope

`main/Kconfig` currently exposes three independent compile-target flags:

- `COMPILE_FOR_WEB`
- `COMPILE_FOR_WIN32`
- `COMPILE_FOR_LINUX`

All three default to disabled. The current ZClaw source does not branch on these symbols; the selected LVGL configuration profile determines the active platform backend.

## Dependencies and generated assets

`main/SConstruct` fetches `cpp-httplib` and compiles it with OpenSSL support. Runtime HTTP, HTTPS, and WebSocket communication therefore depends on the linked `ssl` and `crypto` libraries.

The project stages static files into `build/static/APPLaunch` before linking:

- ZClaw images come from `projects/ZClaw/APPLaunch`.
- `AlibabaPuHuiTi-3-55-Regular.ttf` is copied from the main APPLaunch project.
- The staged `APPLaunch` tree is registered as a project static-file payload.

The SDL build initializes resource paths relative to the executable directory. Device profiles resolve assets through the configured `/usr/share/APPLaunch/` POSIX filesystem root.

## Runtime integration

ZClaw is a local UI client and service bootstrapper, not an embedded ZeroClaw implementation. Quickstart performs these operations through the managed CLI at `~/.zeroclaw/bin/zeroclaw`:

1. Download the AArch64 GNU/Linux ZeroClaw `v0.8.2` archive when the managed executable is absent.
2. Set the local gateway host, port, pairing requirement, and timeouts.
3. Configure the selected model provider and API credentials.
4. Ensure the selected agent exists and references that provider.
5. Run `zeroclaw service install` and `zeroclaw service start`.
6. Generate a new gateway pairing code and exchange it for a bearer token.

The UI assumes the default gateway is reachable at `127.0.0.1:42617`. Pairing posts the code to `/pair`. Paired chat uses `/ws/chat` with WebSocket subprotocol `zeroclaw.v1`; unpaired chat posts JSON to `/webhook` and may include `X-Webhook-Secret`.

## Network behavior

At startup, the UI requires at least one successful HTTPS response from its public connectivity probes. This check happens even when the configured model provider or gateway is local, so an isolated Ollama-only environment does not currently pass startup without public network access.

Important timeouts in the UI client are:

| Operation | Timeout |
| --- | --- |
| Connectivity probe | 2 seconds per endpoint |
| Pairing request | 30-second read timeout |
| Webhook chat | 180-second read timeout |
| WebSocket connect | 15 seconds |
| WebSocket chat/read loop | Up to 900 seconds |
| ZeroClaw download | 300-second read timeout |

## Persistence contract

The UI reads and writes two escaped, tab-separated files in `~/.zeroclaw`:

- `zclaw_ui.tsv` stores `webhook_url`, `agent_alias`, `webhook_secret`, `bearer_token`, and `setup_complete`.
- `zclaw_providers.tsv` stores provider alias, family, model, URI, and API key in that order.

Tabs, newlines, and backslashes are escaped when written. These formats are implementation details rather than a public interchange format; preserve field ordering and escaping if another component needs to edit them.

Because credentials are stored as plain text, packaging and diagnostic tooling must not collect these files by default.
