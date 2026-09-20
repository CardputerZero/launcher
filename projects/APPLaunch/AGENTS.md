# APPLaunch development guidance

## Before code analysis: ensure `compile_commands.json` exists

Before analyzing the code, check whether `compile_commands.json` exists in this
directory (`projects/APPLaunch/compile_commands.json`). If it does not, follow
the cross-compilation instructions in `../../README_ZH.md` (repo root): load
the config matching your current platform, then run one cross-compilation:

```bash
export CONFIG_DEFAULT_FILE=linux_x86_cross_cp0_config_defaults.mk
bbear -- scons -j22
```

`linux_x86_cross_cp0_config_defaults.mk` corresponds to the current platform
(Linux x86_64 host, CP0 cross toolchain). Pick the matching
`*_config_defaults.mk` for your platform from `../../README_ZH.md` — for
example `linux_x86_sdl2_config_defaults.mk` for the SDL2 simulator on Linux.

This command generates `compile_commands.json` in this directory. Then use the
source files that actually participate in the cross-compilation (as listed in
`compile_commands.json`) to confirm which code the project really uses, before
working on the coding task.

## Scoped-enum conversions

`cp0_lvgl` exports the public header `cp0_enum_cast.h`. Include it directly in
any C++ file that uses the conversion macro:

```cpp
#include "cp0_enum_cast.h"
```

Use the general `CP0_ENUM_CAST(target_type, enum_value)` for an explicit
conversion from an `enum class` value. Convenience macros are available for
common targets, including `CP0_ENUM_CAST_INT`, `CP0_ENUM_CAST_SIZE_T`,
`CP0_ENUM_CAST_UINT8`, `CP0_ENUM_CAST_UINT16`, `CP0_ENUM_CAST_UINT32`, and
`CP0_ENUM_CAST_UINT64`:

```cpp
enum class LayoutMetric : int { Width = 320 };

constexpr int width = CP0_ENUM_CAST_INT(LayoutMetric::Width);
constexpr auto count = CP0_ENUM_CAST_SIZE_T(LayoutMetric::Width);
constexpr auto raw = CP0_ENUM_CAST(uint32_t, LayoutMetric::Width);
```

This macro is the shared replacement for repeated enum-only `static_cast`
expressions and conversion helpers that only wrap an enum cast. Use the
type-specific convenience macro when it exists; use the general macro for any
other target type. Do not add a project-local helper solely to wrap one of
these macros; if an existing helper is part of a broader API, keep it and use
the macro in its implementation.

Do not use these macros for pointer casts, arbitrary integer conversions, or
untrusted values read from an external API. Validate and range-check an
external integer before converting it to an enum. Pass a side-effect-free enum
value or enumerator as the macro argument, and do not redefine a project-local
macro with the same name.

The `cp0_lvgl` component publishes `include/` through its `SConstruct`
dependency, so consumers should include `cp0_enum_cast.h` by name rather than
using a relative path into `ext_components`. After migrating a conversion,
keep the surrounding API and numeric behavior unchanged and run the APPLaunch
tests/build.

## Keyboard input systems

APPLaunch has two keyboard delivery systems. They share the same CP0 keyboard
backend and `key_item` queue; they are not two independent device readers. A
single physical, SDL, or injected key can be delivered through both paths:

1. The native LVGL path produces `LV_EVENT_KEY` for the focused object in the
   active input group.
2. The CP0 custom path sends `LV_EVENT_KEYBOARD` to the active screen with a
   complete `struct key_item` as the event parameter.

### Choose the event by capability

Use the native LVGL `LV_EVENT_KEY` path for ordinary UI operations: focus and
group navigation, list or menu movement, button activation, confirmation,
cancellation, and standard widget behavior. Bind the callback to the object
that belongs to the page input group and read the key with `lv_event_get_key()`:

```cpp
static void handle_key(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_KEY) return;
    const uint32_t key = lv_event_get_key(event);
    if (key == LV_KEY_ESC) close_page();
}
```

The native path receives the context-normalized key and CP0-to-LVGL mapping,
such as `LV_KEY_UP`, `LV_KEY_DOWN`, `LV_KEY_LEFT`, `LV_KEY_RIGHT`,
`LV_KEY_ENTER`, and `LV_KEY_ESC`. Make sure the keypad indev is assigned to the
page input group and that the intended object is focused. Prefer this path when
the operation needs no physical-key identity, text, modifiers, or explicit
press/release/repeat state.

Use the custom `LV_EVENT_KEYBOARD` path for text entry, Unicode input, terminal
input, application shortcuts, global shortcuts, physical-key-specific behavior,
modifier combinations, games, and any action that distinguishes pressed,
released, and repeated states. Register it on the current/root screen and read
the event parameter as a `const struct key_item *`. The CP0 backend registers
the shared event ID during input initialization; consumers must use the existing
`LV_EVENT_KEYBOARD` value rather than hard-coding or registering a second ID.

```cpp
static void handle_keyboard(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) !=
            static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD)) return;
    const auto *item = static_cast<const struct key_item *>(
        lv_event_get_param(event));
    if (!item || item->key_state != KBD_KEY_PRESSED) return;
    if (item->mods & KBD_MOD_CTRL) handle_ctrl_shortcut(item->key_code);
    if (item->utf8[0] != '\0') append_text(item->utf8);
}
```

Bind that callback to the active screen after `LV_EVENT_KEYBOARD` has been
registered, and retain the descriptor for teardown:

```cpp
lv_obj_t *keyboard_root = lv_screen_active();
if (keyboard_root && LV_EVENT_KEYBOARD != 0) {
    keyboard_event_dsc = lv_obj_add_event_cb(
        keyboard_root, handle_keyboard,
        static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), page_state);
}
```

Include `keyboard_input.h` when using this path. The useful fields are
`key_code`, `semantic_key`, `key_state`, `utf8`, `codepoint`, `mods`, `keysym`,
`sym_name`, and `input_context`. Use `key_code` for the physical Linux `KEY_*`
identity, `semantic_key` for context-normalized navigation, `utf8` for text,
`KBD_KEY_PRESSED`/`KBD_KEY_RELEASED`/`KBD_KEY_REPEATED` for state, and
`KBD_MOD_*` for modifiers. APPLaunch callbacks may use the helpers in
`main/ui/ui.h`, but only after confirming that the event is
`LV_EVENT_KEYBOARD`.

The Cardputer uses a shared keyboard layout. In the default navigation
context, the physical keys map as `KEY_F` → up, `KEY_X` → down, `KEY_Z` → left,
and `KEY_C` → right. Custom `LV_EVENT_KEYBOARD` handlers that process
physical repeat events should match the raw `key_code` when they need these
keyboard-specific controls. The native `LV_EVENT_KEY` path receives the
context-normalized LVGL key and can continue using `LV_KEY_UP`/`LV_KEY_DOWN`/
`LV_KEY_LEFT`/`LV_KEY_RIGHT`.

Do not parse `LV_EVENT_KEY` with `keyboard_item()` or cast its parameter to
`key_item`; native LVGL key events have different parameter semantics and must
be read with `lv_event_get_key()`. Conversely, do not expect
`LV_EVENT_KEYBOARD` to provide the focused widget behavior of an LVGL group.

### Dual delivery and interception

Without interception, CP0 dispatches `LV_EVENT_KEYBOARD` first and then returns
the mapped key to LVGL, which can produce `LV_EVENT_KEY` and related widget
events. Do not bind the same action to both paths unless ownership and duplicate
suppression are explicit. Calling `lv_event_stop_processing()` in the custom
callback stops only that event's propagation; it does not cancel the later
native LVGL path. A key consumed by the screensaver filter is delivered to
neither business path.

The screensaver filter also observes one key it does not own: while idle it
tracks the press and release of the long-press gesture that enters the lock and
reports them as *not* consumed, so the page underneath keeps the short-press
meaning of that key. The gesture is TAB held for 3000 ms; the model announces it
with `Hold TAB for 3s to lock` on the shared launcher toast from its hint delay (500 ms)
until the hold ends. Clear that toast on release, on a different key, and when
the lock is entered or left. Once the gesture matures the lock takes every key over, and
because only a fresh press changes lock state, the held key's repeats and its
release cannot walk the machine. Do not make the idle observation path consume a
key, and keep every observed press paired with its release.

The screensaver panel is an `lv_layer_top()` overlay, not a page, and it is a lock
screen. While it is up the lock owns every key, so page-level shortcuts never see
them. The states live in `model/lockscreen_state_model.hpp`: (1) locked paints
pure black over the whole display, (2) any fresh press shows the cached Lofoten
wallpaper below the page's own top bar and asks for the next step, (3) TAB
arms the unlock and ENTER confirms it. Any state with no input for 10 s falls back
to (1), and any press restarts that countdown. Only a fresh press changes state —
a repeat is activity and a release does nothing — which is what keeps the held TAB
that entered the lock from walking the machine on its own release.

Because (1) paints black itself, the black screen never depends on the backlight:
the simulator, web and win32 backends accept `BacklightWrite 0` and dim nothing,
so a panel that relied on it would be fully visible there. Driving the backlight
down is a power optimisation, and the visible states restore it before showing the
wallpaper. The 320x150 wallpaper starts at
`AppPageRoot::kTopBarHeightPx`, which leaves the page's top bar visible. Unlocking
slides the wallpaper away; idle lock states do not drive animation frames. Drive the
exit animation from the stored panel rectangle instead of
`lv_obj_get_height()`/`lv_obj_get_y()`: an object that has not been through a
layout pass reports zero geometry, which silently skips the animation.

The unlock hint is a black-backed yellow label covering the top bar's title,
leaving its network, clock and battery visible. It is a sibling on `lv_layer_top()`
so the wallpaper's bounds cannot clip it. Move it above the wallpaper on wake,
hide it on sleep/exit, and delete it when the wallpaper overlay is deleted.
Use `Press TAB to unlock` and `Press ENTER to unlock` for the two visible states.
The supplied JPEG is packaged as `lockscreen.png` for the existing PNG
decoder; own its decoded draw buffer until teardown and reuse it on every wake.

The lock screen has four sounds under `share/audio/` (MP3, because the built-in
decoders cover WAV/MP3/FLAC but not OGG): `lock.mp3` whenever the panel enters the
black state, `select.mp3` when a press advances the lock, `blocked.mp3` when a
press does neither — the model reports that as `blocked` — and `unlock.mp3` on the
confirmation. They are registered once with `RegisterSystemSounds` and then played
by name with `cp0_signal_system_play`, which puts them on the platform's
system-sound player: it decodes each sound once, keeps the decoded PCM and a warm
engine, and plays on its own worker thread. Do not route them through the
unregistered per-file fallback — that re-opens the audio device on every play and
swallows the start of a short sound while the sink settles — and do not hand-roll a
second player. Registration appends after the platform's three indexed slots, so
the launcher's startup/switch/enter sounds keep their indices, and the indexed
`SystemSoundPlay` contract stays limited to 0..2.

Text-entry and other custom-input modes should suppress the native group path
while retaining `LV_EVENT_KEYBOARD`:

```cpp
const auto previous_context = cp0_keyboard_get_input_context();
const int previous_intercept = cp0_keyboard_get_lvgl_keypad_intercept();
cp0_keyboard_set_input_context(KBD_INPUT_CONTEXT_TEXT);
cp0_keyboard_set_lvgl_keypad_intercept(1);

// Restore both values on every exit and teardown path.
cp0_keyboard_set_input_context(previous_context);
cp0_keyboard_set_lvgl_keypad_intercept(previous_intercept);
```

`cp0_keyboard_set_lvgl_keypad_intercept(1)` suppresses only the native
keypad/group delivery. The custom screen event and global key handler remain
active. Long-press and repeat-sensitive behavior should use
`LV_EVENT_KEYBOARD`, because `key_item` exposes `KBD_KEY_REPEATED` while LVGL's
native indev state is only pressed or released.

The `key_item *` event parameter is owned by the input dispatcher and is freed
after synchronous dispatch. Never retain it, capture it in an asynchronous
callback, or pass it to a worker thread. Copy the required scalar fields and
UTF-8 bytes before leaving the event callback. Remove screen event descriptors
before their owning page is destroyed, and restore any saved input context and
intercept state during teardown.

The implementation sources of truth are
`ext_components/cp0_lvgl/src/cp0/cp0_keyboard_lvgl_input.c` for device builds,
`ext_components/cp0_lvgl/src/sdl/sdl_lvgl_keyboard.c` for SDL builds, and
`ext_components/cp0_lvgl/include/keyboard_input.h` for the custom event
contract.
