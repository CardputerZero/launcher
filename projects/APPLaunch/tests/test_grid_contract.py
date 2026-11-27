# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT

"""Wiring contract for the 3x2 home app grid.

The grid model logic is covered by test_launcher_grid_model.cpp.  This pins
the textual wiring that is easy to lose in a refactor: the card theme
invariants inherited from the carousel, the page-flip plumbing and the
arrow-key wiring.
"""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "main/ui"

PAGE_HEADER = (UI / "ui_launch_page.h").read_text(encoding="utf-8")
PAGE = (UI / "ui_launch_page.cpp").read_text(encoding="utf-8")
VIEW = (UI / "ui_launch_page_carousel_view.cpp").read_text(encoding="utf-8")
INPUT = (UI / "ui_launch_page_input.cpp").read_text(encoding="utf-8")
ANIMATION_H = (UI / "animation/ui_launcher_animation.h").read_text(encoding="utf-8")
ANIMATION = (UI / "animation/ui_launcher_animation.cpp").read_text(encoding="utf-8")
LAYOUT = (UI / "animation/launcher_grid_layout.h").read_text(encoding="utf-8")
MODEL = (UI / "model/launcher_grid_model.hpp").read_text(encoding="utf-8")
LAUNCH_H = (UI / "launch.h").read_text(encoding="utf-8")
LAUNCH = (UI / "launch.cpp").read_text(encoding="utf-8")

# ---- layout -------------------------------------------------------------
# 3 columns x 2 rows, six apps per page, square cards so the square icon
# artwork is never stretched by the per-axis image compensation.
assert "kCols = 3" in LAYOUT and "kRows = 2" in LAYOUT
assert "kCells = kCols * kRows" in LAYOUT
assert "kCardSize = 58" in LAYOUT
assert "static_assert(kCellLayout.size() == kCells)" in LAYOUT
assert "launcher_carousel_layout" not in PAGE + VIEW + ANIMATION + INPUT
assert "kLauncherGridElementCount ==" in VIEW or "kLauncherGridElementCount ==" in PAGE

# The old carousel plumbing must stay gone.
for gone in (
    "switch_left",
    "switch_right",
    "rotate_carousel",
    "carousel_slot_app",
    "select_next_app",
    "select_previous_app",
    "kHomeCarouselSlotCount",
    "animate_left",
    "animate_right",
    "arrow_button_",
    "create_grid_arrow",
):
    assert gone not in PAGE_HEADER + PAGE + VIEW + INPUT + ANIMATION_H, gone
    assert gone not in LAUNCH_H + LAUNCH, gone

# ---- selection model ----------------------------------------------------
# The UI drives the model's named moves; left/right stay inside the row and
# the model decides when an edge move means a page flip.
assert "void select_app_relative(int delta);" in LAUNCH_H
assert "select_app_relative(static_cast<int>(grid_.index())" in PAGE
assert "class LauncherGridModel" in MODEL
assert "kCellsPerPage = kCols * kRows" in MODEL
assert "crossed_page_" in MODEL and "moved_forward_" in MODEL
assert "bool move_row_right() { return move_row(1); }" in MODEL
assert "bool move_row_left() { return move_row(-1); }" in MODEL
assert "grid_.move_row_right()" in PAGE and "grid_.move_row_left()" in PAGE
assert "grid_.move_down()" in PAGE and "grid_.move_up()" in PAGE
# Moves ignore while a page flip animates, so strides cannot interleave.
assert "if (page_flip_active_) return false;" in PAGE

# ---- page flip ----------------------------------------------------------
assert "void animate_page_slide(lv_obj_t *container, bool forward," in ANIMATION_H
assert "void animate_page_slide(lv_obj_t *container, bool forward," in ANIMATION
assert "void UILaunchPage::start_page_flip(bool forward)" in PAGE
assert "finish_page_flip_animation()" in PAGE
assert "cancel_page_flip_animation()" in PAGE

# ---- card theme (inherited from the carousel contract) ------------------
# Every card keeps the theme border: the factory never overrides the border
# width and draws it with border_post, never clip_corner.
start = VIEW.index("lv_obj_t *create_grid_card")
end = VIEW.index("lv_obj_t *create_grid_title", start)
factory = VIEW[start:end]
assert "lv_obj_set_style_border_width(card," not in factory
assert "lv_obj_set_style_border_post(card, true," in factory
assert "lv_obj_set_style_clip_corner(card," not in factory
# The selection highlight: theme-accent border + lit tile on the selection;
# every card keeps full icon brightness (no dimming of neighbours).
assert "kSelectedBorderColor" in LAYOUT and "kSelectedBorderColor" in PAGE
assert "BORDER_COLOR_SIDE" in PAGE
assert "kSelectedBgOpa" in LAYOUT and "set_style_bg_opa" in PAGE
assert "kUnselectedIconOpa" not in LAYOUT + PAGE
assert "kUnselectedTitleColor" not in LAYOUT + PAGE
assert "BORDER_COLOR_CENTER" not in PAGE

# ---- arrows and keys ----------------------------------------------------
# All four arrows move the grid; F/Z/X/C remap onto them as before, and the
# old no-op UP/DOWN branches are gone.
assert "move_grid_up()" in INPUT and "move_grid_down()" in INPUT
assert "move_grid_previous()" in INPUT and "move_grid_next()" in INPUT
assert "move_grid_page_previous()" in INPUT and "move_grid_page_next()" in INPUT
assert "case KEY_UP:" in INPUT and "case KEY_DOWN:" in INPUT
assert "case KEY_I:" in INPUT and "case KEY_O:" in INPUT
assert "fzxc_to_arrow" in INPUT
# Clicking a card selects its cell, then launches.
assert "select_grid_cell(cell);" in INPUT
assert "launch_selected_app();" in INPUT
assert "void UILaunchPage::select_grid_cell(size_t cell)" in PAGE

# Ragged pages -------------------------------------------------------------
# Pages never show wrapped filler: cells past the app count are hidden, and
# the model owns the clamped edge/wrap targets plus the I/O page flips.
assert "LV_OBJ_FLAG_HIDDEN" in PAGE and "real" in PAGE
assert "bool flip_page(int dir)" in MODEL
