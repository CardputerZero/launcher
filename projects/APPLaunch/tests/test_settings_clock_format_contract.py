# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT

"""Contracts for the launcher's US 12-hour clock option.

The setting lives in the APPLaunch settings tree, but the clock is drawn by the
cp0_lvgl top bar, so the two sides must not drift apart:

* both read the same shared config key (``CP0_CONFIG_KEY_CLOCK_12H``) and
  neither hard-codes the key string;
* ``cp0_time_str()`` keeps its documented 24-hour ``HH:MM`` contract, because
  the Date & Time page parses that value; the option only changes presentation;
* the toggle persists through ``SetInt`` + ``Save`` with a rollback, like the
  other config-backed toggles in the settings tree.

The checks are static (regex over the sources) so they need no toolchain.
"""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REPO = Path(__file__).resolve().parents[3]
SETTINGS_PAGE = (ROOT / "main/ui/settings/settings_page.cpp").read_text(encoding="utf-8")
TOP_BAR = (REPO / "ext_components/cp0_lvgl/src/ui_app_page.cpp").read_text(
    encoding="utf-8"
)
PUBLIC_API = (REPO / "ext_components/cp0_lvgl/include/cp0_lvgl_app.h").read_text(
    encoding="utf-8"
)
TIME_FORMAT = (REPO / "ext_components/cp0_lvgl/src/cp0_time_format.cpp").read_text(
    encoding="utf-8"
)
ASSETS_HEADER = (
    REPO / "ext_components/cp0_lvgl/include/cp0_lvgl_app_page_assets.h"
).read_text(encoding="utf-8")
TOP_BAR_HEADER = (
    REPO / "ext_components/cp0_lvgl/include/ui_app_page.hpp"
).read_text(encoding="utf-8")


def date_time_block() -> str:
    start = SETTINGS_PAGE.index("NodeIter date_time =")
    system = SETTINGS_PAGE.index("NodeIter system =", start)
    return SETTINGS_PAGE[start:system]


def test_time_format_toggle_is_under_date_and_time():
    block = date_time_block()
    assert 'SettingEntry clock_format{"12H Clock", clock_12h_api, true};' in block
    assert "clock_format.status_read_policy = SettingStatusReadPolicy::Direct;" in block
    assert "mode_tree.append_child(date_time, std::move(clock_format));" in block
    # It must sit after the manual editor: the "Set Manually" workflow is
    # guarded by its own activation gate and must not be reordered.
    assert block.index('SettingEntry clock_format{"12H Clock"') > block.index(
        "NodeIter write_rtc"
    )


def test_setting_and_top_bar_share_one_config_key():
    assert '#define CP0_CONFIG_KEY_CLOCK_12H "clock_12h"' in PUBLIC_API
    # Neither side hard-codes the key string; both use the shared constant.
    assert '"clock_12h"' not in SETTINGS_PAGE
    assert '"clock_12h"' not in TOP_BAR
    assert "CP0_CONFIG_KEY_CLOCK_12H" in SETTINGS_PAGE
    assert "CP0_CONFIG_KEY_CLOCK_12H" in TOP_BAR


def test_toggle_persists_with_a_rollback():
    assert '{"GetInt", kClock12HourConfigKey, "0"}' in SETTINGS_PAGE
    assert '{"SetInt", kClock12HourConfigKey, twelve_hour ? "1" : "0"}' in SETTINGS_PAGE
    assert 'cp0_signal_config_api({"Save"}' in SETTINGS_PAGE
    # SetInt mutates the live config immediately, so a failed Save must restore
    # the previous value instead of leaving an unpersisted preference.
    assert (
        "if (write_clock_12h_config(previous)) (void)save_clock_12h_config();"
        in SETTINGS_PAGE
    )


def test_top_bar_only_changes_the_presentation():
    # The 24-hour value keeps coming from the documented cp0_time_str() API.
    assert "cp0_time_str(text, sizeof(text));" in TOP_BAR
    assert "cp0::time::format_12_hour(text, formatted, sizeof(formatted))" in TOP_BAR
    assert "void cp0_time_str(char *buf, int buf_size);" in PUBLIC_API
    # The formatter only reformats the five-character HH:MM text.
    assert "hh_mm.size() != 5" in TIME_FORMAT
    assert "hh_mm[2] != ':'" in TIME_FORMAT


def test_12_hour_pill_uses_the_wider_background():
    assert "LV_IMAGE_DECLARE(cp0_status_time_background_wide);" in ASSETS_HEADER
    assert "cp0_status_time_background_wide" in TOP_BAR
    assert "static constexpr int kTwelveHourWidth = 68;" in TOP_BAR_HEADER
    assert "static constexpr int kTwentyFourHourWidth = 40;" in TOP_BAR_HEADER


if __name__ == "__main__":
    for _name, _test in sorted(globals().items()):
        if _name.startswith("test_") and callable(_test):
            _test()
