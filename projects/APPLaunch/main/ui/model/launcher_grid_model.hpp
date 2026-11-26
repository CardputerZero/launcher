/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

// Selection model for the 3x2 home grid.  The selected app is one absolute
// index into the (wrapped) app list; every move is a fixed stride on that
// index, so the cursor can never land on an empty cell of a partly filled
// last page - the list simply wraps.
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
    bool move_next() { return step(1); }
    bool move_previous() { return step(-1); }
    bool move_up() { return step(-static_cast<int>(kCols)); }
    bool move_down() { return step(static_cast<int>(kCols)); }

    // Single entry point for the UI: delta is the stride in apps
    // (+-1 walks a row, +-kCols swaps rows).
    bool move_by(int delta)
    {
        switch (delta) {
        case 1: return move_next();
        case -1: return move_previous();
        default: return delta < 0 ? move_up() : move_down();
        }
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

    size_t app_count_ = 0;
    size_t index_ = 0;
    bool crossed_page_ = false;
    bool moved_forward_ = false;
};
