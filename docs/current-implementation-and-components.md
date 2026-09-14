# Current Implementation and Components

This document is a source-of-truth snapshot for the implementation currently
present in this checkout. It complements the longer launcher guides, which
describe the original architecture and historical paths.

## Build and runtime flow

```text
projects/<project>/SConstruct
        |
        +-- SDK/tools/scons/project.py (SCons + Kconfig)
        +-- ext_components/* (component registration)
        +-- LVGL 9.5 and platform libraries
        v
project executable
        |
        +-- cp0_lvgl_run() / cp0_lvgl_run_app()
        +-- LVGL display, input, timers, and service initialization
        +-- project setup callback
        v
screen and application UI
```

The main APPLaunch entry point is `projects/APPLaunch/main/src/main.cpp`. It
creates `Cp0LvglRunOptions`, applies the backlight setting through the shared
signal API, initializes `launcher_ui` and the screensaver, and delegates the
main loop and teardown to `cp0_lvgl_run()`.

`projects/APPLaunch/main/SConstruct` compiles `main/src`, `main/ui`, and
`main/ui/settings`, generates `build/generated/include/generated/page_app.h`,
and injects the launcher version, channel, build date, and Git commit into the
compile. APPLaunch uses the local FreeType override in `main/lvgl/` while the
remaining LVGL implementation comes from the SDK.

The build configurations include device framebuffer, Linux and Windows SDL2,
and Linux AArch64 cross-builds. macOS instructions use the documented Docker
build flow. The same
component graph is reused by the HelloWorld, UserDemo, ZClaw, AppStore, and
other project directories where their SConstruct files select the required
services.

## Current module boundaries

| Area | Current implementation |
| --- | --- |
| Platform layer | `ext_components/cp0_lvgl/src/cp0` for device services and `src/sdl` for SDL simulation |
| Shared public contracts | `ext_components/cp0_lvgl/include`; implementation contracts are mainly `src/cp0_*_contract.*` |
| Display/runtime runner | `cp0_lvgl_run()` in the cp0 LVGL runner sources |
| Input | Native LVGL key delivery plus the shared `LV_EVENT_KEYBOARD` event and `key_item` contract |
| Launcher shell | `projects/APPLaunch/main/ui/launcher_ui_runtime.*`, `launch.*`, registry, desktop loader, and page models |
| Settings | `projects/APPLaunch/main/ui/settings` with policy/model/page files and generated UI component data |
| Built-in pages | `projects/APPLaunch/main/ui/page_app` for game, IP, LoRa, mesh, SSH, ST terminal, and tank battle pages |
| External applications | `.desktop` discovery in `desktop_app_loader.*`, process policy in cp0, and process-group cleanup in `cp0_external_app_runner.*` |
| ZClaw | `projects/ZClaw/main/ui` implements provider setup, webhook/WebSocket transports, approvals, persistence, and chat presentation |
| Bluetooth | `ext_components/bluectl` provides the C DBus API; cp0 contains the C++ BlueZ bridge |
| Wi-Fi | `ext_components/nmtui` implements the NetworkManager signal adapter for the cp0 Wi-Fi contract |

## Input delivery contract

Ordinary focus navigation, activation, and cancellation use native
`LV_EVENT_KEY` and `lv_event_get_key()`. Text entry, physical-key identity,
modifiers, Unicode, repeat state, shortcuts, and terminal/game input use
`LV_EVENT_KEYBOARD` with `const struct key_item *`. The event is registered by
the cp0 input backend and is delivered before the native LVGL path. Consumers
must not treat the two event parameters as interchangeable or retain the
dispatcher-owned `key_item` after the callback returns.

The device and SDL implementations of this contract are
`ext_components/cp0_lvgl/src/cp0/cp0_keyboard_lvgl_input.c`,
`ext_components/cp0_lvgl/src/sdl/sdl_lvgl_keyboard.c`, and
`ext_components/cp0_lvgl/include/keyboard_input.h`.
Event registration belongs to `src/cp0/cp0_lvgl_keyboard.c` on the device
and `src/sdl/sdl_lvgl_keyboard.c` in simulation, before APPLaunch setup.
The setup callback does not register another event. Native navigation uses
`LV_EVENT_KEY`; text, modifiers and press/release/repeat use the custom event.

## Optional services

`ext_components/cp0_lvgl/Kconfig` exposes filesystem, configuration, PTY, RPC,
audio, process, sudo, OS information, screenshot, LoRa, Wi-Fi, Bluetooth,
settings, BQ27220, saved settings, battery, camera, and sound-card services.
The SConstruct file maps enabled services to compile definitions and links
Miniaudio for audio and RadioLib for LoRa. Native builds additionally use
DBus/GIO/GLib, NetworkManager, BlueZ, libcamera, udev, xkbcommon, and optional
ZeroMQ; SDL builds select the SDL platform sources.

## Documentation maintenance

When a module, service, event contract, or build dependency changes, update
this snapshot and the matching detailed chapter under `docs/launcher-project-guide/`.
The component license inventory is maintained separately in
`docs/OPEN_SOURCE_COMPONENTS.md`.

## Application submodules

The 2026-09-09 source maintenance pass includes `AppStore`, `Calculator`,
`CameraApp/main_CameraApp`, `FactoryTest/main_FactoryTest` and
`LaunchWizard/main_Keyboard_Guide`. Changes inside them remain uncommitted
working-tree changes in their respective Git repositories; parent gitlinks
have not been advanced. SDK and emulator LVGL upstream source trees retain
their own licenses and are outside the 1,480-file maintenance manifest.

| Application | Current source boundary |
| --- | --- |
| AppStore | `main/interface` owns client/protocol and job state; `main/backend` supplies native package/network work; `main/ui` coordinates catalog, details, package jobs, refresh and shutdown |
| Calculator | `main/src` hosts input/display integration; `main/ui` holds generated widgets; `main/SConstruct` selects tinyalsa for the device configuration |
| CameraApp | `src/services` contains libcamera/V4L2 backends, frame pool, gallery, metadata and recording; `src/viewmodels` and `src/views` separate state from LVGL presentation |
| FactoryTest | `src/factory_precheck` implements precheck model/view; `src/platform` owns Linux input, USB enumeration and device services |
| Keyboard Guide | `src/core`, `models`, `view_models`, `views`, `input` and `hal` separate guide state, rendering and keyboard delivery; generated Noto font data keeps OFL licensing |

See [source audit status](source-audit/README.md) for the exact scanned file
list, outstanding independent-agent coverage, and verification limits.
