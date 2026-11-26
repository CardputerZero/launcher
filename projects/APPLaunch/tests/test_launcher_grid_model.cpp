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
        assert(!model.move_next());
        assert(!model.move_previous());
        assert(!model.move_up());
        assert(!model.move_down());
        assert(!model.crossed_page());
    }

    // Exactly one page: every stride wraps inside the page and never reports
    // a page crossing.
    {
        Model model;
        model.set_app_count(4);
        assert(model.page_count() == 1);
        assert(model.move_next() && model.index() == 1 && !model.crossed_page());
        // Down from app 1 wraps within the page: (1 + 3) % 4 = 0.
        assert(model.move_down() && model.index() == 0 && !model.crossed_page());
        // Up from app 0 wraps the other way: (0 - 3 + 4) % 4 = 1.
        assert(model.move_up() && model.index() == 1);
        assert(model.move_previous() && model.index() == 0);
        assert(model.move_previous() && model.index() == 3);
        assert(model.move_previous() && model.index() == 2);
        assert(!model.crossed_page());
    }

    // Two pages: row strides and horizontal strides cross pages, and the
    // direction flag drives the slide animation.
    {
        Model model;
        model.set_app_count(12);
        assert(model.page_count() == 2);

        assert(model.move_next() && model.index() == 1 && !model.crossed_page());
        assert(model.move_previous() && model.index() == 0 && !model.crossed_page());
        // Previous from the first app wraps to the last app of the last page.
        assert(model.move_previous() && model.index() == 11);
        assert(model.crossed_page() && !model.moved_forward());
        assert(model.move_next() && model.index() == 0 && model.crossed_page());
        assert(model.moved_forward());

        // Down from cell 0 (row 0) lands on cell 3 of the same page.
        model.set_index(0);
        assert(model.move_down() && model.index() == 3 && !model.crossed_page());
        // Down from row 1 crosses to the next page.
        assert(model.move_down() && model.index() == 6 && model.crossed_page());
        assert(model.moved_forward());
        // Up from the first app of a page wraps back a page.
        assert(model.move_up() && model.index() == 3 && model.crossed_page());
        assert(!model.moved_forward());
    }

    // A partly filled last page never traps the cursor on an empty cell.
    {
        Model model;
        model.set_app_count(11);
        assert(model.page_count() == 2);
        model.set_index(8);
        assert(model.page() == 1);
        // Walking forward runs 8 -> 9 -> 10 -> wrap to 0; the wrapped slot on
        // the last page is never selected.
        assert(model.move_next() && model.index() == 9);
        assert(model.move_next() && model.index() == 10);
        assert(model.move_next() && model.index() == 0 && model.crossed_page());
        // set_app_count shrinking below the current index clamps safely.
        model.set_index(10);
        model.set_app_count(7);
        assert(model.index() == 6);
    }

    return 0;
}
