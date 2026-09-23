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
native LVGL path. The lock-screen filter runs before the configurable key filter,
custom screen events, global shortcuts, and native keypad delivery. A key it
consumes reaches none of those paths, including during the unlock animation.

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

## Lock Screen (Not the Removed Bouncing Screensaver)

`ui_screensaver.*`, `model/screensaver_model.*`, and the `ui_screensaver_*`
entry points retain their legacy names, but only implement the lock screen.
Do not remove them or the keyboard backend hooks when cleaning up the removed
bouncing-image screensaver. `home_icon_fallback.*` is an independent embedded
home icon; the lock screen uses its own cached `share/images/lockscreen.png`.

Initialize the lock screen after `launcher_ui::init()` and deinitialize it in
`launcher_ui::deinit()`. `Launch::launch_Exec()` calls
`ui_screensaver_set_foreground(0)` before handing off input/timers to an external
process and `ui_screensaver_set_foreground(1)` after restoring home. This clears
the lock and restarts the idle interval; it does not resume a previous lock state.

`settings/settings_screen_timeout_page.*` implements Screen -> DarkTime.
It reads and saves `dark_time` through `GetInt`, `SetInt`, and `Save`, with
rollback on write/save failure. Never/10S/30S/60S/300S map to 0/10/30/60/300
seconds. The default is 30 seconds; 0 disables automatic locking only.
The lock-screen runtime reads this setting during its idle checks.

While unlocked, the filter observes TAB without consuming its press/release,
so the page retains the short-tap behavior. After 500 ms it shows the persistent
`Hold TAB for 3s to lock` toast; at 3000 ms it enters the lock. Release, another
key, or a lock transition clears the hold hint. Repeats must not restart the hold.

`model/lockscreen_state_model.hpp` owns the three states: full-screen black,
visible wallpaper waiting for TAB, and armed waiting for ENTER (or keypad ENTER).
Only a fresh press changes state. Any fresh press wakes black to the wallpaper;
TAB then arms and ENTER confirms. ESC from either visible state returns to black;
another key in the armed state returns to waiting for TAB. Press/repeat activity
resets the 10-second visible-state timeout; release does not. All lock-state
events and the 350 ms exit animation consume keys. Low-battery UI remains hidden
until `ui_screensaver_is_active()` becomes false, including that animation.

The lock is an `lv_layer_top()` overlay, not a page. Paint the black state over
the whole display even on backends whose backlight write is a no-op. Backlight
suspension/restoration uses `launcher_media_controls` without persisting zero
as the working brightness. The static wallpaper sits below the 20 px top bar;
its black-backed yellow unlock hint is a top-layer sibling over the title.
Cache and own the decoded wallpaper buffer until teardown. Animate exit using
the stored panel rectangle, not potentially stale LVGL layout geometry.

Keep `lock.mp3`, `select.mp3`, `blocked.mp3`, and `unlock.mp3` in `share/audio/`.
Register them with `RegisterSystemSounds` and play them via
`cp0_signal_system_play`, preserving the platform's startup/switch/enter slots.
Use `lock` on entering black, `select` on advancing, `blocked` on invalid input,
and `unlock` on confirmation. Lock entry suspends the system sound player;
exit prepares it asynchronously and completes cleanup before exposing the working screen.
