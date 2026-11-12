# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
# SPDX-License-Identifier: MIT

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "main/ui/settings/settings_volume_page.cpp").read_text(
    encoding="utf-8"
)


def function_body(signature: str) -> str:
    start = SOURCE.index(signature)
    opening_brace = SOURCE.index("{", start)
    depth = 0
    for index in range(opening_brace, len(SOURCE)):
        if SOURCE[index] == "{":
            depth += 1
        elif SOURCE[index] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[opening_brace + 1 : index]
    raise AssertionError(f"unterminated function: {signature}")


def normalized(source: str) -> str:
    return re.sub(r"\s+", " ", source).strip()


def test_page_seeds_the_persisted_volume_before_the_list_is_built():
    body = normalized(function_body("void LvSettingVolumePage3::initialize_page"))
    seed = body.index("read_saved_volume")
    built = body.index("initialize(parent)")

    assert seed < built, "the saved volume must be read before the value list is built"
    for member in (
        "original_volume_",
        "backend_volume_",
        "last_preview_requested_",
        "last_preview_actual_",
        "locked_selection_",
    ):
        assert member in body[seed:built], f"{member} is not seeded before create_ui"


def test_seed_uses_the_persisted_config_not_the_mixer():
    body = normalized(function_body("static int read_saved_volume(int fallback)"))

    assert '{"GetInt", "volume"' in body
    assert "VolumeRead" not in body


def test_initial_selection_uses_the_seeded_volume():
    body = normalized(function_body("int LvSettingVolumePage3::initial_selection() const"))

    assert "return selection_for_volume(backend_volume_);" in body
    assert "return 0;" not in body


def test_failed_read_does_not_restore_the_100_percent_default():
    body = function_body("void LvSettingVolumePage3::handle_volume_result(")
    read_branch = body[
        body.index("if (request.kind == RequestKind::Read)") : body.index(
            "if (request.kind == RequestKind::RestoreWrite)"
        )
    ]
    read_branch = normalized(read_branch)

    assert "metric(LayoutMetric::MaxVolume)" not in read_branch
    assert "select_volume(0);" not in read_branch
    assert "select_volume(selection_for_volume(backend_volume_));" in read_branch
