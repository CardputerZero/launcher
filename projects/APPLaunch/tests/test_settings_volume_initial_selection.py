# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
# SPDX-License-Identifier: MIT

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "main/ui/settings/settings_volume_page.cpp").read_text(
    encoding="utf-8"
)
UI_SOURCE = (ROOT / "main/ui/ui.cpp").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening_brace = source.index("{", start)
    depth = 0
    for index in range(opening_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening_brace + 1 : index]
    raise AssertionError(f"unterminated function: {signature}")


def normalized(source: str) -> str:
    return re.sub(r"\s+", " ", source).strip()


def test_page_seeds_the_cached_volume_before_the_list_is_built():
    body = normalized(
        function_body(SOURCE, "void LvSettingVolumePage3::initialize_page")
    )
    seed = body.index("launcher_media_controls::cached_volume")
    built = body.index("initialize(parent)")

    assert seed < built, "the cached volume must be read before the value list is built"
    for member in (
        "original_volume_",
        "backend_volume_",
        "last_preview_requested_",
        "last_preview_actual_",
        "locked_selection_",
    ):
        assert member in body[seed:built], f"{member} is not seeded before create_ui"


def test_seed_falls_back_to_the_persisted_config_only_without_a_cache():
    body = normalized(
        function_body(SOURCE, "void LvSettingVolumePage3::initialize_page")
    )

    assert "launcher_media_controls::has_cached_volume" in body
    assert "read_saved_volume" in body
    assert body.index("cached_volume") < body.index("read_saved_volume"), (
        "the launcher cache is the primary seed; the persisted key is a fallback"
    )


def test_fallback_seed_uses_the_persisted_config_not_the_mixer():
    body = normalized(
        function_body(SOURCE, "static int read_saved_volume(int fallback)")
    )

    assert '{"GetInt", "volume"' in body
    assert "VolumeRead" not in body


def test_initial_selection_uses_the_seeded_volume():
    body = normalized(
        function_body(SOURCE, "int LvSettingVolumePage3::initial_selection() const")
    )

    assert "return selection_for_volume(backend_volume_);" in body
    assert "return 0;" not in body


def test_failed_read_does_not_restore_the_100_percent_default():
    body = function_body(SOURCE, "void LvSettingVolumePage3::handle_volume_result(")
    read_branch = body[
        body.index("if (request.kind == RequestKind::Read)") : body.index(
            "if (request.kind == RequestKind::RestoreWrite)"
        )
    ]
    read_branch = normalized(read_branch)

    assert "metric(LayoutMetric::MaxVolume)" not in read_branch
    assert "select_volume(0);" not in read_branch
    assert "select_volume(selection_for_volume(backend_volume_));" in read_branch


def test_mixer_results_refresh_the_shared_cache():
    body = normalized(
        function_body(SOURCE, "void LvSettingVolumePage3::handle_volume_result(")
    )
    cache = "launcher_media_controls::update_cached_volume"

    assert cache in body, "the page must publish mixer results to the shared cache"
    read_branch = body[
        body.index("RequestKind::Read") : body.index("RequestKind::RestoreWrite")
    ]
    assert cache in read_branch, "the mixer read must refresh the shared cache"

    write_branch = body[body.index("if (!valid)") :]
    assert cache in write_branch, "preview/commit writes must refresh the shared cache"


def test_launcher_startup_starts_and_stops_the_cache():
    init_body = normalized(function_body(UI_SOURCE, "void init()"))
    deinit_body = normalized(function_body(UI_SOURCE, "void deinit()"))

    assert "launcher_media_controls::start_volume_cache()" in init_body
    assert "launcher_media_controls::stop_volume_cache()" in deinit_body
