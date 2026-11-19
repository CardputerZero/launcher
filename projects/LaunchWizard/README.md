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

## Timezone modes

The timezone list shows only standard UTC offsets, without city or region names.
After selecting an offset, choose a time mode:

- **Winter** (default): keep the displayed standard offset throughout the year.
- **Summer**: keep the mapped daylight saving offset throughout the year. This
  option is offered only for mappings with a configured daylight saving adjustment,
  which may be 30 minutes rather than one hour.

For the UTC-05:00 selection, Winter always uses UTC-05:00 and Summer always
uses UTC-04:00. UTC+08:00 offers only Winter. The internal IANA mapping is used
only to determine whether a Summer option exists; it is not shown in the UI.

Manual modes use fixed `Etc/GMT` zones for whole hours. Fractional offsets are
compiled using `zic` (provided by `libc-bin` on the Debian device image) into
`/usr/share/zoneinfo/LaunchWizard/`. Applying a timezone requires `timedatectl`
and root access; failure stops setup and offers retry. Host timezone tests use
temporary files and never change the host's timezone.

## Hostname

Applying the hostname writes `/etc/hostname` through `hostnamectl` and updates
the `127.0.1.1` mapping in `/etc/hosts` in place. Other hosts entries
(localhost, IPv6 aliases, user additions) are preserved, and a missing mapping
is appended. Without that mapping, tools such as `sudo` print
`unable to resolve host <name>` on every invocation. Re-running the wizard from
APPLaunch's "Run Setup Again" re-applies the hostname step, which repairs
devices provisioned by older images.

## Root filesystem expansion

On the first boot the wizard grows the root filesystem just before it finishes.
The expansion is the second-to-last step of the apply pipeline: it runs after
the timezone, hostname, account, network, and SSH steps and before the final step
that disables `LaunchWizard.service` and drops the OOBE markers, so it always
happens before the restart that ends the OOBE. It runs the packaged
`/usr/bin/raspi-config --expand-rootfs`, which recreates the root partition with
the card's full length and installs `/etc/init.d/resize2fs_once`; the actual
`resize2fs` then runs during that reboot.

The step only runs while the OOBE is configuring the device. It is skipped when
the OOBE was re-armed from APPLaunch's "Run Setup Again" entry, because that card
is already expanded and its partition table must not be rewritten again; a
factory marker that is still present always wins, so a re-arm that races an
unfinished first boot cannot leave the card unexpanded.

### Skipped first boot

A factory image that was provisioned before its first boot (Raspberry Pi Imager
renamed the user or set a password) skips the OOBE, so the apply pipeline — and
with it the expansion step — would never run. After the keyboard guide, the skip
path therefore expands the root filesystem itself and reboots instead of starting
APPLaunch: it runs the same `/usr/bin/raspi-config --expand-rootfs`, drops the
OOBE markers, and issues `systemctl reboot`, so `resize2fs_once` grows the
filesystem during that reboot. The next boot sees no factory marker and hands off
to APPLaunch without touching the partition table. If the expansion cannot be
scheduled, or the marker cannot be dropped, the wizard falls back to the normal
handoff and warns on stderr rather than rebooting into a repeat expansion.

The image gets exactly one expansion attempt for its whole lifetime. Before
`raspi-config` runs, the wizard records the attempt in
`/var/lib/LaunchWizard/rootfs-expansion-attempted` and never removes it, so a
successful expansion, a failed one, and an interrupted apply or reboot all
consume the attempt: nothing retries it automatically, neither a re-run of the
OOBE nor a later boot. To retry a genuinely failed expansion by hand, run
`sudo raspi-config --expand-rootfs` and reboot (the marker only stops the wizard,
not the command).

Expansion is best effort. `raspi-config` exits 0 even when it refuses to
repartition (for example when the root partition is not the last one), so the
wizard verifies that `resize2fs_once` was installed, warns on stderr otherwise,
and never blocks the rest of the setup. If the apply is interrupted after this
step, the init script stays armed and the next boot still expands the card; if
the command itself failed, the one-shot attempt is already consumed, so the card
stays unexpanded until the command is run manually.
The image must provide the `raspi-config` package with `parted`, `fdisk`, and
`resize2fs` (`e2fsprogs`).

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
