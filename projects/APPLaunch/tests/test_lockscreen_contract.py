# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT

"""Wiring contract for the screensaver lock screen.

The state machine itself is covered by test_lockscreen_state_model.cpp, the panel
and backlight behaviour by the low_battery_ui harness (which drives the real
ui_screensaver.cpp), and the non-persisting backlight by
test_launcher_media_controls.cpp.  This only pins the wiring that is easy to lose
in a refactor.
"""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LOCK = (ROOT / "main/ui/model/lockscreen_state_model.hpp").read_text()
SOURCE = (ROOT / "main/ui/ui_screensaver.cpp").read_text()
CONTROLS = (ROOT / "main/ui/launcher_media_controls.cpp").read_text()
CONTROLS_HEADER = (ROOT / "main/ui/launcher_media_controls.h").read_text()
PAGE_SHELL = (ROOT / "../../ext_components/cp0_lvgl/include/ui_app_page.hpp").read_text()

# The lock states, their keys, and the idle timeout live in the model.
assert "enum class LockscreenState" in LOCK
assert "PendingUnlock" in LOCK and "Armed" in LOCK
assert "TimeoutMs = 10000" in LOCK
assert "KEY_TAB" in LOCK
assert "KEY_ENTER" in LOCK and "KEY_KPENTER" in LOCK
# A key that neither advanced nor confirmed is reported so the caller can play
# the blocked sound instead of the select sound.
assert "bool blocked = false;" in LOCK

# Sound feedback: lock on entering the black state, select when the lock advances,
# blocked for a key that does not, and unlock on the confirmation.
assert 'play_lockscreen_sound("lock.mp3")' in SOURCE
assert 'play_lockscreen_sound("blocked.mp3")' in SOURCE
assert 'play_lockscreen_sound("select.mp3")' in SOURCE
assert 'play_lockscreen_sound("unlock.mp3")' in SOURCE
assert "decision.blocked" in SOURCE

# The wiring is useless without the assets; they ship with the app tree, and the
# built-in decoders cover WAV/MP3/FLAC but not OGG.
for asset in ("lock.mp3", "blocked.mp3", "select.mp3", "unlock.mp3"):
    path = ROOT / "APPLaunch/share/audio" / asset
    assert path.is_file(), path
    assert path.read_bytes()[:3] == b"ID3", path

# Both entry points (the idle timeout and the long-press gesture) share one path,
# so the two can no longer drift apart.
assert "void enter_lockscreen()" in SOURCE
assert SOURCE.count("enter_lockscreen();") >= 2
assert "start_screensaver" not in SOURCE
assert "start_screen_off" not in SOURCE

# While the screensaver is up the lock screen owns every key and decides when the
# panel sleeps or unlocks.
assert "s_lock.handle_key(item->key_code, pressed, released, now)" in SOURCE
assert "s_lock.poll(now)" in SOURCE
assert "decision.unlock" in SOURCE
assert "show_lockscreen_panel()" in SOURCE
assert "sleep_lockscreen()" in SOURCE

# The long-press gesture stays observation-only while the screensaver is idle, and
# a pending hold is polled faster than the idle check.
assert "s_model.observe_hold_key(item->key_code, released, now)" in SOURCE
assert "s_model.poll_hold(now)" in SOURCE
assert "kHoldPollMs = 100" in SOURCE

# The panel geometry has a single source of truth in the page shell, and the
# visible lock states clear the top bar so the status bar stays part of the layout.
assert "kTopBarHeightPx" in PAGE_SHELL
assert "AppPageRoot::kTopBarHeightPx" in SOURCE

# The locked state paints pure black over the whole display, so the black screen
# never depends on the backlight actually dimming: the simulator, web and win32
# backends accept the write and dim nothing.
assert "ScreensaverPanel full_screen_panel()" in SOURCE
assert "apply_panel(full_screen_panel())" in SOURCE
assert "panel_layout" not in SOURCE

# The visible states show cached wallpaper, with step hints over the top-bar title.
assert "ScreensaverPanel wake_panel()" in SOURCE
assert "apply_panel(wake_panel())" in SOURCE
assert "TAB&ENTER to unlock" in SOURCE
assert "Press ENTER to unlock" in SOURCE
assert "show_hint(" in SOURCE and "hide_hint()" in SOURCE
assert 'launcher_platform::path("lofoten_320x150.png")' in SOURCE
assert "lv_draw_buf_dup(decoder.decoded)" in SOURCE
assert "panel.black ? nullptr : s_background_cache.image()" in SOURCE

# Unlocking slides the wallpaper away, restoring the backlight first.
assert "lv_anim_set_values(&animation, s_panel.y, -s_panel.height)" in SOURCE
assert "release_screen_off_backlight();" in SOURCE

# The legacy icon stays hidden and the wallpaper is static.
assert "lv_obj_add_flag(s_block, LV_OBJ_FLAG_HIDDEN)" in SOURCE
assert "kAnimationFrameMs" not in SOURCE

# The sounds ride the platform's system-sound player, which decodes each sound
# once, keeps the PCM and a warm engine, and plays on its own worker.  Do not
# hand-roll a second player here, and do not fall back to the uncached per-file
# path (that re-opens the audio device per play and swallows the sound's start).
assert 'cp0_signal_audio_api({"RegisterSystemSounds"' in SOURCE
assert "cp0_signal_system_play(asset);" in SOURCE
assert "launcher_platform::path_c" not in SOURCE
assert "std::thread" not in SOURCE

# ...which relies on the platform half of the contract staying in place.
PLATFORM = ROOT / "../../ext_components/cp0_lvgl/src"
PLAYER = (PLATFORM / "cp0/cp0_audio_system_sound_player.cpp").read_text()
PLAYER_HEADER = (PLATFORM / "cp0/cp0_audio_system_sound_player.hpp").read_text()
CONTRACT = (PLATFORM / "cp0_audio_api_contract.cpp").read_text()
AUDIO_DEVICE = (PLATFORM / "cp0/cp0_lvgl_audio.cpp").read_text()

# Decoded PCM is cached and reused; the slots can grow so the launcher's sounds
# get the same treatment as the platform's indexed ones.
assert "struct CachedSound" in PLAYER
assert "ma_audio_buffer_init" in PLAYER
assert "ma_engine_init" in PLAYER
assert "int add_named(const std::vector<std::string> &names)" in PLAYER_HEADER
assert 'command == "RegisterSystemSounds"' in CONTRACT
assert "void RegisterSystemSounds" in AUDIO_DEVICE

# The panel is driven dark on entry and restored on every exit path, including
# teardown and the object-delete callbacks.
assert "suspend_screen_off_backlight()" in SOURCE
assert SOURCE.count("release_screen_off_backlight()") >= 4

# Suspending must not persist the brightness, or unlocking restores the wrong
# value; the normal step-based control can never express zero.
assert "int suspend_backlight();" in CONTROLS_HEADER
assert "void restore_backlight(int raw);" in CONTROLS_HEADER
assert "write_backlight_raw(0)" in CONTROLS
assert "if (raw <= 0) return;" in CONTROLS
