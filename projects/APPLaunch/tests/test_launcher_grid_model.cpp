/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "model/launcher_grid_model.hpp"

#include <cassert>

int main()
{
    using Model = LauncherGridModel;
    constexpr size_t kCells = Model::kCellsPerPage;
    static_assert(kCells == 6);
    static_assert(Model::kCols == 3);
    static_assert(Model::kRows == 2);

    // Empty app list: nothing moves, indices stay put.
    {
        Model model;
        assert(model.index() == 0);
        assert(model.page_count() == 0);
        assert(!model.move_row_right());
        assert(!model.move_row_left());
        assert(!model.move_up());
        assert(!model.move_down());
        assert(!model.crossed_page());
    }

    // Exactly one page: right/left wrap inside the same row, up/down wrap
    // through the rows, and nothing ever reports a page crossing.
    {
        Model model;
        model.set_app_count(4);
        assert(model.page_count() == 1);
        assert(model.move_row_right() && model.index() == 1 && !model.crossed_page());
        assert(model.move_row_right() && model.index() == 2 && !model.crossed_page());
        // Right past the row end wraps to the row start of the same page.
        assert(model.move_row_right() && model.index() == 0 && !model.crossed_page());
        assert(model.moved_forward());
        // Left from the row start wraps to the row end of the same page.
        assert(model.move_row_left() && model.index() == 2 && !model.crossed_page());
        assert(!model.moved_forward());
        // Down from app 1 wraps within the page: (1 + 3) % 4 = 0.
        model.set_index(1);
        assert(model.move_down() && model.index() == 0 && !model.crossed_page());
        // Up from app 0 wraps the other way: (0 - 3 + 4) % 4 = 1.
        assert(model.move_up() && model.index() == 1);
    }

    // Two full pages: row moves stay in the row, edge moves flip pages at the
    // same row, up/down swap rows and cross the page boundary by stride.
    {
        Model model;
        model.set_app_count(12);
        assert(model.page_count() == 2);

        // Walk row 0 of page 0: 0 -> 1 -> 2, then the edge flips forward.
        assert(model.move_row_right() && model.index() == 1 && !model.crossed_page());
        assert(model.move_row_right() && model.index() == 2 && !model.crossed_page());
        assert(model.move_row_right() && model.index() == 6);
        assert(model.crossed_page() && model.moved_forward());
        // Left from page 1 row 0 col 0 flips back to page 0 row 0 col 2.
        assert(model.move_row_left() && model.index() == 2);
        assert(model.crossed_page() && !model.moved_forward());

        // Inner-row moves never flip.
        model.set_index(6);
        assert(model.move_row_right() && model.index() == 7 && !model.crossed_page());
        model.set_index(9);  // page 1, row 1, col 0
        assert(model.move_row_left() && model.index() == 5);
        assert(model.crossed_page() && !model.moved_forward());
        model.set_index(11);  // page 1, row 1, col 2
        assert(model.move_row_right() && model.index() == 3);
        assert(model.crossed_page() && model.moved_forward());

        // Up/down keep the column and swap rows, wrapping by stride.
        model.set_index(0);
        assert(model.move_down() && model.index() == 3 && !model.crossed_page());
        assert(model.move_down() && model.index() == 6 && model.crossed_page());
        assert(model.moved_forward());
        assert(model.move_up() && model.index() == 3 && model.crossed_page());
        assert(!model.moved_forward());
    }

    // A ragged last page: edge flips land on the wrapped cells actually shown
    // on screen, and the cursor never traps on an empty slot.
    {
        Model model;
        model.set_app_count(11);
        assert(model.page_count() == 2);

        model.set_index(8);  // last page, row 0, col 2
        assert(model.move_row_right() && model.index() == 0);
        assert(model.crossed_page() && model.moved_forward());
        model.set_index(6);  // last page, row 0, col 0
        assert(model.move_row_left() && model.index() == 2);
        assert(model.crossed_page() && !model.moved_forward());

        // Forward walk wraps through the end: 9 -> 10 stays on the page,
        // 10 -> 0 crosses it.
        model.set_index(9);
        assert(model.move_row_right() && model.index() == 10 && !model.crossed_page());
        assert(model.move_row_right() && model.index() == 0 && model.crossed_page());
        assert(model.moved_forward());

        // set_app_count shrinking below the current index clamps safely.
        model.set_index(10);
        model.set_app_count(7);
        assert(model.index() == 6);
    }

    return 0;
}
