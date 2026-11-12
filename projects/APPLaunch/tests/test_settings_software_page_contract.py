# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODEL = (ROOT / "main/ui/settings/settings_software_model.cpp").read_text(
    encoding="utf-8"
)
PAGE = (ROOT / "main/ui/settings/settings_system_page.cpp").read_text(
    encoding="utf-8"
)

# Packages installed by pi-gen/stage2/05-cardputerzero/03-run.sh.
PACKAGES = [
    "m5cardputerzero-recorder",
    "m5cardputerzero-compass",
    "camera",
    "factorytest",
    "m5cardputerzero-files",
    "m5cardputerzero-music",
    "m5cardputerzero-ir-chat",
    "m5cardputerzero-piano",
    "m5cardputerzero-ir-remote",
    "m5cardputerzero-cap-cc1101-subg-chat",
    "m5cardputerzero-cap-cc1101-nfc",
    "m5cardputerzero-cap-lora-1262",
    "m5cardputerzero-cap-lora-1262-gps",
    "m5cardputerzero-keyboard-guide",
]

for package in PACKAGES:
    assert package in MODEL, f"missing software package: {package}"

assert "read_installed_apps" in MODEL
assert "parse_check_result" in MODEL
assert 'constexpr const char *prefix = "m5cardputerzero-";' in MODEL

assert '#include "settings_software_updater.hpp"' in PAGE
assert '#include "settings_http_worker.hpp"' in PAGE
assert "settings_software::check_software" in PAGE
assert "settings_http::download_batch" in PAGE
assert "prefetch_software_http" in PAGE
assert (ROOT / "main/ui/settings/settings_http_worker.cpp").exists()
assert (ROOT / "main/ui/settings/settings_http_worker.hpp").exists()
sconstruct = (ROOT / "main/SConstruct").read_text(encoding="utf-8")
assert "APPLAUNCH_USE_LIBCURL" in sconstruct
assert ":libcurl.so.4" in sconstruct
assert "CONFIG_APPLAUNCH_LINUX_X86_CROSS_CP0" in sconstruct
assert "settings_software::read_installed_apps()" in PAGE
assert 'lv_msgbox_add_title(state->dialog, "Updates available")' in PAGE
assert "settings_software::update_line(update)" in PAGE
assert 'lv_msgbox_add_footer_button(state->dialog, "Update apps")' in PAGE
assert 'lv_msgbox_add_footer_button(state->dialog, "Update APPLaunch")' in PAGE
assert 'lv_msgbox_add_footer_button(state->dialog, "Not now")' in PAGE
assert "SoftwareDialogOption::Apps" in PAGE
assert 'start_process_request' in PAGE
assert 'applaunch-apps-updater.service' in PAGE

# The platform layer must not receive APPLaunch-specific updater commands.
for platform_source in (
    ROOT.parents[1] / "ext_components/cp0_lvgl/src/cp0/cp0_lvgl_osinfo.cpp",
    ROOT.parents[1] / "ext_components/cp0_lvgl/src/sdl/sdl_lvgl_osinfo.cpp",
):
    text = platform_source.read_text(encoding="utf-8")
    assert "SoftwareCheckStart" not in text
    assert "AppUpdateStart" not in text
    assert "AppUpdateStatus" not in text
    assert "AppUpdateCancel" not in text
assert 'state->status = "All software is up to date"' in PAGE
assert 'state->status = "Update check cancelled"' in PAGE

# The privileged updater keeps the same package allowlist as the page.  A
# package added to one list but not the other would silently stop updating.
PACKAGER = (ROOT.parents[1] / "scripts/debian_packager.py").read_text(
    encoding="utf-8"
)
for package in PACKAGES:
    assert package in PACKAGER, f"missing updater package: {package}"
assert "APP_UPDATE_SPECS" in PACKAGER

import re

model_specs = {
    (package, repo, prefix)
    for _short, package, repo, prefix in re.findall(
        r'\{"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"\}',
        MODEL,
    )
}
spec_block = PACKAGER.split("APP_UPDATE_SPECS = (", 1)[1].split("\n)", 1)[0]
packager_specs = {
    (package, repo, prefix)
    for package, repo, prefix in re.findall(
        r'\("([^"]+)",\s*"([^"]+)",\s*"([^"]+)"\)',
        spec_block,
    )
}
assert model_specs == packager_specs, (model_specs, packager_specs)
