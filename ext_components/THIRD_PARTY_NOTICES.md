# Third-Party Notices

The component wrappers in this directory fetch or expose upstream projects.
Their licenses remain those of the upstream projects and are not changed by
the MIT license used for M5Stack-authored wrapper code.

| Wrapper | Upstream source | License record |
| --- | --- | --- |
| `Miniaudio` | `https://github.com/mackron/miniaudio` | The complete public-domain/MIT-0 notice is embedded at the end of `Miniaudio/include/miniaudio.h`. |
| `RadioLib` | `https://github.com/jgromes/RadioLib` | MIT text in `RadioLib/LICENSE`; SConstruct fetches a URL without pinning a commit. |
| `Sigslot` | `https://github.com/palacaze/sigslot` | Fetched at the commit pinned by `Sigslot/SConstruct`; retain the upstream MIT notice. |
| `SmoothUI` | `https://github.com/Forairaaaaa/smooth_ui_toolkit` | MIT text in `SmoothUI/LICENSE`; commit pinned by SConstruct. |
| `Spdlog` | `https://github.com/gabime/spdlog` | Fetched at the commit pinned by `Spdlog/SConstruct`; retain the upstream MIT notice. |
| `tree.hh` | Kasper Peeters tree container | GPL-3.0-only attribution in `cp0_lvgl/include/tree.hh`; full text in `../LICENSES/tree.hh-GPL-3.0-only.txt`. |

Full notices are also retained in `Miniaudio/LICENSE`, `Sigslot/LICENSE`
and `Spdlog/LICENSE`. SDK dependencies used by these components include
LVGL (MIT), eventpp (Apache-2.0), and platform libraries selected by
SConstruct. `../LICENSES/` records libhv (BSD-3-Clause), fmt (MIT), tinyalsa
(BSD-3-Clause), cJSON, C-Thread-Pool and cpp-httplib (MIT). The repository
inventory distinguishes optional cached sources from linked dependencies.
In particular, the cached SimpleBLE checkout uses BUSL-1.1; cp0 Bluetooth
uses BlueZ/DBus, and the SimpleBLE cache must not be described as MIT.

See [`../docs/OPEN_SOURCE_COMPONENTS.md`](../docs/OPEN_SOURCE_COMPONENTS.md)
for the complete repository inventory, including SDK, LVGL, and system
dependencies.
