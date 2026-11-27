/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

// Selection model for the 3x2 home grid.  The selected app is one absolute
// index into the app list; pages never show wrapped filler, so a ragged
// last page simply leaves its trailing cells empty and every move targets a
// real app.
class LauncherGridModel
{
public:
    static constexpr size_t kCols = 3;
    static constexpr size_t kRows = 2;
    static constexpr size_t kCellsPerPage = kCols * kRows;

    void set_app_count(size_t count)
    {
        if (app_count_ == count) return;
        app_count_ = count;
        if (app_count_ == 0) {
            index_ = 0;
            return;
        }
        if (index_ >= app_count_) index_ = app_count_ - 1;
    }

    size_t index() const { return index_; }

    void set_index(size_t index)
    {
        if (app_count_ == 0) return;
        index_ = index % app_count_;
    }

    size_t page() const { return app_count_ == 0 ? 0 : index_ / kCellsPerPage; }

    size_t page_count() const
    {
        return app_count_ == 0 ? 0 : (app_count_ + kCellsPerPage - 1) / kCellsPerPage;
    }

    // Each move returns true when the selection changed.  After a move,
    // crossed_page() reports whether the new selection sits on another grid
    // page (the caller plays the page-flip animation) and moved_forward()
    // reports the travel direction for the slide.
    //
    // Left/right stay inside the current row: stepping past the row edge
    // flips to the neighbouring page at the same row, clamped to a real app
    // when that page is ragged; walking right past the very last app wraps
    // to the first page's first cell.  Up/down swap rows and wrap through
    // the page boundary.
    bool move_row_right() { return move_row(1); }
    bool move_row_left() { return move_row(-1); }
    bool move_up() { return step(-static_cast<int>(kCols)); }
    bool move_down() { return step(static_cast<int>(kCols)); }

    // Direct page flip (I/O keys): keep the cursor's cell position on the
    // neighbouring page; a ragged neighbour clamps to its last real app.
    bool flip_page(int dir)
    {
        crossed_page_ = false;
        moved_forward_ = false;
        const size_t pages = page_count();
        if (app_count_ == 0 || pages == 1) return false;
        const size_t page = index_ / kCellsPerPage;
        const size_t cell = index_ % kCellsPerPage;
        const size_t neighbour = dir > 0 ? (page + 1) % pages
                                         : (page + pages - 1) % pages;
        size_t target = neighbour * kCellsPerPage + cell;
        if (target >= app_count_) target = app_count_ - 1;
        if (target == index_) return false;
        index_ = target;
        crossed_page_ = index_ / kCellsPerPage != page;
        moved_forward_ = dir > 0;
        return true;
    }

    bool crossed_page() const { return crossed_page_; }
    bool moved_forward() const { return moved_forward_; }

private:
    bool step(int delta)
    {
        crossed_page_ = false;
        moved_forward_ = false;
        if (app_count_ == 0) return false;
        const size_t previous = index_;
        const int raw = static_cast<int>(index_) + delta;
        int wrapped = raw % static_cast<int>(app_count_);
        if (wrapped < 0) wrapped += static_cast<int>(app_count_);
        index_ = static_cast<size_t>(wrapped);
        if (index_ == previous) return false;
        crossed_page_ = index_ / kCellsPerPage != previous / kCellsPerPage;
        moved_forward_ = delta > 0;
        return true;
    }

    // dir 1 = right, -1 = left.  Within the row this is a plain +-1 (right
    // past the very last app wraps to the front); at the row edge the target
    // jumps to the neighbouring page's same-row far cell, clamped to a real
    // app when that page is ragged.
    bool move_row(int dir)
    {
        crossed_page_ = false;
        moved_forward_ = false;
        if (app_count_ == 0) return false;
        const size_t page = index_ / kCellsPerPage;
        const size_t cell = index_ % kCellsPerPage;
        const size_t row = cell / kCols;
        const size_t col = cell % kCols;
        const size_t pages = page_count();
        size_t target;
        if (dir > 0 && col + 1 < kCols) {
            target = index_ + 1;
            if (target >= app_count_) target = 0;  // wrap past the last app
        } else if (dir < 0 && col > 0) {
            target = index_ - 1;  // always real: empty cells sit at the end
        } else {
            const size_t neighbour = dir > 0 ? (page + 1) % pages
                                             : (page + pages - 1) % pages;
            target = neighbour * kCellsPerPage + row * kCols +
                     (dir > 0 ? 0 : kCols - 1);
            if (target >= app_count_)
                target = app_count_ - 1;  // ragged neighbour: nearest real app
        }
        if (target == index_) return false;
        index_ = target;
        // Crossing is a page-number change, nothing else: on a single-page
        // list the row-edge wrap stays on the same page and animates as a
        // plain highlight move.
        crossed_page_ = index_ / kCellsPerPage != page;
        moved_forward_ = dir > 0;
        return true;
    }

    size_t app_count_ = 0;
    size_t index_ = 0;
    bool crossed_page_ = false;
    bool moved_forward_ = false;
};
