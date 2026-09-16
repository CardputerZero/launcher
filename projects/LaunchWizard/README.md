# LaunchWizard

LaunchWizard is the CardputerZero first-boot setup application. Its own code uses
a conventional C++ project layout; the hardware GUI is integrated through the
repository SDK's SCons component system.

## Layout

```text
main/src/               Application and LVGL runtime entry points
main/ui/wizard_model.*  UI-independent Model and validation rules
main/ui/wizard_view.cpp LVGL View and interaction coordinator
main/ui/wizard_service.* Platform Service implementation
tests/                  Host-side unit tests
main/                   Thin SDK/SCons component adapter
```

## Host build and tests

The reusable, platform-independent code is a regular CMake target and does not
require the device SDK:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Consumers can link the `LaunchWizard::core` target through `add_subdirectory`
or an installed `find_package(LaunchWizard CONFIG REQUIRED)` package.

## GUI application

The complete executable depends on the repository-specific LVGL, display,
keyboard, audio, and radio components. Build it from this directory using the
SDK entry point:

```sh
# Native SDL development build (default on x86_64 Linux)
scons -j8

# CardputerZero cross build
CardputerZero=y scons -j8
```

The device service contract is:

```text
ExecStart=/usr/share/APPLaunch/bin/LaunchWizard
WorkingDirectory=/usr/share/APPLaunch
```

Install `dist/LaunchWizard` at that exact path and merge `dist/APPLaunch/` into
`/usr/share/APPLaunch/` to deploy the audio resources as well. The local release
script and CI bundle both into the APPLaunch package.

Both build paths follow the same `main/src` and `main/ui` layout as the
HelloWorld reference project.

The SDL acceptance pages can be opened directly with:

```sh
./dist/LaunchWizard --preview-configuring
./dist/LaunchWizard --preview-restart
```

The UI follows an MSV boundary: `WizardModel` owns setup state and validation,
platform operations are isolated from it, and the LVGL source owns view objects
and translates input events into model changes.

## Key sound

The wizard uses the same six active cues and per-cue volumes as Keyboard-Guide:

| Feedback | Local WAV | Trigger |
| --- | --- | --- |
| Typing | `launch-wizard-key.wav` | Text entry, navigation, back, and focus switching |
| Lock | `launch-wizard-lock.wav` | Hide a password |
| Unlock | `launch-wizard-unlock.wav` | Show a password |
| Error | `launch-wizard-error.wav` | Validation, connection, apply, or reboot failure |
| Confirm | `launch-wizard-notification.wav` | Confirm an action or complete a Wi-Fi connection |
| Complete | `launch-wizard-achievement.wav` | Successfully apply configuration |

Each initial key press selects one cue after processing the action; releases
and auto-repeat do not trigger another sound. Asynchronous results have their
own feedback. The CC0 files live in `APPLaunch/share/audio/`, alongside their
license notice. The tutorial's two currently unused assets (`mechanical-complete`
and `mechanical-progress-step`) are not included.
Like ZClaw, the project uses SCons `STATIC_FILES` to copy the resource tree to
`dist/APPLaunch/`. It has no build-time dependency on Keyboard-Guide's assets.
Playback resolves resources relative to the executable in both the `dist/`
layout and the installed `/usr/share/APPLaunch/` layout, regardless of the
working directory.

An exec'ed audio helper preloads the cues with miniaudio and plays them through
PulseAudio at 48 kHz stereo, with the same 1.5 master gain as the tutorial. New
feedback replaces the current sound, except that the completion cue is allowed
to finish before more key sounds play. When the wizard runs as root, only the helper drops to UID 1000
and connects to that user's audio session. Playback requests are nonblocking;
audio initialization failures are logged and leave the wizard usable silently.
The helper is stopped and reaped before account migration, restarted when apply
finishes, and stopped during UI teardown.
