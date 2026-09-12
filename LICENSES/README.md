# License text provenance

This directory stores third-party notices alongside the repository's
M5Stack MIT license. It does not relicense dependencies. The usage and
selection inventory is in [OPEN_SOURCE_COMPONENTS.md](../docs/OPEN_SOURCE_COMPONENTS.md).

| Archived text | Original in this checkout |
| --- | --- |
| M5Stack-SDK-MIT.txt | SDK/LICENSE |
| LVGL-MIT.txt | SDK/github_source/lvgl/lvgl_9_5/lvgl/LICENCE.txt |
| eventpp-Apache-2.0.txt | SDK/github_source/eventpp/license |
| libhv-BSD-3-Clause.txt | SDK/github_source/libhv/LICENSE |
| fmt-MIT.txt | SDK/components/utilities/party/fmt/LICENSE.rst |
| cJSON-MIT.txt | SDK/github_source/cJSON/LICENSE |
| C-Thread-Pool-MIT.txt | SDK/github_source/C-Thread-Pool/LICENSE |
| cpp-httplib-MIT.txt | SDK/github_source/cpp-httplib/LICENSE |
| tinyalsa-BSD-3-Clause.txt | SDK/github_source/tinyalsa/NOTICE; its LICENSE file refers to NOTICE |
| SimpleBLE-BUSL-1.1.md | SDK/github_source/SimpleBLE/LICENSE.md; cached source, not established as linked |
| tree.hh-GPL-3.0-only.txt | Standard GPL v3 text from /usr/share/common-licenses/GPL-3; version and Kasper Peeters attribution from ext_components/cp0_lvgl/include/tree.hh |
| nlohmann-json-MIT.txt | Standard MIT text with 2013–2023 Niels Lohmann attribution from the emulator's vendor/nlohmann/json.hpp |
| libv4l-host-copyright.txt | /usr/share/doc/libv4l-0t64/copyright on the audit host; reference only, not claimed to match CameraApp's target package version |
| libv4l-1.32.0-copyright.txt | usr/share/doc/libv4l-0t64/copyright from Ubuntu libv4l-0t64_1.32.0-2ubuntu1_arm64.deb, matching CameraApp's libv4l-dev package version |
| GPL-2.0.txt, LGPL-2.0.txt, LGPL-2.1.txt | Full standard texts from /usr/share/common-licenses/GPL-2, LGPL-2 and LGPL-2.1, referenced by the v4l-utils copyright record |

The target libv4l copyright was fetched from
`https://ports.ubuntu.com/ubuntu-ports/pool/main/v/v4l-utils/libv4l-0t64_1.32.0-2ubuntu1_arm64.deb`
through the configured proxy without installing or executing the package.
Package metadata confirms `libv4l-0t64`, version `1.32.0-2ubuntu1`, `arm64`;
SHA-256 is `9a3a5c89c2b367d04734bc07ce285ed559228373cda1eef038c24da1d9c2098a`.
The development package's documentation is a symlink to this runtime package.

Wrapper-local LICENSE files for RadioLib, Sigslot, SmoothUI and Spdlog were
copied from their corresponding SDK/github_source trees. Miniaudio/LICENSE
is the complete dual-license block extracted from its single header.
APPLaunch's local LVGL directory contains copies of the LVGL and FreeType
notices. Keyboard Guide's NotoSans/OFL.txt comes from LVGL's
scripts/built_in_font/font_license/NotoSansSC/OFL.txt; the input font's
embedded metadata confirms the same Adobe copyright and OFL-1.1 license.

Retain original per-file notices and any nested dependency licenses. Recheck
these copies whenever dependency revisions change, especially unpinned downloads.
