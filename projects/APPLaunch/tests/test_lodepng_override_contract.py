# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
# SPDX-License-Identifier: MIT

"""Issue #268: APPLaunch must build its patched lodepng.c instead of the SDK copy."""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / "main/lvgl/lodepng.c"
SCONS = (ROOT / "main/SConstruct").read_text(encoding="utf-8")

assert LOCAL.is_file(), "missing local lodepng override"
SRC = LOCAL.read_text(encoding="utf-8")

# License / provenance header (zlib clause 2 + project SPDX convention).
head = SRC[:2000]
assert "SPDX-License-Identifier: Zlib" in head
assert "Altered source version" in head
assert "Copyright (c) 2005-2023 Lode Vandevenne" in head

# decodeGeneric(): the intermediate buffer is sized for the native bit depth.
start = SRC.index("static void decodeGeneric(")
end = SRC.index("unsigned lodepng_decode(", start)
body = SRC[start:end]
assert "unsigned bpp = lodepng_get_bpp(&state->info_png.color);" in body
assert "size_t stride = LODEPNG_MAX((size_t)4u * *w, raw_row);" in body
assert "if(stride > 0xFFFFFFFFu / *h) state->error = 92;" in body
assert "LV_COLOR_FORMAT_ARGB8888, (uint32_t)stride);" in body
assert "LV_COLOR_FORMAT_ARGB8888, 4 * *w);" not in body
assert body.index("(uint32_t)stride);") < body.index("postProcessScanlines(decoded->data")

# SConstruct override block: glob, basename filter, private include, log line.
block_start = SCONS.index("# custom lvgl component")
block = SCONS[block_start:SCONS.index("if 'CONFIG_APPLAUNCH_CROSS'", block_start)]
assert "Glob('lvgl/lodepng.c')" in block
assert "if Path(str(src)).name not in local_lvgl_override_names" in block
assert block.index("not in local_lvgl_override_names") < block.index(
    "lvgl_component['SRCS'] += local_lvgl_override_srcs")
assert "ADir('../../../SDK/github_source/lvgl/lvgl_9_5/lvgl/src/libs/lodepng')" in block
assert "lvgl override active: main/lvgl/lodepng.c" in block

# The SDK original must never be re-added to the component sources.
assert not re.search(r"SRCS'\]\s*\+?=.*libs/lodepng/lodepng\.c", SCONS)
assert "src/libs/lodepng/lodepng.c'" not in SCONS
assert "lodepng.h" not in {p.name for p in (ROOT / "main/lvgl").iterdir()}


if __name__ == "__main__":
    pass
