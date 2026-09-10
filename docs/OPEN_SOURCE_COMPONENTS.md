# Open-source and third-party components

Inventory checked against this checkout on 2026-09-09. Build references distinguish linked dependencies from optional SDK downloads; this is not a binary SBOM. M5Stack SPDX headers and the root MIT license apply to M5Stack-authored code, without replacing upstream, font or submodule notices.

## Project code

The [root MIT license](../LICENSE) uses `2026 M5Stack Technology CO LTD`. Copies are in `ext_components/{cp0_lvgl,bluectl,nmtui}/LICENSE`. Application submodules are separate Git repositories: their source and license changes must be delivered with those repositories. Keyboard Guide's existing community copyright remains in its `LICENSE`.

## Embedded, fetched and linked source dependencies

Paths in the evidence column are relative to the repository root.

| Component | Build/source evidence | License and local text |
| --- | --- | --- |
| M5Stack Linux Libs SDK | `SDK`, pinned gitlink; SCons/Kconfig framework | [MIT, 2024 M5Stack](../LICENSES/M5Stack-SDK-MIT.txt) |
| LVGL 9.5 | SDK LVGL component, emulator `lib/lvgl`, APPLaunch `lv_freetype*` and `lv_sdl_keyboard.c` overrides | [MIT](../LICENSES/LVGL-MIT.txt); bundled sublibraries retain their own notices |
| FreeType | APPLaunch `main/lvgl/ftmodule.h` and `main/SConstruct` | FTL or GPL-2.0; [FTL text](../projects/APPLaunch/main/lvgl/freetype-LICENSE.txt) |
| tree.hh 3.20 | `ext_components/cp0_lvgl/include/tree.hh`, Kasper Peeters 2001–2024 | [GPL-3.0-only](../LICENSES/tree.hh-GPL-3.0-only.txt): header specifies version 3 without an “or later” grant |
| miniaudio 0.11.25 | `ext_components/Miniaudio/include/miniaudio.h`; audio service | [Unlicense OR MIT-0](../ext_components/Miniaudio/LICENSE), David Reid |
| RadioLib | `ext_components/RadioLib/SConstruct`, SX126x sources; URL fetched without a pinned commit | [MIT](../ext_components/RadioLib/LICENSE), Jan Gromeš |
| sigslot | `ext_components/Sigslot/SConstruct`, pinned commit `b588b791b9cf7eb17ff0a74d8aebd4a61166c2e1` | [MIT](../ext_components/Sigslot/LICENSE) |
| smooth_ui_toolkit | `ext_components/SmoothUI/SConstruct`, pinned optional UI helper | [MIT](../ext_components/SmoothUI/LICENSE) |
| spdlog | `ext_components/Spdlog/SConstruct`, pinned optional logging component | [MIT](../ext_components/Spdlog/LICENSE); bundled fmt notice also retained |
| eventpp | cp0_lvgl, APPLaunch, AppStore and ZClaw requirements; SDK eventpp component | [Apache-2.0](../LICENSES/eventpp-Apache-2.0.txt) |
| libhv | AppStore and ZClaw `main/SConstruct` requirement `hv` | [BSD-3-Clause](../LICENSES/libhv-BSD-3-Clause.txt) |
| fmt | `SDK/components/utilities/SConstruct`, `party/fmt` | [MIT with optional compiled-object exception](../LICENSES/fmt-MIT.txt) |
| nlohmann/json 3.11.3 | `projects/CardputerZero-Emulator/vendor/nlohmann/json.hpp` | [MIT](../LICENSES/nlohmann-json-MIT.txt), Niels Lohmann; original SPDX preserved |
| tinyalsa | Calculator `main/SConstruct` requirement `tinyalsa_component` | [BSD-3-Clause](../LICENSES/tinyalsa-BSD-3-Clause.txt), copied from upstream `NOTICE` (`LICENSE` points there) |
| libv4l / libv4lconvert | CameraApp `main_CameraApp/SConstruct` links static libraries from Ubuntu `libv4l-dev_1.32.0-2ubuntu1_arm64.deb` | [Matching 1.32.0-2ubuntu1 package copyright](../LICENSES/libv4l-1.32.0-copyright.txt): LGPL-2.1-or-later for core libraries, plus per-file BSD/JPEG/GPL terms |
| Noto Sans SC generated fonts | Keyboard Guide `src/assets/res_c/font_noto_sans_sc_*.c`; conversion command names input font | [OFL-1.1](../projects/LaunchWizard/main_Keyboard_Guide/src/assets/fonts/NotoSans/OFL.txt) |
| JetBrains Mono | `projects/APPLaunch/APPLaunch/share/font` | [OFL-1.1](../projects/APPLaunch/APPLaunch/share/font/JetBrainsMono-OFL.txt) |

LVGL embeds separately licensed codecs, renderers and fonts. Preserve its `src/libs/*/LICENSE*`, `src/stdlib/builtin/LICENSE*` and `scripts/built_in_font/font_license/` records when distributing those features. Generated font arrays retain the input font's license. Audio notices remain beside assets, including FactoryTest and ZClaw `LICENSE-UI-SFX.txt` and Keyboard Guide `LICENSE-AUDIO`.

## Additional SDK downloads

These source trees exist locally; cache presence does not establish that APPLaunch links them.

| Component | Evidence / selection | License text |
| --- | --- | --- |
| cJSON | Optional `SDK/components/cjson/SConstruct` | [MIT](../LICENSES/cJSON-MIT.txt) |
| C-Thread-Pool | `SDK/github_source/C-Thread-Pool`; no direct launcher wrapper found | [MIT](../LICENSES/C-Thread-Pool-MIT.txt) |
| cpp-httplib | `SDK/github_source/cpp-httplib`; no direct launcher wrapper found | [MIT](../LICENSES/cpp-httplib-MIT.txt) |
| SimpleBLE | `SDK/github_source/SimpleBLE`; cp0 Bluetooth uses BlueZ/DBus | [BUSL-1.1](../LICENSES/SimpleBLE-BUSL-1.1.md): source-available with usage restrictions and version-dependent GPL v3 change date; not a permissive open-source dependency |

## Platform dependencies

Component SConstruct files and application SConstruct/CMake files resolve these from the host or target sysroot. Preserve version-specific notices from the packages actually shipped.

| Dependency | Use | License family |
| --- | --- | --- |
| SDL2, optional SDL2_mixer | Simulator/emulator display, input and audio | zlib; mixer codecs have separate terms |
| GLib, GObject, GIO | Native service/event integration | LGPL-2.1-or-later |
| libdbus; BlueZ daemon | Bluetooth IPC | D-Bus AFL/GPL dual terms; BlueZ GPL/LGPL per component |
| libnm; NetworkManager daemon | Wi-Fi | libnm LGPL-2.1-or-later; NetworkManager GPL-2.0-or-later |
| libinput, xkbcommon, udev | Native input/device discovery | MIT for libinput/xkbcommon; udev LGPL terms |
| libcamera, libcamera-base | Camera | LGPL-2.1-or-later library; separate tool terms |
| libjpeg / libjpeg-turbo | Camera/image encoding | IJG, BSD and zlib notices by source file |
| ZeroMQ | Optional log publisher and automation RPC | MPL-2.0 for current libzmq; verify selected package |
| OpenSSL | AppStore/ZClaw TLS | OpenSSL 3 Apache-2.0; older releases differ |
| libcrypt, libc, libstdc++ | Account setup and runtime | Package-specific LGPL/GPL with runtime exceptions |

Packaging must preserve notices for the selected configuration, including static libraries, fonts and audio. tree.hh GPL terms, SimpleBLE restrictions and the selection of libv4l objects must be checked against the shipped artifact; this source inventory does not resolve those release-specific questions.
