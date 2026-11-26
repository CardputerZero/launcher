# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT

"""Wiring contract for the screen-off power staging.

The launcher's ui_screensaver.cpp drives the stages; this pins the platform
half (status pause flag, LED/governor/panel helpers, the paused callbacks and
the network polling hold) and the packaging that makes the sysfs nodes
writable for the device user.
"""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLATFORM = ROOT / "../../ext_components/cp0_lvgl"

PAUSE_HEADER = (PLATFORM / "include/cp0_status_pause.h").read_text()
PAUSE_IMPL = (PLATFORM / "src/cp0_status_pause.c").read_text()
PAGE = (PLATFORM / "src/ui_app_page.cpp").read_text()
NETWORK = (PLATFORM / "src/cp0/cp0_lvgl_network.cpp").read_text()
SETTINGS_DEVICE = (PLATFORM / "src/cp0/cp0_lvgl_settings.cpp").read_text()
SETTINGS_SDL = (PLATFORM / "src/sdl/sdl_lvgl_settings.cpp").read_text()
APP_HEADER = (PLATFORM / "include/cp0_lvgl_app.h").read_text()
PACKAGER = (ROOT / "../../scripts/debian_packager.py").read_text()

# The pause flag crosses layers as a plain C symbol, same pattern as
# LVGL_RUN_FLAGE: set from the launcher, read from the top bar and the
# network worker.
assert "void cp0_status_pause_set(int paused);" in PAUSE_HEADER
assert "int cp0_status_pause_get(void);" in PAUSE_HEADER
assert "static volatile int s_status_pause" in PAUSE_IMPL

# The top bar's per-second clock, the 5 s status poll and battery events all
# stand down while the panel is dark; each callback gates before touching
# LVGL or the bus.
assert PAGE.count("cp0_status_pause_get()") >= 3
assert "void AppTopBarRegion::time_timer_cb" in PAGE
assert "void AppTopBarRegion::status_timer_cb" in PAGE
assert "void AppTopBarRegion::battery_event_cb" in PAGE

# The network worker holds *all* polling during the dark period (no 60 s
# hidden backstop either) and refreshes right after the flag clears.  Nothing
# here disconnects Wi-Fi; only observation pauses.
assert "#include \"cp0_status_pause.h\"" in NETWORK
assert "if (cp0_status_pause_get())" in NETWORK
assert "Wi-Fi" in NETWORK or "wifi" in NETWORK.lower()

# Device implementations exist next to the backlight sysfs helpers; the SDL
# simulator gets no-ops.
for symbol in (
    "cp0_status_led_suspend",
    "cp0_status_led_restore",
    "cp0_cpufreq_powersave_set",
    "cp0_cpufreq_wake_reset",
    "cp0_panel_fb_blank",
):
    assert f"int {symbol}(void)" in APP_HEADER or f"{symbol}(" in APP_HEADER
    assert f'"{symbol}"' not in SETTINGS_DEVICE
    assert f"{symbol}(void)" in SETTINGS_DEVICE or f"{symbol}(int" in SETTINGS_DEVICE
    assert symbol in SETTINGS_SDL

# The LED stage saves the active trigger token before detaching it, so the
# red status led comes back with its original behaviour.
assert "active_trigger_token" in SETTINGS_DEVICE
assert '"/sys/class/leds/ACT"' in SETTINGS_DEVICE
assert "FB_BLANK_POWERDOWN" in SETTINGS_DEVICE
assert "FB_BLANK_UNBLANK" in SETTINGS_DEVICE

# The package ships the tmpfiles rule granting the gpio group write access to
# the staged nodes, mirroring the image's cardputerzero-leds.conf, and the
# postinst applies it immediately.
assert "applaunch-power.conf" in PACKAGER
assert "scaling_governor" in PACKAGER
assert "0664 root gpio" in PACKAGER
assert "systemd-tmpfiles --create" in PACKAGER
assert "TMPFILES_PATH = PurePosixPath(\"usr/lib/tmpfiles.d\")" in PACKAGER
