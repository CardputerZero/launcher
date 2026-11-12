# AGENTS.md — Working Index for the launcher Repository

This file is **navigation + hard rules**, written for AI agents and newcomers working in this repo.
It does not replace the long-form material under `docs/`; it only tells you **what to read first, what the commands are, where to change things, and which traps to avoid**.

> **The one thing that matters most**: this repo is being developed by multiple people and multiple agents in parallel, and branches switch frequently.
> `docs/` and code comments lag behind. **The code is the single source of truth**; every path and symbol in this file is worth one `grep` before you rely on it.

Related files (all worth a quick scan):

| File | Purpose |
| --- | --- |
| [`README_ZH.md`](README_ZH.md) / [`README.md`](README.md) / [`README_JA.md`](README_JA.md) | Dependency install, per-platform builds, running. **Start here for build problems** |
| [`projects/APPLaunch/AGENTS.md`](projects/APPLaunch/AGENTS.md) | APPLaunch specifics: `compile_commands.json` prerequisite, `CP0_ENUM_CAST` macro conventions |
| [`ext_components/cp0_lvgl/AGENTS.md`](ext_components/cp0_lvgl/AGENTS.md) | **Platform-layer contract**; required reading before touching `cp0_signal_*` |
| [`projects/LaunchWizard/AGENTS.md`](projects/LaunchWizard/AGENTS.md) | LaunchWizard's single hard rule: UI font size ≥ 12px |
| [`docs/current-implementation-and-components.md`](docs/current-implementation-and-components.md) | Self-declared source-of-truth implementation snapshot + component inventory |

**Contents**:
[§1 Repository at a Glance](#1-repository-at-a-glance) ·
[§2 Quick Start](#2-quick-start-60-seconds) ·
[§3 Project Cheat Sheet](#3-project-cheat-sheet) ·
[§4 Build System](#4-build-system) ·
[§5 Architecture Entry Points](#5-applaunch-architecture-entry-points) ·
[§6 App/Page Framework](#6-app-model-and-page-framework) ·
[§7 Resources and Config](#7-resources-paths-and-configuration) ·
[§8 Code Style](#8-code-style-and-hard-rules) ·
[§9 Tests](#9-tests) ·
[§10 Common Pitfalls](#10-common-pitfalls-most-likely-first) ·
[§11 Documentation Map](#11-documentation-map) ·
[§12 Task Routing](#12-task-routing-table) ·
[§13 Collaboration](#13-collaboration-and-commits) ·
[§14 Checklist](#14-pre-delivery-checklist)

**The three most frequently violated rules** (memorize these first):

1. **Run `scons` inside `projects/<project>/`, and run `scons distclean` before switching profiles** (§4.1).
2. **`generated/page_app.h` is a build artifact — never edit it by hand, never commit it**; adding a page means touching only `page_app/` + `BUILTIN_APPS[]` (§6.2).
3. **Run `git status --short` before you start; never `git add -A`, never commit submodule pointer drift or untracked leftover directories** (§10.3, §13).

---

## 1. Repository at a Glance

```
launcher/
├── AGENTS.md                  # ← this file
├── README{,_ZH,_JA}.md        # trilingual READMEs: deps / build / run / packaging
├── SDK/                       # [submodule] M5Stack_Linux_Libs: component libs + SCons toolchain + 3rd-party source cache
├── ext_components/            # vendored external components (cp0_lvgl is the core platform layer)
│   ├── cp0_lvgl/              #   ★ platform adapter: LVGL runner, AppPage bases, cp0_signal_* service bus
│   ├── Miniaudio/ RadioLib/ Sigslot/ SmoothUI/ Spdlog/ bluectl/ nmtui/
│   └── THIRD_PARTY_NOTICES.md
├── projects/
│   ├── APPLaunch/             # ★ core: on-device launcher (carousel home + built-in pages + .desktop app management)
│   │   ├── SConstruct         #   project entry: pick config → invoke SDK build
│   │   ├── *_config_defaults.mk  # 7 build profiles (see §4.1)
│   │   ├── main/              #   main/{src,ui,ui/{animation,model,page_app,settings},include}/ + main/SConstruct
│   │   ├── APPLaunch/         #   runtime resource tree (copied wholesale into dist/): bin share lib applications adb
│   │   ├── tests/             #   43 .cpp unit tests + 15 Python source-contract tests + run_tests.sh (+ low_battery_ui/ runner)
│   │   └── dist/ build/       #   artifacts (git-ignored, wholly replaced on every build)
│   ├── ZClaw/                 # ZeroClaw chat/config frontend (SCons, 37 tests)
│   ├── LaunchWizard/          # OOBE first-boot wizard (SCons for the GUI + CMake for pure-logic tests)
│   ├── CardputerZero-Emulator/ # desktop/web device emulator (**CMake**, reuses APPLaunch/main at source level)
│   ├── HelloWorld/ UserDemo/  # minimal example / official demo
│   ├── AppStore/              # [submodule] app store
│   ├── Calculator/            # [submodule] calculator
│   ├── CameraApp/             # body lives here; main_CameraApp/ is a [submodule]
│   └── Keyboard-Guide/        # directory lives here; main_Keyboard-Guide/ is a [submodule] (currently uninitialized)
├── docs/                      # three parallel 13-part launcher guides (zh/en/ja) + topical docs (see §11)
├── scripts/                   # packaging / release / RPC / logging / hotplug scripts
├── LICENSES/                  # verbatim third-party licenses (this does NOT relicense the repo)
├── .github/workflows/         # CI: launcher-build.yml / launcher-release.yml / emulator-build.yml
├── .clang-format              # Google-derived, see §8
└── tests/                     # empty directory, no content — NOT a test entry point
```

---

## 2. Quick Start (60 Seconds)

```bash
# 0. Always inspect workspace state first. This is a shared checkout; others may be editing right now
git status --short && git branch --show-current && git submodule status

# 1. Fastest local verification: SDL2 simulator (x86_64 host)
cd projects/APPLaunch
export CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk
scons distclean && scons -j$(nproc)
cd dist && ./M5CardputerZero-APPLaunch        # needs SDL2 + FreeType

# 2. Produce on-device artifacts: cross-compile to aarch64
cd projects/APPLaunch
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
scons distclean && scons -j$(nproc)
file dist/M5CardputerZero-APPLaunch            # expect ARM aarch64

# 3. Run tests after changing code (run both; CI only runs the second)
sh projects/APPLaunch/tests/run_tests.sh          # 42 C++ + 15 Python contracts
sh ext_components/cp0_lvgl/tests/run_tests.sh     # 85 C++

# 4. Build the .deb (**does not compile anything** — packages the existing dist/)
python3 scripts/debian_packager.py

# 5. Generate compile_commands.json (for clangd / code navigation)
cd projects/APPLaunch && bear -- scons -j$(nproc)
```

Dependency install is documented in the "编译程序" (Build) section of [`README_ZH.md`](README_ZH.md): `python3 python3-pip build-essential pkg-config libffi-dev libsdl2-dev libfreetype6-dev` plus `pip3 install parse scons requests tqdm setuptools-rust paramiko scp`.

---

## 3. Project Cheat Sheet

> Run commands **inside each project directory**; when `CONFIG_DEFAULT_FILE` is unset, an x86_64 host defaults to SDL2;
> `CardputerZero=y` is equivalent to forcing `linux_x86_cross_cp0_config_defaults.mk`. Artifacts are relative to `projects/<X>/dist/`.

| Project | Purpose | Build | Artifacts | Host SDL2 | Repo nature |
| --- | --- | --- | --- | --- | --- |
| **APPLaunch** | Launcher: carousel home, built-in pages, `.desktop` app management | SCons | `M5CardputerZero-APPLaunch` + `APPLaunch/` resource tree | ✅ | plain directory |
| ZClaw | Chat/config frontend (WS or webhook) | SCons | `ZClaw` + `dist/APPLaunch/` | ✅ | plain directory |
| LaunchWizard | OOBE first-boot wizard | SCons (GUI) / CMake `ctest --preset dev` (tests) | `LaunchWizard` (CI copies it into `APPLaunch/bin/`) | ✅ | plain directory |
| HelloWorld / UserDemo | Minimal example / official demo | SCons | `HelloWorld` / `M5CardputerZero-UserDemo` | ✅ | plain directory |
| CardputerZero-Emulator | Desktop/web device emulator | **CMake** (not SCons) | `cardputer-zero-emu`; web `index.html/js/wasm/data` | ✅ | plain directory (`lib/lvgl` is a submodule) |
| AppStore | App store (registry→md5→dpkg) | SCons | `M5CardputerZero-AppStore` | ✅ | **submodule** |
| Calculator | Calculator | SCons | `Calculator` | ✅ | **submodule** |
| CameraApp | Camera (libcamera ≥ 0.7.2) | CMake presets (the SCons path is unsuitable for x86) | `CameraApp` / `.deb` presets | ⚠️ | body lives here, `main_CameraApp/` is a **submodule** |
| Keyboard-Guide | Keyboard tutorial | CMake `./bootstrap.sh` | `M5CardputerZero-Keyboard-Guide` | ✅ | **submodule** (uninitialized) |
| FactoryTest | Historical factory test | **no working entry point** | none | ❌ | untracked leftover, see §10.3 |

---

## 4. Build System

### 4.1 Seven profiles — select with `CONFIG_DEFAULT_FILE`, never hand-edit

| File | Target |
| --- | --- |
| `config_defaults.mk` | **Native build on device** (aarch64, FBDEV + EVDEV) |
| `linux_x86_sdl2_config_defaults.mk` | **Linux host SDL2 simulator** (default for day-to-day work) |
| `linux_x86_cross_cp0_config_defaults.mk` | Linux x86_64 → aarch64 cross-compile |
| `darwin_config_defaults.mk` | macOS native SDL |
| `mac_cross_cp0_config_defaults.mk` | macOS → aarch64 cross-compile |
| `win_x86_sdl2_config_defaults.mk` | Windows MSYS2/MinGW SDL2 |
| `win_x86_cross_config_defaults.mk` | Windows SysGCC → aarch64 cross-compile |

**Hard rules**:

1. **You must run `scons` inside `projects/<project>/`.** Running it from the repo root fails — `CONFIG_DEFAULT_FILE` is a bare filename and the SDK resolves it with `resolve()` relative to the CWD.
2. **Run `scons distclean` before switching profiles.** APPLaunch's `SConstruct` has no config-invalidation logic (other projects have a `selected-defaults.txt` mechanism). When `build/config/global_config.mk` already exists the SDK will not regenerate the config → you will compile arm64 while believing you are on SDL2.
3. **Do not run `scons save`**, which writes back into the shared `*_config_defaults.mk` — those are public profiles used by everyone.

### 4.2 Artifacts and directories

```
projects/APPLaunch/
├── build/config/{global_config.mk, global_config.h, lvgl_config.h, config_tmp.mk}
├── build/<Component>/*.o                # cp0_lvgl / lvgl_component / eventpp / Miniaudio ...
├── main/build/generated/include/generated/page_app.h   # ← auto-generated; do not edit or commit
├── main/build/static/APPLaunch/         # staged resource tree (re-copied on every scons run)
└── dist/
    ├── M5CardputerZero-APPLaunch        # executable
    └── APPLaunch/{adb,applications,bin,lib,share}/ + preinstalled-desktop-apps.tsv
```

- **`dist/` is wholly replaced on every build** (`rmtree` + `copytree`). Never place things there by hand.
- **Changing resources requires a rebuild**: the copy `projects/APPLaunch/APPLaunch/**` → `main/build/static/` → `dist/APPLaunch/` happens at build time.
- `build/`, `dist/`, and `compile_commands.json` are all git-ignored.

### 4.3 `compile_commands.json`

SCons itself **does not** generate it. The `bbear -- scons` command in `projects/APPLaunch/AGENTS.md` cannot run on this machine (there is no `bbear`, only `bear`):

```bash
cd projects/APPLaunch && bear -- scons -j$(nproc)   # writes ./compile_commands.json (do not commit)
```

### 4.4 CI and release

| Workflow | Trigger | Purpose |
| --- | --- | --- |
| `.github/workflows/launcher-build.yml` | push to `master`/`ci/**` (path-filtered), PR, manual | Cross-builds APPLaunch/AppStore/Calculator/LaunchWizard/ZClaw → aggregates → `debian_packager.py` produces the `.deb`; a separate `unit-tests` job runs the `cp0_lvgl` tests |
| `.github/workflows/launcher-release.yml` | manual | Release publishing (whether prereleases are published automatically varies by branch, see §10.1) |
| `.github/workflows/emulator-build.yml` | push/PR (paths such as `projects/CardputerZero-Emulator/**`) | 4 jobs: macos / linux / windows / web |

Release pipeline (local equivalent): `scripts/build_cardputerzero_release_local.sh <ver> m5stack1`
→ cross-build the five projects → aggregate `dist/` → build `cardputer-adb-hotplug` → `scripts/debian_packager.py` → `applaunch_<ver>-m5stack1_arm64.deb`.

**Note**: `debian_packager.py` only packages the **already-existing** `projects/APPLaunch/dist/APPLaunch`; it will not compile for you. The claim in `09-*.md` that packaging prefers the source tree is out of date.

### 4.5 Other build notes

- **Automation environments must `export CONFIG_REPO_AUTOMATION=y`**: otherwise, when third-party sources are missing, `SDK/tools/scons/SConstruct_tool.py` blocks on an `input()` prompt.
- **Semantic version**: `APPLAUNCH_VERSION` must be `X.Y.Z[-tag]` (`main/SConstruct` validates it with a regex and raises otherwise); there are also `APPLAUNCH_CHANNEL` and `SOURCE_DATE_EPOCH`.
- **Cross-compile sysroot**: `SDK/github_source/static_lib_<ver>`, with the version read from `ext_components/cp0_lvgl/sdk_version.txt` (currently `v0.0.7`; **the file must not end with a newline**). Downloaded automatically on first use (about 1.3G).
- **The third-party source cache does not validate versions**: after changing a pin you must delete `SDK/github_source/<repo>` to force a re-fetch.

---

## 5. APPLaunch Architecture Entry Points

### 5.1 Boot chain (from `main()` to the main loop)

```
main/src/main.cpp  main()
  ├─ system(bash_init)                    # unzip /var/template.zip → ~/Downloads
  ├─ options.after_resource_init          # backlight to 0 → restore previous brightness
  ├─ options.setup   → launcher_ui::init() [ui/ui.cpp:36] + ui_screensaver_init()
  └─ cp0_lvgl_run(options)                # ★ the real main loop
        [ext_components/cp0_lvgl/src/cp0_lvgl_app_runner.cpp  cp0_lvgl_run()]
        lv_init() → cp0_lvgl_init() → setup() → first frame →
        while: ms = lv_timer_handler(); semaphore wait for ms  → teardown()
```

- **The main loop is not under `projects/APPLaunch/main/`**; it lives in `ext_components/cp0_lvgl/src/cp0_lvgl_app_runner.cpp`.
- To wake the UI from another thread use `cp0_lvgl_wake()`. **Never** spin up your own thread to touch LVGL.
- `ui/ui.cpp:36` `launcher_ui::init()`: installs the global key handler → constructs `LauncherUiRuntime` → `start()`.

### 5.2 The display backend is selected at **compile time**

`CONFIG_V9_5_LV_USE_SDL` decides whether `ext_components/cp0_lvgl/SConstruct` collects `src/sdl/*` or `src/cp0/*`, and injects `-DHAL_PLATFORM_SDL=1`.
APPLaunch has only two `#ifdef HAL_PLATFORM_SDL` branches: `ui/launcher_ui_runtime.cpp` (skips the boot GIF) and `ui/builtin_app_registry.cpp` (SSH / IP Panel are device-only).
Screen geometry: **320×170**, a 20px top bar plus a 320×150 content area.

### 5.3 "I want to change X → open Y"

> Line numbers drift across branches — **search by symbol name first**.

| Goal | File | Key symbols |
| --- | --- | --- |
| Add/remove a built-in app | `main/ui/builtin_app_registry.cpp` | `BUILTIN_APPS[]`, `append_page_app<PageT>`, `append_cli_app` |
| Change app fields / origin semantics | `main/ui/app_registry.h` | `AppDescriptor`, `LauncherAppOrigin` |
| Enable/disable an app | `main/ui/app_registry.cpp` | `launcher_app_registry_is_enabled/_set_enabled/_notify_changed` |
| Change home app ordering | `main/ui/app_display_order.cpp` | `kBuiltInAppOrder`, `launcher_builtin_app_display_order` |
| `.desktop` parsing (add a field) | `main/ui/desktop_entry.cpp` + `tests/test_desktop_entry.cpp` | `parse_desktop_entry`, `desktop_entry.h` |
| `.desktop` scanning/filtering/limit | `main/ui/desktop_app_loader.cpp` | `launcher_append_desktop_apps` (limit 128, `desktop_app_loader.hpp`) |
| `.desktop` directory watching | `main/ui/app_directory_watcher.cpp` | `WATCH_POLL_PERIOD_MS` (3000ms) |
| External app launch / return home | `main/ui/launch.cpp` | `Launch::launch_Exec` (ordering is locked by `tests/test_external_framebuffer_ownership.py`) |
| Terminal app (PTY) | `main/ui/page_app/ui_app_st_session.cpp` | `pty_open`, `poll_cb`, `stop_pty` |
| Terminal rendering / escape sequences | `main/ui/page_app/ui_app_st_parser.cpp`, `ui_app_st_renderer.cpp` | parsing and rendering are separated |
| Terminal key encoding | `main/ui/model/st_key_encoder.cpp` + `tests/test_st_key_encoder.cpp` | — |
| CLI / Python help overlay text | `main/ui/cli_terminal_help.cpp`, `python_terminal_help.cpp` | static text only |
| Carousel geometry (slot positions/sizes) | `main/ui/animation/launcher_carousel_layout.h` | `kSlots` (editing it trips `static_assert`; sync `UILaunchPage::LauncherCarouselElement`) |
| Carousel animation curve | `main/ui/animation/ui_launcher_animation.cpp` | `kLauncherAnimationTimeMs`, `launcher_home_animate` |
| Carousel left/right switching | `main/ui/ui_launch_page.cpp` | `switch_right` / `switch_left` ⚠️ see §10.2 |
| Card/title/progress-bar appearance | `main/ui/ui_launch_page_carousel_view.cpp` | constrained by `tests/test_carousel_border_contract.py` |
| Home-screen keys | `main/ui/ui_launch_page_input.cpp` | `handle_home_key`, `fzxc_to_arrow` |
| Boot logo / GIF / startup sound | `main/ui/ui_launch_page_startup.cpp` | — |
| Icon pre-decode cache | `main/ui/home_icon_buffer_pool.cpp` | `kIconSize`, `rebuild`, `find` |
| Default placeholder icon | `main/ui/home_icon_fallback.c` | 50×50 ARGB8888; constrained by `tests/test_home_icon_fallback_contract.py` |
| Add a built-in page | `main/ui/page_app/` + `builtin_app_registry.cpp` | see §6 |
| Page base class / top bar / bottom bar | `ext_components/cp0_lvgl/include/ui_app_page.hpp` (**not in `main/ui/`**) | `AppPageRoot`, `AppPage`, `AppPageWithBottomBarLayout`, `cp0_lvgl_start_app_page` |
| Return-home state machine | `main/ui/model/launcher_navigation_model.hpp` | `LauncherPageLifecycleModel` |
| ESC long-press to return home | `main/ui/esc_hold_hint_controller.cpp` | `handle`, `poll` |
| Hang watchdog | `main/ui/esc_ui_watchdog.{h,cpp}` | `ESC_UI_RECOVERY_DELAY_MS` (exit code 75) |
| Global keyboard shortcuts | `main/ui/model/global_hint_policy.cpp` + `ui_global_hint.cpp` | `action_for` |
| Volume/brightness stepping and persistence | `main/ui/launcher_media_controls.cpp` + `model/setup_value_policy.hpp` | — |
| Volume/brightness OSD | `main/ui/launcher_media_osd.cpp` | `model/launcher_media_model.hpp` |
| Toast styling | `main/ui/launcher_toast.cpp` | `kShowDurationMs` |
| Loading overlay | `main/ui/ui_loading.cpp` | `show` / `hide` |
| Lock screen (timeout/unlock/wallpaper) | `main/ui/ui_screensaver.cpp` + `model/lockscreen_state_model.hpp`, `model/screensaver_model.hpp` | constrained by `tests/test_lockscreen_contract.py` |
| Low-battery policy / appearance / sound cues | `main/ui/low_battery_flow.hpp` / `ui_low_battery.cpp` | the policy is pure logic and unit-tested; cues register with `RegisterSystemSounds` and play via `cp0_signal_system_play` |
| Settings menu structure | `main/ui/settings/settings_page.cpp` | `create_page_detail` (node order = menu order), `UISettingTreePage` |
| Settings entry policy/gating | `main/ui/settings/settings_tree_types.hpp` | `SettingEntry`, `SettingActivationPolicy`, `activation_gate` |
| Add a settings sub-page | new `*_page.{hpp,cpp}` in `main/ui/settings/` + a factory and tree node in `settings_page.cpp` | three-level value pages derive from `LvSettingValuePage3Base` |
| Settings value ranges (brightness/volume) | `main/ui/model/setup_value_policy.{hpp,cpp}` | unit-tested |
| Theme / display initialization | `main/ui/launcher_ui_runtime.cpp` | `create_display`, `build_launcher_home`, `show_initial_screen` |
| Resource install layout | `main/SConstruct` | `STATIC_FILES` ← `projects/APPLaunch/APPLaunch/` |
| Deployment / systemd | `scripts/debian_packager.py` | — |

---

## 6. App Model and Page Framework

### 6.1 Two launch paths — do not mix them

| Type | Entry point | Behavior |
| --- | --- | --- |
| **External executable** (`.desktop`, `Exec`) | `Launch::launch_Exec()` | `cp0_signal_process_api({"ExecBlocking", ...})` — **blocking**; meanwhile `lv_timer_enable(false)` and the child process owns the framebuffer. **Do not** create timers / toasts / threads inside that window |
| **Terminal-style** (CLI, Python, `Terminal=true`) | `Launch::launch_Exec_in_terminal()` + `UISTPage` | goes through `cp0_signal_pty_api`, 30ms polling, ESC long-press to exit |

The framebuffer hand-off order for external apps is **asserted point by point** by `tests/test_external_framebuffer_ownership.py` — read that test before refactoring the code.

### 6.2 Adding a built-in page (minimal checklist)

1. Create `ui_app_xxx.hpp` + `.cpp` (optionally `ui_app_xxx_view.cpp`) in `projects/APPLaunch/main/ui/page_app/`.
2. Every `.cpp` **must** carry `#define APP_PAGE_IMPLEMENTATION_UNIT` on **line 7** (`main/ui/ui.h` uses it to decide whether to pull launcher-internal headers into the page TU; omitting it causes type/header collisions).
3. Derive the page class from `AppPage` / `AppPageRoot` (`ext_components/cp0_lvgl/include/ui_app_page.hpp`).
4. Add one row with `append_page_app<YourPageT>` to `BUILTIN_APPS[]` in `main/ui/builtin_app_registry.cpp`.
5. **Do not touch** `generated/page_app.h` and **do not touch** `SConstruct`:
   - `main/ui/generate_page_app_includes.py` scans only top-level `page_app/*.hpp`, sorts them, and emits an include list;
   - `main/SConstruct` writes it to `main/build/generated/include/generated/page_app.h`;
   - `append_srcs_dir(ADir('ui'))` collects `.cpp` recursively, so new files join the build automatically.
6. Put the icon in `projects/APPLaunch/APPLaunch/share/images/` and reference it via `launcher_platform::path(...)`.
7. Run `sh projects/APPLaunch/tests/run_tests.sh`.

### 6.3 Adding an external `.desktop` app

Drop it into `projects/APPLaunch/APPLaunch/applications/` (installed as `/usr/share/APPLaunch/applications/`). **No code changes needed** — the directory watcher reloads within 3s.

```ini
[Desktop Entry]
Name=MyApp
Exec=/usr/share/APPLaunch/bin/my_app
Terminal=false
Icon=share/images/my_app_100.png
Type=Application
```

Only these keys are parsed: `Name` (≤128B, required), `Icon` (≤512B), `Exec` (≤512B, required), `Terminal`, `Sysplause` (default true), `Type` (must be `Application`), `Hidden`/`NoDisplay`. All other keys (including `TryExec`) are **ignored**.

Silently skipped when: the filename does not end in `.desktop`; it contains `/ \` or control characters; `Name`/`Exec` is missing; `Exec` fails the backend `DesktopExecIsSafe` check; `Exec` duplicates a built-in or an already-loaded entry; `Icon` cannot be resolved. **Check the stderr log — do not delete those rejection messages.**

To make it toggleable under Settings→Launcher: the filename must be listed in `kManagedDesktopFilenames` in `main/ui/app_display_order.cpp` and the label in `kBuiltInAppOrder`; otherwise its origin becomes `StoreInstalled` → `always_on` and it cannot be disabled. To be recognized as a product-preinstalled app it must also be listed in `projects/APPLaunch/APPLaunch/preinstalled-desktop-apps.tsv`.

---

## 7. Resources, Paths, and Configuration

| Resource | Development path | Access method |
| --- | --- | --- |
| Images | `projects/APPLaunch/APPLaunch/share/images/` | `launcher_platform::path("xxx.png")` / `path_c(...)` |
| Audio | `.../share/audio/` | `launcher_platform::path(...)` + `cp0_signal_audio_api` |
| Fonts | `.../share/font/` | `launcher_fonts().get(...)` (`main/ui/ui.h`; wraps `cp0_fonts()`) |
| External binaries | `.../bin/` | the `.desktop` `Exec` |
| App descriptors | `.../applications/*.desktop` | scanned automatically |
| Bundled libraries | `.../lib/` | handle `LD_LIBRARY_PATH` / rpath yourself |

- The device resource root is fixed at `/usr/share/APPLaunch` (`kAppRoot` in `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_filesystem.cpp`); on SDL2 the root is inferred from the executable directory by `ext_components/cp0_lvgl/src/sdl/sdl_lvgl_filesystem.cpp` (default `./APPLaunch/share/images`).
- **Filename letter case must match exactly** (Linux).
- Settings persistence: `cp0_signal_config_api` (`GetInt`/`SetInt`/`GetStr`/`SetStr`/`Save`), backed by `ext_components/cp0_lvgl/src/cp0/cp0_lvgl_config.cpp`.

---

## 8. Code Style and Hard Rules

**.clang-format** (Google-derived, key effective settings): 4-space indent, `ColumnLimit: 120`, braces on a **new line for functions and class/struct**, same-line braces for `if`/`for`, `case` aligned with `switch`, left-aligned pointers/references `type *name`, `AllowShortFunctionsOnASingleLine: false`.

**Stable conventions in the actual code**:

- **C++ files mix `.hpp`/`.cpp` with `.h`**: prefer `.hpp` for new code (`page_app/`, `model/`, `settings/` all use `.hpp`); historic files such as `launch.h`, `app_registry.h`, `desktop_entry.h`, `ui.h` keep `.h`. **Follow the local directory's style.**
- New files carry an SPDX header:
  ```cpp
  /*
   * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
   *
   * SPDX-License-Identifier: MIT
   */
  ```
- **Logging uses `SLOG*`** (`SLOGI`/`SLOGE` from `sample_log.h`), **not spdlog** (spdlog is only used by side projects such as LaunchWizard/Keyboard-Guide).
- **`enum class` conversions use `CP0_ENUM_CAST`**: `#include "cp0_enum_cast.h"`, then `CP0_ENUM_CAST(Target, value)` or `CP0_ENUM_CAST_INT/SIZE_T/UINT8/...`. Do not add a project-local helper that merely wraps one of these casts; do not use them for pointer casts or for converting unvalidated external integers into enums. See [`projects/APPLaunch/AGENTS.md`](projects/APPLaunch/AGENTS.md).
- **Put new testable logic in `main/ui/model/`** (or `ui/settings/`): pure logic with no LVGL dependency is what makes unit tests possible outside the UI.
- **Do not reflow whole files**: format only the lines you changed; a large formatting sweep makes the diff unreviewable.
- **Worker-thread callbacks must marshal back to the LVGL thread** (callbacks of `cp0_signal_*` may run on worker threads).
- **All hardware/file/process/config access** goes through `cp0_signal_*` or `cp0_file_*` wrappers. Do not bypass the HAL with direct `open()`/`fork()`/`system()`.

---

## 9. Tests

| Scope | Command | Size (driven by that runner) | CI |
| --- | --- | --- | --- |
| APPLaunch | `sh projects/APPLaunch/tests/run_tests.sh` | 42 `.cpp` + 15 Python contract tests | ❌ not run |
| APPLaunch (LVGL UI) | `sh projects/APPLaunch/tests/low_battery_ui/run_tests.sh` | 1 `.cpp` (needs an LVGL build) | ❌ |
| cp0_lvgl | `sh ext_components/cp0_lvgl/tests/run_tests.sh` | 85 `.cpp` | ✅ run |
| cp0_lvgl (Bluetooth, on-device) | `run_bluetooth_reset_test.sh`, `build_bluetooth_backend_smoke.sh` | 2 `.cpp` (need real hardware) | ❌ |
| ZClaw | `sh projects/ZClaw/tests/run_tests.sh` | 37 | ❌ |
| LaunchWizard | `cmake --preset dev && ctest --preset dev` | 10 | ❌ |
| Repo-root `tests/` | — | **empty directory, not an entry point** | — |

On-disk file counts are 43 (APPLaunch) and 87 (cp0_lvgl); the difference is exactly the 1 + 2 hardware/LVGL-dependent tests that have their own dedicated runners above.

**Key insight**: `projects/APPLaunch/tests/` contains **15 Python "source contract" tests** that `read_text()` the `.cpp`/`.h` sources directly and assert on **textual structure, call ordering, and specific strings**. Therefore:

- changing UI source code can break a test **without a single compile error**;
- before refactoring, `grep projects/APPLaunch/tests/` for assertions covering the code you are about to touch;
- pattern for a new test: write `tests/test_xxx.cpp`, then append a `g++ -std=c++17 -Wall -Wextra -Werror ... -o ... && ./...` block to `run_tests.sh`.

---

## 10. Common Pitfalls (most likely first)

### 10.1 Repository state changes; config/CI are branch-dependent

**This is pitfall number one**: the repo has many parallel branches and the same checkout can be switched away within minutes.

**Do this before starting**: `git branch --show-current && git status --short`.

Known **branch-dependent points** (never treat these as fixed facts):

- **LoRa / RadioLib is a three-layer switch** — and how tightly the layers are coupled **differs by branch**, so read the actual files:
  1. `CONFIG_CP0_LVGL_INIT_LORA` — service sources + signal registration;
  2. `CONFIG_RADIOLIB_COMPONENT_ENABLED` — component registration;
  3. the `RadioLib` entry in `REQUIREMENTS` of `main/SConstruct` — linking.

  Two known shapes of layer 3, both live in this repo's history:
  - **Coupled** (`master`, `fix/radiolib-lora-disabled-by-default`): `REQUIREMENTS += ['RadioLib']` sits inside `if 'CONFIG_CP0_LVGL_INIT_LORA' in os.environ:`, so dropping layer 1 also drops the link flag — no link error, but LoRa silently disappears from the build.
  - **Uncoupled** (`pr-231`): `RadioLib` is hard-coded in the base `REQUIREMENTS` list, so disabling layer 1 alone leaves a bare `-lRadioLib` → **link failure**.

  Practical rule: after touching LoRa, run one build and grep the link line for `RadioLib`. Which defaults are `=y` versus commented out also varies per branch — **always go by the current branch's actual contents**, never by this document.

- **Whether CI publishes prereleases automatically**, and which workflow files exist, vary by branch.
- **Whether `.workbuddy/` is git-ignored** varies by branch (some branches lack that line).

### 10.2 Semantic traps

- **`switch_right()` selects the previous item**: `UILaunchPage::switch_right` (`ui/ui_launch_page.cpp`) moves cards visually right and selects the **previous** entry, triggered by **←**; `switch_left()` is the one that advances. This is the easiest thing to get backwards when editing carousel navigation.
- **`page_app.h` is a build artifact**: hand edits are overwritten by the next `scons` run, and it must not be committed.
- **A `page_app/*.cpp` missing `#define APP_PAGE_IMPLEMENTATION_UNIT`** pulls in conflicting headers.
- **There is no `ui_app_page.hpp` in `main/ui/`**: the page base classes live in `ext_components/cp0_lvgl/include/`.
- **`ui_app_setup.hpp` does not exist**: the settings page is `UISettingTreePage` in `main/ui/settings/settings_page.cpp`.

### 10.3 Untracked leftover directories — do not `git add`

`git status` will always show these two untracked directories:

- `projects/FactoryTest/` (containing `main_FactoryTest/`, whose `.git` points into `.git/modules/...`)
- `projects/LaunchWizard/main_Keyboard_Guide/`

Both are **leftover clones of removed submodules** and are not part of the current build. If you need Keyboard-Guide, initialize the official path `projects/Keyboard-Guide/main_Keyboard-Guide` (currently an empty directory).

Likewise, submodule pointers (`M SDK`, `M projects/AppStore`, etc.) are often left unstaged as "new commits" — **do not commit them casually**.

### 10.4 Build / environment

- Running `scons` from the repo root → fails.
- Switching profiles without `distclean` → compiled with the wrong config.
- Changing resources without rebuilding → `dist/` still holds the old ones.
- Missing `CONFIG_REPO_AUTOMATION=y` → CI/automation hangs on `input()`.
- A trailing newline in `sdk_version.txt` → malformed sysroot path.
- Changing a third-party pin without deleting `SDK/github_source/<repo>` → the old sources are still used.
- `bbear` does not exist (use `bear`).
- Cross-compiling without `aarch64-linux-gnu-g++`.
- `scripts/create_settings_ui_worker.sh` is a **dead script** (the template `projects/ui_test` it copies was removed).

### 10.5 Known outdated documentation

| Location | Problem |
| --- | --- |
| `docs/launcher工程详细说明/12-*` and the en/ja versions | The `BUILTIN_APPS[]` sample is outdated (actual: Settings/Store/CLI/Python/SSH/IP Panel/Calculator/Snake/Tank); the referenced `ui_app_setup.hpp`, `main/ui/ui_app_page.hpp`, and old `page_app.h` paths do not exist |
| Several guides | Hardcoded absolute paths such as `/home/nihao/w2T/...` and `/home/pi/launcher` |
| `docs/.../08-build-and-compilation-guide.md` | Still says `static_lib_v0.0.6` (actual: `v0.0.7`); some FLAGS descriptions do not match the real `config_tmp.mk` |
| `docs/.../09-packaging-deployment-and-systemd.md` | Claims packaging prefers the source tree (it actually uses `dist/APPLaunch`); says nothing about the updater systemd unit / polkit / `/usr/libexec/applaunch-updater` |
| `docs/launcher-project-guide-ja/` parts 02/04/05/12 | **Pre-refactor versions** (they reference `ui_init()`, `LauncherFonts`, `make_photo`, etc., which no longer exist) — do not use them as implementation references |
| `docs/current-implementation-and-components.md` | Still treats FactoryTest as an existing submodule; the `source-audit/README.md` link is broken |
| `docs/settings_ui_integration_task_assignment.md` | Outdated historical task sheet (references the deleted `docs/settings_ui_tasks/` and `projects/ui_test`) → **do not link to it** |
| `projects/APPLaunch/AGENTS.md` | Its `bbear` command cannot run on this machine |

---

## 11. Documentation Map

`docs/` contains **three parallel 13-part sets (zh/en/ja)** whose numbering maps 1:1.
⚠️ **The Chinese set uses Chinese filenames; the English and Japanese sets use English filenames** (they are not identically named):

| No. | Chinese file (`docs/launcher工程详细说明/`) | EN/JA file (`docs/launcher-project-guide{,-ja}/`) | Topic |
| --- | --- | --- | --- |
| 00 | `00-总览与阅读路线.md` | `00-overview-and-reading-path.md` | Positioning, reading order, terminology |
| 01 | `01-工程目录与模块职责.md` | `01-project-layout-and-module-responsibilities.md` | Directories, module dependencies, code style |
| 02 | `02-运行框架与启动流程.md` | `02-runtime-framework-and-boot-flow.md` | `main()` → `cp0_lvgl_run()` → first frame |
| 03 | `03-UI框架与首页轮播.md` | `03-ui-framework-and-home-carousel.md` | `UILaunchPage`, slots, switching/recentering |
| 04 | `04-应用模型与启动机制.md` | `04-application-model-and-launch-mechanism.md` | `app` model, launch paths, `.desktop` |
| 05 | `05-内置页面框架.md` | `05-built-in-page-framework.md` | `AppPage`, lifecycle, adding a page |
| 06 | `06-资源与配置系统.md` | `06-resources-and-configuration.md` | Resource tree, path resolution, config keys |
| 07 | `07-输入系统与按键映射.md` | `07-input-system-and-key-mapping.md` | Keyboard thread, evdev→LVGL, F/X/Z/C |
| 08 | `08-构建与编译指南.md` | `08-build-and-compilation-guide.md` | Build matrix, environment variables, common errors |
| 09 | `09-打包部署与systemd.md` | `09-packaging-deployment-and-systemd.md` | `.deb`, systemd, `scons push` |
| 10 | `10-扩展开发指南.md` | `10-extension-development-guide.md` | Adding pages/apps/resources/toggles |
| 11 | `11-调试与故障排查.md` | `11-debugging-and-troubleshooting.md` | Blank screen / resources / input / external apps |
| 12 | `12-常用修改入口速查.md` | `12-common-modification-entry-points.md` | Change→file→verification tables |

```
docs/launcher工程详细说明/        # Chinese (Chinese filenames) — first choice, closest to the code today
docs/launcher-project-guide/     # English — second choice; part 12 is a condensed table
docs/launcher-project-guide-ja/  # Japanese — parts 02/04/05/12 are old; do not use as implementation references
docs/launcher工程详细说明.md / launcher-project-guide.md / launcher-project-guide-ja.md  # three index pages
```

**All three guide sets were batch-snapshotted on 2026-09-23**; the code has kept refactoring since (settings pages migrated, the LoRa entry point removed, `launcher_platform::path()` replacing the old API). **Treat them as architecture tours, not API references.**

**Topical docs** (newer than the guides and mostly kept in sync with the code):

| File | Content |
| --- | --- |
| `docs/cp0_lvgl.md` / `.en.md` / `.ja.md` | ★ Complete `cp0_signal_*` usage, arguments, return formats, threading rules (required reading before touching the platform layer) |
| `docs/current-implementation-and-components.md` | Implementation snapshot + component/dependency inventory |
| `docs/OPEN_SOURCE_COMPONENTS.md` + repo-root `LICENSES/` | Third-party components and license registration |
| `docs/automation-rpc.md` | Local automation RPC (key injection, screenshots) |
| `docs/adb-*.md` (4 files) | ADB debugging / shell / capabilities / hotplug |
| `docs/APPLaunch-App-打包指南.md` / `-ja.md` | App packaging (external apps as `.deb`) |
| `docs/file-transfer-performance.md` | File transfer performance |
| `docs/macos-docker-build.md` / `-ja.md` | macOS + Docker builds |

---

## 12. Task Routing Table

| I want to… | Read first | Then work on |
| --- | --- | --- |
| Get it running on the host | `README_ZH.md` build section + §2 of this file | `projects/APPLaunch` with `linux_x86_sdl2_config_defaults.mk` |
| Produce device/release artifacts | §4.4 of this file + `docs/.../09-*.md` (mind the outdated parts) | `scripts/build_cardputerzero_release_local.sh` → `scripts/debian_packager.py` |
| Add a home-screen app (built-in page) | §6.2 of this file + `docs/.../10-extension-development-guide.md` | `main/ui/page_app/` + `builtin_app_registry.cpp` |
| Add an external app (`.desktop`) | §6.3 of this file | `projects/APPLaunch/APPLaunch/applications/` |
| Change the carousel / home UI | `docs/.../03-ui-framework-and-home-carousel.md` | `main/ui/ui_launch_page*.cpp`, `animation/` |
| Change the settings page | §5.3 of this file, "Settings menu structure" | `main/ui/settings/` |
| Change keys/input | `docs/.../07-input-system-and-key-mapping.md` | `main/ui/ui_launch_page_input.cpp`, `model/global_hint_policy.cpp` |
| Change platform capabilities (audio/network/process/config…) | `ext_components/cp0_lvgl/AGENTS.md` + `docs/cp0_lvgl.md` | `ext_components/cp0_lvgl/`, syncing the trilingual docs + contracts + tests |
| Change the camera | `docs/current-implementation-and-components.md` | `projects/CameraApp/` (note `main_CameraApp/` is a submodule) |
| Change the emulator | `projects/CardputerZero-Emulator/` | CMake, not SCons |
| Debug a blank screen / boot failure | `docs/.../11-debugging-and-troubleshooting.md` | `journalctl --user -u APPLaunch.service -f` |
| Debug an external app that never returns | §6.1 of this file + `tests/test_external_framebuffer_ownership.py` | `main/ui/launch.cpp` |

---

## 13. Collaboration and Commits

- **Commit messages**: Conventional Commits — `type(scope): one-line summary`, with type ∈ `feat|fix|chore|refactor|docs|ci|test|perf|style`; details go in the body as `-` bullets. Examples (from `git log`): `fix(audio): stop the saved-volume restore when audio tears down`, `chore(config): disable RadioLib/LoRa by default and pin RadioLib commit`.
- **Branches**: prefixes `fix/`, `feat/`, `feature/`, `refactor/`, `ci/`, `codex/`; PRs land on `master` via merge commits; release tags are `launcher-vX.Y.Z`.
- **There is no CONTRIBUTING file, no PR template, and no issue template.**
- **Licensing**: the root [`LICENSE`](LICENSE) is MIT (M5Stack code). [`LICENSES/`](LICENSES/) holds verbatim third-party texts plus provenance and **does not relicense this repo**.
  New third-party components must be registered following [`docs/OPEN_SOURCE_COMPONENTS.md`](docs/OPEN_SOURCE_COMPONENTS.md): the verbatim text in `LICENSES/` + a provenance row in `LICENSES/README.md` + a row in `OPEN_SOURCE_COMPONENTS.md` (plus `ext_components/THIRD_PARTY_NOTICES.md` if the component lives under `ext_components/`).
- **Never commit**: `build/`, `dist/`, `compile_commands.json`, `.workbuddy/`, or the leftover directories from §10.3.
- **Multi-agent / shared working tree**:
  1. run `git status --short` before touching anything;
  2. **never `git add -A`** — stage only the files you changed;
  3. **do not switch or reorder branches**, do not `git stash` other people's changes, do not `git checkout -- .`;
  4. if you find changes you do not recognize, stop and ask — do not overwrite them.

### Commit boundaries (submodules)

`SDK`, `projects/AppStore`, `projects/Calculator`, `projects/CameraApp/main_CameraApp`, `projects/Keyboard-Guide/main_Keyboard-Guide`, and `projects/CardputerZero-Emulator/lib/lvgl` are **git submodules**:

- changes inside them must be committed to **their own upstream repositories**; `git add` in the parent repo only records a gitlink;
- an `M <submodule>` entry here usually just means "pointer drift" and does not indicate a substantive change;
- `projects/Keyboard-Guide/main_Keyboard-Guide` may be empty in the current checkout (uninitialized); run `git submodule update --init` if you need it.

### How `ext_components` is consumed

- Among `ext_components`, APPLaunch **depends directly only on `cp0_lvgl`**. The base `REQUIREMENTS` list in `main/SConstruct` is `cp0_lvgl`, `eventpp`, `utilities`, `lvgl_component`, `freetype`, `pthread` — the latter five are SDK or system libraries, not `ext_components`. `Miniaudio` is pulled in transitively by `cp0_lvgl` via `CONFIG_CP0_LVGL_INIT_AUDIO`.
- `SmoothUI` / `Spdlog` are used only by Keyboard-Guide; `bluectl` / `nmtui` are currently **enabled by no project** (do not treat them as default dependencies); `Sigslot` is never `#include`d and serves only as an OR-gate in cp0_lvgl; `eventpp` is an SDK component and a global dependency.
- The cache of upstream-fetched components **does not validate versions**; after changing a pin you must delete `SDK/github_source/<repo>`.

---

## 14. Pre-Delivery Checklist

Before handing off, confirm each of the following:

- [ ] `git status --short` lists only files I intended to change (no `build/`, `dist/`, leftover directories, or submodule drift)
- [ ] `sh projects/APPLaunch/tests/run_tests.sh` passes (if APPLaunch was touched)
- [ ] `sh ext_components/cp0_lvgl/tests/run_tests.sh` passes (if the platform layer was touched)
- [ ] The SDL2 build passes: `cd projects/APPLaunch && CONFIG_DEFAULT_FILE=linux_x86_sdl2_config_defaults.mk scons -j$(nproc)`
- [ ] New UI text is at least 12px (a LaunchWizard hard rule; applies elsewhere too on a 320×170 screen)
- [ ] New/modified files carry an SPDX header
- [ ] If a `cp0_signal_*` signature or return format changed: trilingual docs + `*_contract.*` + C wrappers + all call sites + tests are all updated
- [ ] If resources changed: rebuilt, and `dist/APPLaunch/` actually contains the new files
- [ ] The commit message follows Conventional Commits, and no one else's changes were swept into the commit
