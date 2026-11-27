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
        assert(model.page_count() == 0);
        assert(!model.move_row_right());
        assert(!model.move_row_left());
        assert(!model.move_up());
        assert(!model.move_down());
        assert(!model.flip_page(1));
        assert(!model.crossed_page());
    }

    // Exactly one page: right/left wrap inside the same row, up/down wrap
    // through the rows, flips are no-ops, nothing crosses a page.
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
        assert(model.move_row_left() && model.index() == 1);
        assert(model.move_row_left() && model.index() == 0);
        // Down from app 0 wraps within the page: (0 + 3) % 4 = 3.
        assert(model.move_down() && model.index() == 3 && !model.crossed_page());
        // Up from app 3 wraps back: (3 - 3 + 4) % 4 = 0.
        assert(model.move_up() && model.index() == 0);
        // Direct page flips need at least two pages.
        assert(!model.flip_page(1));
        assert(!model.flip_page(-1));
    }

    // Two full pages: row moves stay in the row, edge moves flip pages at
    // the same row, up/down swap rows and cross the page boundary by stride.
    {
        Model model;
        model.set_app_count(12);
        assert(model.page_count() == 2);

        assert(model.move_row_right() && model.index() == 1 && !model.crossed_page());
        assert(model.move_row_right() && model.index() == 2 && !model.crossed_page());
        assert(model.move_row_right() && model.index() == 6);
        assert(model.crossed_page() && model.moved_forward());
        assert(model.move_row_left() && model.index() == 2);
        assert(model.crossed_page() && !model.moved_forward());

        model.set_index(9);  // page 1, row 1, col 0
        assert(model.move_row_left() && model.index() == 5);
        assert(model.crossed_page() && !model.moved_forward());
        model.set_index(11);  // page 1, row 1, col 2: right wraps to page 0
        assert(model.move_row_right() && model.index() == 3);
        assert(model.crossed_page() && model.moved_forward());

        model.set_index(0);
        assert(model.move_down() && model.index() == 3 && !model.crossed_page());
        assert(model.move_down() && model.index() == 6 && model.crossed_page());
        assert(model.moved_forward());
        assert(model.move_up() && model.index() == 3 && model.crossed_page());
        assert(!model.moved_forward());

        // I/O flips keep the cell position.
        model.set_index(7);  // page 1, cell 1
        assert(model.flip_page(-1) && model.index() == 1);
        assert(model.crossed_page() && !model.moved_forward());
        assert(model.flip_page(1) && model.index() == 7 && model.moved_forward());
    }

    // Ragged list (13 apps = two pages plus a single app on page 3): the
    // last page shows one app and empty cells; every edge move lands on a
    // real app, and LEFT from the very first app reaches the last page.
    {
        Model model;
        model.set_app_count(13);
        assert(model.page_count() == 3);

        model.set_index(0);  // Settings, page 0 row 0 col 0
        assert(model.move_row_left() && model.index() == 12);
        assert(model.crossed_page() && !model.moved_forward());
        // And back: right past the very last app wraps to the front.
        assert(model.move_row_right() && model.index() == 0);
        assert(model.crossed_page() && model.moved_forward());

        model.set_index(11);  // page 1, row 1, col 2
        assert(model.move_row_right() && model.index() == 12);
        assert(model.crossed_page() && model.moved_forward());  // clamped real app
        model.set_index(12);
        assert(model.move_row_left() && model.index() == 8);
        assert(model.crossed_page() && !model.moved_forward());  // page 1 row 0 col 2

        model.set_index(2);  // page 0, row 0, col 2
        assert(model.move_row_right() && model.index() == 6 && model.crossed_page());
        model.set_index(6);
        assert(model.move_row_left() && model.index() == 2 && model.crossed_page());

        // Up/down stride still wraps modulo the app count.
        model.set_index(12);
        assert(model.move_up() && model.index() == 9 && model.crossed_page());
        assert(model.move_down() && model.index() == 12 && model.crossed_page());
        model.set_index(12);
        assert(model.move_down() && model.index() == 2 && model.crossed_page());

        // I/O flip onto the ragged page keeps the cell when it exists.
        model.set_index(6);
        assert(model.flip_page(1) && model.index() == 12 && model.moved_forward());
        assert(model.flip_page(1) && model.index() == 0 && model.moved_forward());
        // Clamp when the kept cell would be past the end: page 1 cell 5 is
        // real (app 11), page 2 cell 5 is not -> clamp to app 12.
        model.set_index(11);
        assert(model.flip_page(1) && model.index() == 12);
        assert(model.crossed_page() && model.moved_forward());
    }

    return 0;
}
