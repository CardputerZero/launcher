/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#define APP_PAGE_IMPLEMENTATION_UNIT
#include "ui_app_st.hpp"

void UISTPage::update_scrollbar()
{
    if (!scrollbar_track_ || !scrollbar_thumb_) return;
    if (big_mode_) {
        int max_y = max_viewport_y();
        lv_obj_clear_flag(scrollbar_track_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scrollbar_thumb_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(scrollbar_track_, SCROLLBAR_W, visible_h());
        lv_obj_set_pos(scrollbar_track_, TERM_W - SCROLLBAR_W - 1, 0);

        int thumb_height = std::max(8, visible_h() * visible_rows() / std::max(term_rows_, 1));
        thumb_height = std::min(visible_h(), thumb_height);
        int range = visible_h() - thumb_height;
        int thumb_y = max_y > 0 ? viewport_y_ * range / max_y : 0;
        lv_obj_set_size(scrollbar_thumb_, SCROLLBAR_W, thumb_height);
        lv_obj_set_pos(scrollbar_thumb_, TERM_W - SCROLLBAR_W - 1, thumb_y);
        lv_obj_move_foreground(scrollbar_track_);
        lv_obj_move_foreground(scrollbar_thumb_);
        return;
    }

    int history_rows = static_cast<int>(scrollback_.size());
    if (history_rows <= 0) {
        lv_obj_add_flag(scrollbar_track_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scrollbar_thumb_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(scrollbar_track_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(scrollbar_thumb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(scrollbar_track_, SCROLLBAR_W, TERM_H);
    lv_obj_set_pos(scrollbar_track_, TERM_W - SCROLLBAR_W - 1, 0);
    int total_rows = history_rows + term_rows_;
    int thumb_height = std::max(8, TERM_H * term_rows_ / std::max(total_rows, 1));
    thumb_height = std::min(TERM_H, thumb_height);
    int range = TERM_H - thumb_height;
    int thumb_y = range - scrollback_offset_ * range / std::max(1, history_rows);
    lv_obj_set_size(scrollbar_thumb_, SCROLLBAR_W, thumb_height);
    lv_obj_set_pos(scrollbar_thumb_, TERM_W - SCROLLBAR_W - 1, thumb_y);
    lv_obj_move_foreground(scrollbar_track_);
    lv_obj_move_foreground(scrollbar_thumb_);
}

void UISTPage::update_hscrollbar()
{
    if (!hscrollbar_track_ || !hscrollbar_thumb_) return;
    if (!big_mode_) {
        lv_obj_add_flag(hscrollbar_track_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(hscrollbar_thumb_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(hscrollbar_track_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(hscrollbar_thumb_, LV_OBJ_FLAG_HIDDEN);
    int y = visible_h();
    int width = TERM_W - SCROLLBAR_W - 2;
    lv_obj_set_size(hscrollbar_track_, width, 3);
    lv_obj_set_pos(hscrollbar_track_, 0, y);
    int max_x = max_viewport_x();
    int thumb_width = std::max(18, width * visible_cols() / std::max(term_cols_, 1));
    thumb_width = std::min(width, thumb_width);
    int range = width - thumb_width;
    int thumb_x = max_x > 0 ? viewport_x_ * range / max_x : 0;
    lv_obj_set_size(hscrollbar_thumb_, thumb_width, 3);
    lv_obj_set_pos(hscrollbar_thumb_, thumb_x, y);
    lv_obj_move_foreground(hscrollbar_track_);
    lv_obj_move_foreground(hscrollbar_thumb_);
}

void UISTPage::set_bottom_label(int index, const char *text)
{
    if (index < 0 || index >= BOTTOM_BAR_SLOTS || !bottom_labels_[static_cast<size_t>(index)])
        return;
    lv_label_set_text(bottom_labels_[static_cast<size_t>(index)], text);
}

void UISTPage::update_big_mode_ui()
{
    for (auto *label : bottom_labels_) {
        if (!label) continue;
        if (big_mode_) lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
    for (auto *indicator : bottom_indicators_) {
        if (!indicator) continue;
        if (big_mode_) lv_obj_clear_flag(indicator, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
    }
    update_hscrollbar();
    update_scrollbar();
}

void UISTPage::switch_big_mode(bool enable)
{
    if (big_mode_ == enable) return;
    terminal_active_ = false;
    stop_timers();
    stop_pty();
    big_mode_ = enable;
    term_cols_ = big_mode_ ? BIG_COLS : NORMAL_COLS;
    term_rows_ = big_mode_ ? BIG_ROWS : NORMAL_ROWS;
    viewport_x_ = 0;
    viewport_y_ = 0;
    big_view_locked_ = false;
    reset_terminal();
    update_big_mode_ui();
    start_shell();
}

void UISTPage::leave_scrollback()
{
    if (scrollback_offset_ == 0) return;
    scrollback_offset_ = 0;
    dirty_all();
}

void UISTPage::pan_big_view(int dx, int dy)
{
    if (!big_mode_) return;
    big_view_locked_ = true;
    int old_x = viewport_x_;
    int old_y = viewport_y_;
    viewport_x_ = clamp(viewport_x_ + dx, 0, max_viewport_x());
    viewport_y_ = clamp(viewport_y_ + dy, 0, max_viewport_y());
    if (viewport_x_ != old_x || viewport_y_ != old_y) dirty_all();
}

void UISTPage::follow_cursor_in_big_mode()
{
    if (!big_mode_ || big_view_locked_) return;
    int old_x = viewport_x_;
    int old_y = viewport_y_;
    if (cursor_.x < viewport_x_)
        viewport_x_ = cursor_.x;
    else if (cursor_.x >= viewport_x_ + visible_cols())
        viewport_x_ = cursor_.x - visible_cols() + 1;
    if (cursor_.y < viewport_y_)
        viewport_y_ = cursor_.y;
    else if (cursor_.y >= viewport_y_ + visible_rows())
        viewport_y_ = cursor_.y - visible_rows() + 1;

    viewport_x_ = clamp(viewport_x_, 0, max_viewport_x());
    viewport_y_ = clamp(viewport_y_, 0, max_viewport_y());
    if (viewport_x_ != old_x || viewport_y_ != old_y) dirty_all();
}


