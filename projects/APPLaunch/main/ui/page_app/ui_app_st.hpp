/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "sample_log.h"
#include "../ui_app_page.hpp"
#include "cp0_lvgl_app.h"
#include "compat/input_keys.h"
#include <algorithm>
#include <array>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include <string>
#include <sstream>
#include <vector>
#include <chrono>

extern "C" {
LV_FONT_DECLARE(ui_font_liberation_mono_11)
}

// ============================================================
//  ST terminal  UISTPage
//  Screen resolution: 320 x 170  (top bar 20px, ui_APP_Container 320x150)
//
//  This is a standalone terminal page. It does not inherit from or include
//  the existing console page. The structure follows suckless st's split:
//    - terminal state: glyph grid, cursor, modes, scroll region
//    - byte stream parser: ESC/CSI/OSC handling
//    - frontend: LVGL row renderer and keyboard -> PTY writer
// ============================================================
class UISTPage : public AppPage
{
    static constexpr int TERM_W = 320;
    static constexpr int TERM_H = 150;
    static constexpr int CHAR_W = 7;
    static constexpr int CHAR_H = 15;
    static constexpr int TEXT_Y_PAD = 2;
    static constexpr int NORMAL_COLS = TERM_W / CHAR_W;
    static constexpr int NORMAL_ROWS = TERM_H / CHAR_H;
    static constexpr int BIG_COLS = 80;
    static constexpr int BIG_ROWS = 24;
    static constexpr int BIG_BOTTOM_H = 15;
    static constexpr int BIG_VIEW_ROWS = (TERM_H - BIG_BOTTOM_H) / CHAR_H;
    static constexpr int MAX_COLS = BIG_COLS;
    static constexpr int MAX_ROWS = BIG_ROWS;
    static constexpr int COLS = NORMAL_COLS;
    static constexpr int ROWS = NORMAL_ROWS;
    static constexpr int SCROLLBACK_MAX_ROWS = 200;
    static constexpr int SCROLLBAR_W = 3;
    static constexpr int BOTTOM_BAR_SLOTS = 5;

    static constexpr uint32_t DEFAULT_FG = 7;
    static constexpr uint32_t DEFAULT_BG = 0;

    enum GlyphAttr : uint16_t {
        ATTR_NULL      = 0,
        ATTR_BOLD      = 1 << 0,
        ATTR_FAINT     = 1 << 1,
        ATTR_UNDERLINE = 1 << 2,
        ATTR_BLINK     = 1 << 3,
        ATTR_REVERSE   = 1 << 4,
        ATTR_INVISIBLE = 1 << 5,
    };

    enum TermMode : uint16_t {
        MODE_WRAP      = 1 << 0,
        MODE_INSERT    = 1 << 1,
        MODE_APPCURSOR = 1 << 2,
    };

    enum class ParseState {
        Normal,
        Esc,
        Csi,
        Osc,
        Charset,
    };

    struct Glyph {
        uint32_t u = ' ';
        uint16_t attr = ATTR_NULL;
        uint32_t fg = DEFAULT_FG;
        uint32_t bg = DEFAULT_BG;
    };

    struct Cursor {
        int x = 0;
        int y = 0;
        Glyph attr;
    };

    struct RenderSegment {
        lv_obj_t *label = nullptr;
        std::string text;
        int x = 0;
        int width = 0;
        uint32_t fg = UINT32_MAX;
        uint32_t bg = UINT32_MAX;
        bool hidden = true;
    };

    struct SegmentData {
        std::string text;
        int x = 0;
        uint32_t fg = DEFAULT_FG;
        uint32_t bg = DEFAULT_BG;
    };

public:
    bool terminal_sysplause = true;

    UISTPage() : AppPage()
    {
        set_page_title("ST");
        reset_terminal();
        create_ui();
        bind_events();
        start_shell();
    }

    ~UISTPage()
    {
        terminal_active_ = false;
        stop_timers();
        stop_pty();
    }

    void exec(std::string cmd)
    {
        stop_timers();
        stop_pty();

        terminal_active_ = true;
        waiting_key_to_exit_ = false;
        big_mode_ = false;
        term_cols_ = NORMAL_COLS;
        term_rows_ = NORMAL_ROWS;
        viewport_x_ = 0;
        viewport_y_ = 0;
        big_view_locked_ = false;
        reset_terminal();
        update_big_mode_ui();
        render_all();

        std::vector<std::string> tokens;
        std::istringstream iss(cmd);
        std::string token;
        while (iss >> token)
            tokens.push_back(token);

        if (tokens.empty()) {
            const char *err = "Error: empty command\r\n";
            process_bytes(err, (int)strlen(err));
            render_all();
            terminal_active_ = false;
            waiting_key_to_exit_ = true;
            return;
        }

        std::list<std::string> args;
        for (size_t i = 1; i < tokens.size(); ++i)
            args.push_back(tokens[i]);

        start_command(tokens[0], args, tokens[0].c_str(), "Error: openpty/fork failed\r\n");
    }

private:
    std::array<std::array<Glyph, MAX_COLS>, MAX_ROWS> screen_{};
    std::array<std::vector<RenderSegment>, ROWS> row_segments_{};
    std::array<bool, ROWS> dirty_{};
    std::vector<std::array<Glyph, MAX_COLS>> scrollback_;
    int scrollback_offset_ = 0;
    int term_cols_ = NORMAL_COLS;
    int term_rows_ = NORMAL_ROWS;
    int viewport_x_ = 0;
    int viewport_y_ = 0;
    bool big_view_locked_ = false;
    Cursor cursor_{};
    Cursor saved_cursor_{};
    int scroll_top_ = 0;
    int scroll_bot_ = NORMAL_ROWS - 1;
    uint16_t mode_ = MODE_WRAP;

    ParseState parse_state_ = ParseState::Normal;
    bool csi_private_ = false;
    bool csi_secondary_ = false;
    int csi_params_[16] = {};
    int csi_param_count_ = 0;
    int csi_param_value_ = 0;
    bool csi_have_value_ = false;

    lv_obj_t *terminal_container_ = nullptr;
    lv_obj_t *term_canvas_ = nullptr;
    lv_obj_t *scrollbar_track_ = nullptr;
    lv_obj_t *scrollbar_thumb_ = nullptr;
    lv_obj_t *hscrollbar_track_ = nullptr;
    lv_obj_t *hscrollbar_thumb_ = nullptr;
    std::array<lv_obj_t *, BOTTOM_BAR_SLOTS> bottom_labels_{};
    std::array<lv_obj_t *, BOTTOM_BAR_SLOTS> bottom_indicators_{};
    const lv_font_t *mono_font_ = nullptr;
    lv_obj_t *cursor_label_ = nullptr;
    lv_timer_t *poll_timer_ = nullptr;
    lv_timer_t *cursor_timer_ = nullptr;

    std::string pty_handle_;
    bool terminal_active_ = false;
    bool waiting_key_to_exit_ = false;
    bool cursor_blink_visible_ = false;
    bool cursor_visible_mode_ = true;
    bool shift_down_ = false;
    bool big_mode_ = false;
    int home_hold_status_ = 0;
    std::chrono::time_point<std::chrono::steady_clock> home_hold_start_{};

    static int clamp(int v, int lo, int hi)
    {
        if (v < lo)
            return lo;
        if (v > hi)
            return hi;
        return v;
    }

    static char printable(uint32_t u)
    {
        if (u < 32 || u == 127)
            return ' ';
        if (u > 126)
            return '?';
        return (char)u;
    }

    static lv_color_t palette(uint32_t color)
    {
        static const uint32_t colors[] = {
            0x0D1117, 0xFF5F56, 0x27C93F, 0xFFBD2E,
            0x2F81F7, 0xBC8CFF, 0x39C5CF, 0xF0F6FC,
            0x6E7681, 0xFFA198, 0x56D364, 0xE3B341,
            0x79C0FF, 0xD2A8FF, 0x56D4DD, 0xFFFFFF,
        };
        return lv_color_hex(colors[color < 16 ? color : DEFAULT_FG]);
    }

    static const lv_font_t *terminal_font()
    {
        return &ui_font_liberation_mono_11;
    }

    static uint32_t xterm256_to_palette(int color)
    {
        color = clamp(color, 0, 255);
        if (color < 16)
            return (uint32_t)color;
        if (color >= 232)
            return color >= 244 ? 15 : 8;

        int idx = color - 16;
        int r = idx / 36;
        int g = (idx / 6) % 6;
        int b = idx % 6;
        if (r >= g && r >= b)
            return r >= 3 ? 9 : 1;
        if (g >= r && g >= b)
            return g >= 3 ? 10 : 2;
        return b >= 3 ? 12 : 4;
    }

    static uint32_t rgb_to_palette(int r, int g, int b)
    {
        r = clamp(r, 0, 255);
        g = clamp(g, 0, 255);
        b = clamp(b, 0, 255);
        int maxc = std::max(r, std::max(g, b));
        int minc = std::min(r, std::min(g, b));
        if (maxc < 80)
            return 0;
        if (maxc - minc < 35)
            return maxc > 180 ? 15 : 8;
        if (r >= g && r >= b)
            return r > 180 ? 9 : 1;
        if (g >= r && g >= b)
            return g > 180 ? 10 : 2;
        return b > 180 ? 12 : 4;
    }

    Glyph blank_glyph() const
    {
        Glyph g;
        g.u = ' ';
        g.attr = cursor_.attr.attr;
        g.fg = cursor_.attr.fg;
        g.bg = cursor_.attr.bg;
        return g;
    }

    void dirty_row(int row)
    {
        if (big_mode_) {
            int view_row = row - viewport_y_;
            if (view_row >= 0 && view_row < visible_rows())
                dirty_[view_row] = true;
            return;
        }
        if (row >= 0 && row < ROWS)
            dirty_[row] = true;
    }

    void dirty_all()
    {
        dirty_.fill(true);
        for (auto &segments : row_segments_) {
            for (auto &segment : segments) {
                segment.text.clear();
                segment.fg = UINT32_MAX;
                segment.bg = UINT32_MAX;
                segment.hidden = true;
                if (segment.label)
                    lv_obj_add_flag(segment.label, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    int visible_cols() const
    {
        return NORMAL_COLS;
    }

    int visible_rows() const
    {
        return big_mode_ ? BIG_VIEW_ROWS : NORMAL_ROWS;
    }

    int visible_h() const
    {
        return visible_rows() * CHAR_H;
    }

    int max_viewport_x() const
    {
        return std::max(0, term_cols_ - visible_cols());
    }

    int max_viewport_y() const
    {
        return std::max(0, term_rows_ - visible_rows());
    }

    void append_scrollback_row(const std::array<Glyph, MAX_COLS> &row)
    {
        scrollback_.push_back(row);
        if ((int)scrollback_.size() > SCROLLBACK_MAX_ROWS) {
            int drop = (int)scrollback_.size() - SCROLLBACK_MAX_ROWS;
            scrollback_.erase(scrollback_.begin(), scrollback_.begin() + drop);
            scrollback_offset_ = std::max(0, scrollback_offset_ - drop);
        }
    }

    const std::array<Glyph, MAX_COLS> &display_row(int r) const
    {
        static const std::array<Glyph, MAX_COLS> empty_row{};
        if (big_mode_) {
            int y = clamp(viewport_y_ + r, 0, term_rows_ - 1);
            return screen_[(size_t)y];
        }
        int history_rows = (int)scrollback_.size();
        int total_rows = history_rows + term_rows_;
        int top = total_rows - term_rows_ - scrollback_offset_;
        int idx = top + r;
        if (idx < 0 || idx >= total_rows)
            return empty_row;
        if (idx < history_rows)
            return scrollback_[(size_t)idx];
        return screen_[(size_t)(idx - history_rows)];
    }

    void scrollback_page(int direction)
    {
        int old_offset = scrollback_offset_;
        int page = std::max(1, visible_rows() - 1);
        if (direction > 0)
            scrollback_offset_ = std::min((int)scrollback_.size(), scrollback_offset_ + page);
        else
            scrollback_offset_ = std::max(0, scrollback_offset_ - page);
        if (scrollback_offset_ != old_offset)
            dirty_all();
    }

    void update_scrollbar()
    {
        if (!scrollbar_track_ || !scrollbar_thumb_)
            return;

        if (big_mode_) {
            int max_y = max_viewport_y();
            lv_obj_clear_flag(scrollbar_track_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(scrollbar_thumb_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(scrollbar_track_, SCROLLBAR_W, visible_h());
            lv_obj_set_pos(scrollbar_track_, TERM_W - SCROLLBAR_W - 1, 0);

            int thumb_h = std::max(8, visible_h() * visible_rows() / std::max(term_rows_, 1));
            thumb_h = std::min(visible_h(), thumb_h);
            int range = visible_h() - thumb_h;
            int thumb_y = max_y > 0 ? viewport_y_ * range / max_y : 0;
            lv_obj_set_size(scrollbar_thumb_, SCROLLBAR_W, thumb_h);
            lv_obj_set_pos(scrollbar_thumb_, TERM_W - SCROLLBAR_W - 1, thumb_y);
            lv_obj_move_foreground(scrollbar_track_);
            lv_obj_move_foreground(scrollbar_thumb_);
            return;
        }

        int history_rows = (int)scrollback_.size();
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
        int thumb_h = std::max(8, TERM_H * term_rows_ / std::max(total_rows, 1));
        thumb_h = std::min(TERM_H, thumb_h);
        int range = TERM_H - thumb_h;
        int max_offset = std::max(1, history_rows);
        int thumb_y = range - (scrollback_offset_ * range / max_offset);

        lv_obj_set_size(scrollbar_thumb_, SCROLLBAR_W, thumb_h);
        lv_obj_set_pos(scrollbar_thumb_, TERM_W - SCROLLBAR_W - 1, thumb_y);
        lv_obj_move_foreground(scrollbar_track_);
        lv_obj_move_foreground(scrollbar_thumb_);
    }

    void update_hscrollbar()
    {
        if (!hscrollbar_track_ || !hscrollbar_thumb_)
            return;
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
        int thumb_w = std::max(18, width * visible_cols() / std::max(term_cols_, 1));
        thumb_w = std::min(width, thumb_w);
        int range = width - thumb_w;
        int thumb_x = max_x > 0 ? viewport_x_ * range / max_x : 0;
        lv_obj_set_size(hscrollbar_thumb_, thumb_w, 3);
        lv_obj_set_pos(hscrollbar_thumb_, thumb_x, y);
        lv_obj_move_foreground(hscrollbar_track_);
        lv_obj_move_foreground(hscrollbar_thumb_);
    }

    void set_bottom_label(int idx, const char *text)
    {
        if (idx < 0 || idx >= BOTTOM_BAR_SLOTS || !bottom_labels_[(size_t)idx])
            return;
        lv_label_set_text(bottom_labels_[(size_t)idx], text);
    }

    void update_big_mode_ui()
    {
        bool show = big_mode_;
        for (auto *label : bottom_labels_) {
            if (!label)
                continue;
            if (show)
                lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
        for (auto *label : bottom_indicators_) {
            if (!label)
                continue;
            if (show)
                lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
        update_hscrollbar();
        update_scrollbar();
    }

    void switch_big_mode(bool enable)
    {
        if (big_mode_ == enable)
            return;

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

    void leave_scrollback()
    {
        if (scrollback_offset_ == 0)
            return;
        scrollback_offset_ = 0;
        dirty_all();
    }

    void pan_big_view(int dx, int dy)
    {
        if (!big_mode_)
            return;
        big_view_locked_ = true;
        int old_x = viewport_x_;
        int old_y = viewport_y_;
        viewport_x_ = clamp(viewport_x_ + dx, 0, max_viewport_x());
        viewport_y_ = clamp(viewport_y_ + dy, 0, max_viewport_y());
        if (viewport_x_ != old_x || viewport_y_ != old_y)
            dirty_all();
    }

    void follow_cursor_in_big_mode()
    {
        if (!big_mode_ || big_view_locked_)
            return;
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
        if (viewport_x_ != old_x || viewport_y_ != old_y)
            dirty_all();
    }

    void reset_terminal()
    {
        cursor_ = Cursor{};
        cursor_.attr.fg = DEFAULT_FG;
        cursor_.attr.bg = DEFAULT_BG;
        saved_cursor_ = cursor_;
        scroll_top_ = 0;
        scroll_bot_ = term_rows_ - 1;
        scrollback_.clear();
        scrollback_offset_ = 0;
        mode_ = MODE_WRAP;
        parse_state_ = ParseState::Normal;
        cursor_visible_mode_ = true;
        csi_reset();
        for (auto &row : screen_)
            for (auto &cell : row)
                cell = blank_glyph();
        dirty_all();
    }

    void clear_region(int x1, int y1, int x2, int y2)
    {
        x1 = clamp(x1, 0, term_cols_ - 1);
        x2 = clamp(x2, 0, term_cols_ - 1);
        y1 = clamp(y1, 0, term_rows_ - 1);
        y2 = clamp(y2, 0, term_rows_ - 1);
        if (x1 > x2)
            std::swap(x1, x2);
        if (y1 > y2)
            std::swap(y1, y2);

        Glyph blank = blank_glyph();
        for (int y = y1; y <= y2; ++y) {
            for (int x = x1; x <= x2; ++x)
                screen_[y][x] = blank;
            dirty_row(y);
        }
    }

    void move_to(int x, int y)
    {
        cursor_.x = clamp(x, 0, term_cols_ - 1);
        cursor_.y = clamp(y, 0, term_rows_ - 1);
    }

    void scroll_up(int top, int bot, int n)
    {
        top = clamp(top, 0, term_rows_ - 1);
        bot = clamp(bot, 0, term_rows_ - 1);
        if (top > bot || n <= 0)
            return;
        n = std::min(n, bot - top + 1);
        if (!big_mode_ && top == 0 && bot == term_rows_ - 1) {
            for (int y = 0; y < n; ++y)
                append_scrollback_row(screen_[y]);
        }
        for (int y = top; y <= bot - n; ++y) {
            screen_[y] = screen_[y + n];
            dirty_row(y);
        }
        Glyph blank = blank_glyph();
        for (int y = bot - n + 1; y <= bot; ++y) {
            for (auto &cell : screen_[y])
                cell = blank;
            dirty_row(y);
        }
    }

    void scroll_down(int top, int bot, int n)
    {
        top = clamp(top, 0, term_rows_ - 1);
        bot = clamp(bot, 0, term_rows_ - 1);
        if (top > bot || n <= 0)
            return;
        n = std::min(n, bot - top + 1);
        for (int y = bot; y >= top + n; --y) {
            screen_[y] = screen_[y - n];
            dirty_row(y);
        }
        Glyph blank = blank_glyph();
        for (int y = top; y < top + n; ++y) {
            for (auto &cell : screen_[y])
                cell = blank;
            dirty_row(y);
        }
    }

    void newline(bool first_col)
    {
        if (first_col)
            cursor_.x = 0;
        if (cursor_.y == scroll_bot_)
            scroll_up(scroll_top_, scroll_bot_, 1);
        else
            cursor_.y = clamp(cursor_.y + 1, 0, term_rows_ - 1);
    }

    void put_tab()
    {
        int next = ((cursor_.x / 8) + 1) * 8;
        cursor_.x = clamp(next, 0, term_cols_ - 1);
    }

    void insert_blank(int n)
    {
        n = std::max(n, 1);
        n = std::min(n, term_cols_ - cursor_.x);
        auto &line = screen_[cursor_.y];
        for (int x = term_cols_ - 1; x >= cursor_.x + n; --x)
            line[x] = line[x - n];
        Glyph blank = blank_glyph();
        for (int x = cursor_.x; x < cursor_.x + n; ++x)
            line[x] = blank;
        dirty_row(cursor_.y);
    }

    void delete_chars(int n)
    {
        n = std::max(n, 1);
        n = std::min(n, term_cols_ - cursor_.x);
        auto &line = screen_[cursor_.y];
        for (int x = cursor_.x; x < term_cols_ - n; ++x)
            line[x] = line[x + n];
        Glyph blank = blank_glyph();
        for (int x = term_cols_ - n; x < term_cols_; ++x)
            line[x] = blank;
        dirty_row(cursor_.y);
    }

    void put_rune(uint32_t rune)
    {
        if (mode_ & MODE_INSERT)
            insert_blank(1);

        if (cursor_.x >= term_cols_) {
            if (mode_ & MODE_WRAP) {
                cursor_.x = 0;
                newline(false);
            } else {
                cursor_.x = term_cols_ - 1;
            }
        }

        Glyph g = cursor_.attr;
        g.u = rune;
        screen_[cursor_.y][cursor_.x] = g;
        dirty_row(cursor_.y);
        cursor_.x++;
    }

    void control_code(uint8_t c)
    {
        switch (c) {
        case '\t':
            put_tab();
            break;
        case '\b':
            cursor_.x = std::max(0, cursor_.x - 1);
            break;
        case '\r':
            cursor_.x = 0;
            break;
        case '\n':
        case '\v':
        case '\f':
            newline(true);
            break;
        case 0x0e:
        case 0x0f:
            break;
        default:
            break;
        }
    }

    int param(int index, int def) const
    {
        if (index >= csi_param_count_)
            return def;
        return csi_params_[index] == 0 ? def : csi_params_[index];
    }

    void csi_reset()
    {
        csi_private_ = false;
        csi_secondary_ = false;
        csi_param_count_ = 0;
        csi_param_value_ = 0;
        csi_have_value_ = false;
        memset(csi_params_, 0, sizeof(csi_params_));
    }

    void csi_push_param()
    {
        if (csi_param_count_ >= (int)(sizeof(csi_params_) / sizeof(csi_params_[0])))
            return;
        csi_params_[csi_param_count_++] = csi_have_value_ ? csi_param_value_ : 0;
        csi_param_value_ = 0;
        csi_have_value_ = false;
    }

    void set_sgr()
    {
        if (csi_param_count_ == 0) {
            cursor_.attr.attr = ATTR_NULL;
            cursor_.attr.fg = DEFAULT_FG;
            cursor_.attr.bg = DEFAULT_BG;
            return;
        }

        for (int i = 0; i < csi_param_count_; ++i) {
            int val = csi_params_[i];
            if (val == 0) {
                cursor_.attr.attr = ATTR_NULL;
                cursor_.attr.fg = DEFAULT_FG;
                cursor_.attr.bg = DEFAULT_BG;
            } else if (val == 1) {
                cursor_.attr.attr |= ATTR_BOLD;
            } else if (val == 2) {
                cursor_.attr.attr |= ATTR_FAINT;
            } else if (val == 4) {
                cursor_.attr.attr |= ATTR_UNDERLINE;
            } else if (val == 5) {
                cursor_.attr.attr |= ATTR_BLINK;
            } else if (val == 7) {
                cursor_.attr.attr |= ATTR_REVERSE;
            } else if (val == 8) {
                cursor_.attr.attr |= ATTR_INVISIBLE;
            } else if (val == 22) {
                cursor_.attr.attr &= ~(ATTR_BOLD | ATTR_FAINT);
            } else if (val == 24) {
                cursor_.attr.attr &= ~ATTR_UNDERLINE;
            } else if (val == 25) {
                cursor_.attr.attr &= ~ATTR_BLINK;
            } else if (val == 27) {
                cursor_.attr.attr &= ~ATTR_REVERSE;
            } else if (val == 28) {
                cursor_.attr.attr &= ~ATTR_INVISIBLE;
            } else if (val >= 30 && val <= 37) {
                cursor_.attr.fg = (uint32_t)(val - 30);
            } else if (val >= 40 && val <= 47) {
                cursor_.attr.bg = (uint32_t)(val - 40);
            } else if (val >= 90 && val <= 97) {
                cursor_.attr.fg = (uint32_t)(val - 90 + 8);
            } else if (val >= 100 && val <= 107) {
                cursor_.attr.bg = (uint32_t)(val - 100 + 8);
            } else if ((val == 38 || val == 48) && i + 2 < csi_param_count_ && csi_params_[i + 1] == 5) {
                uint32_t mapped = xterm256_to_palette(csi_params_[i + 2]);
                if (val == 38)
                    cursor_.attr.fg = mapped;
                else
                    cursor_.attr.bg = mapped;
                i += 2;
            } else if ((val == 38 || val == 48) && i + 4 < csi_param_count_ && csi_params_[i + 1] == 2) {
                uint32_t mapped = rgb_to_palette(csi_params_[i + 2], csi_params_[i + 3], csi_params_[i + 4]);
                if (val == 38)
                    cursor_.attr.fg = mapped;
                else
                    cursor_.attr.bg = mapped;
                i += 4;
            } else if (val == 39) {
                cursor_.attr.fg = DEFAULT_FG;
            } else if (val == 49) {
                cursor_.attr.bg = DEFAULT_BG;
            }
        }
    }

    void handle_private_mode(char final)
    {
        bool set = final == 'h';
        for (int i = 0; i < csi_param_count_; ++i) {
            switch (csi_params_[i]) {
            case 1:
                if (set)
                    mode_ |= MODE_APPCURSOR;
                else
                    mode_ &= ~MODE_APPCURSOR;
                break;
            case 7:
                if (set)
                    mode_ |= MODE_WRAP;
                else
                    mode_ &= ~MODE_WRAP;
                break;
            case 25:
                cursor_visible_mode_ = set;
                break;
            case 1049:
                if (set)
                    clear_region(0, 0, term_cols_ - 1, term_rows_ - 1);
                break;
            default:
                break;
            }
        }
    }

    void handle_csi(char final)
    {
        if (csi_secondary_) {
            if (final == 'c')
                pty_write("\033[>0;115;0c", 11);
            return;
        }

        if (csi_private_ && (final == 'h' || final == 'l')) {
            handle_private_mode(final);
            return;
        }

        switch (final) {
        case '@':
            insert_blank(param(0, 1));
            break;
        case 'A':
            move_to(cursor_.x, cursor_.y - param(0, 1));
            break;
        case 'B':
            move_to(cursor_.x, cursor_.y + param(0, 1));
            break;
        case 'C':
            move_to(cursor_.x + param(0, 1), cursor_.y);
            break;
        case 'D':
            move_to(cursor_.x - param(0, 1), cursor_.y);
            break;
        case 'G':
            move_to(param(0, 1) - 1, cursor_.y);
            break;
        case 'H':
        case 'f':
            move_to(param(1, 1) - 1, param(0, 1) - 1);
            break;
        case 'J':
            if (param(0, 0) == 0)
                clear_region(cursor_.x, cursor_.y, term_cols_ - 1, term_rows_ - 1);
            else if (param(0, 0) == 1)
                clear_region(0, 0, cursor_.x, cursor_.y);
            else
                clear_region(0, 0, term_cols_ - 1, term_rows_ - 1);
            break;
        case 'K':
            if (param(0, 0) == 0)
                clear_region(cursor_.x, cursor_.y, term_cols_ - 1, cursor_.y);
            else if (param(0, 0) == 1)
                clear_region(0, cursor_.y, cursor_.x, cursor_.y);
            else
                clear_region(0, cursor_.y, term_cols_ - 1, cursor_.y);
            break;
        case 'L':
            scroll_down(cursor_.y, scroll_bot_, param(0, 1));
            break;
        case 'M':
            scroll_up(cursor_.y, scroll_bot_, param(0, 1));
            break;
        case 'P':
            delete_chars(param(0, 1));
            break;
        case 'd':
            move_to(cursor_.x, param(0, 1) - 1);
            break;
        case 'h':
            if (param(0, 0) == 4)
                mode_ |= MODE_INSERT;
            break;
        case 'l':
            if (param(0, 0) == 4)
                mode_ &= ~MODE_INSERT;
            break;
        case 'm':
            set_sgr();
            break;
        case 'n':
            if (param(0, 0) == 5) {
                pty_write("\033[0n", 4);
            } else if (param(0, 0) == 6) {
                char reply[32];
                int len = snprintf(reply, sizeof(reply), "\033[%d;%dR", cursor_.y + 1, cursor_.x + 1);
                pty_write(reply, (size_t)len);
            }
            break;
        case 'r':
            scroll_top_ = clamp(param(0, 1) - 1, 0, term_rows_ - 1);
            scroll_bot_ = clamp(param(1, term_rows_) - 1, 0, term_rows_ - 1);
            if (scroll_top_ >= scroll_bot_) {
                scroll_top_ = 0;
                scroll_bot_ = term_rows_ - 1;
            }
            move_to(0, 0);
            break;
        case 's':
            saved_cursor_ = cursor_;
            break;
        case 'u':
            cursor_ = saved_cursor_;
            move_to(cursor_.x, cursor_.y);
            break;
        case 'c':
            pty_write("\033[?1;2c", 7);
            break;
        default:
            break;
        }
    }

    void handle_esc(uint8_t c)
    {
        switch (c) {
        case '[':
            csi_reset();
            parse_state_ = ParseState::Csi;
            return;
        case ']':
            parse_state_ = ParseState::Osc;
            return;
        case '(':
        case ')':
        case '*':
        case '+':
            parse_state_ = ParseState::Charset;
            return;
        case '7':
            saved_cursor_ = cursor_;
            break;
        case '8':
            cursor_ = saved_cursor_;
            move_to(cursor_.x, cursor_.y);
            break;
        case 'D':
            newline(false);
            break;
        case 'E':
            newline(true);
            break;
        case 'M':
            if (cursor_.y == scroll_top_)
                scroll_down(scroll_top_, scroll_bot_, 1);
            else
                move_to(cursor_.x, cursor_.y - 1);
            break;
        case 'c':
            reset_terminal();
            break;
        default:
            break;
        }
        parse_state_ = ParseState::Normal;
    }

    void process_bytes(const char *data, int len)
    {
        for (int i = 0; i < len; ++i) {
            uint8_t c = (uint8_t)data[i];

            if (parse_state_ == ParseState::Osc) {
                if (c == 0x07)
                    parse_state_ = ParseState::Normal;
                else if (c == 0x1b && i + 1 < len && data[i + 1] == '\\') {
                    ++i;
                    parse_state_ = ParseState::Normal;
                }
                continue;
            }

            if (parse_state_ == ParseState::Charset) {
                parse_state_ = ParseState::Normal;
                continue;
            }

            if (parse_state_ == ParseState::Esc) {
                handle_esc(c);
                continue;
            }

            if (parse_state_ == ParseState::Csi) {
                if (c == '?') {
                    csi_private_ = true;
                    continue;
                }
                if (c == '>') {
                    csi_secondary_ = true;
                    continue;
                }
                if (isdigit(c)) {
                    csi_param_value_ = csi_param_value_ * 10 + (c - '0');
                    csi_have_value_ = true;
                    continue;
                }
                if (c == ';') {
                    csi_push_param();
                    continue;
                }
                if (c >= 0x20 && c <= 0x2f)
                    continue;
                csi_push_param();
                handle_csi((char)c);
                parse_state_ = ParseState::Normal;
                continue;
            }

            if (c == 0x1b) {
                parse_state_ = ParseState::Esc;
            } else if (c < 0x20 || c == 0x7f) {
                control_code(c);
            } else {
                put_rune(c);
            }
        }
    }

    void create_ui()
    {
        terminal_container_ = lv_obj_create(ui_APP_Container);
        lv_obj_remove_style_all(terminal_container_);
        lv_obj_set_size(terminal_container_, TERM_W, TERM_H);
        lv_obj_set_pos(terminal_container_, 0, 0);
        lv_obj_set_style_bg_color(terminal_container_, palette(DEFAULT_BG), 0);
        lv_obj_set_style_bg_opa(terminal_container_, LV_OPA_COVER, 0);
        lv_obj_clear_flag(terminal_container_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        term_canvas_ = lv_obj_create(terminal_container_);
        lv_obj_set_size(term_canvas_, TERM_W, TERM_H);
        lv_obj_set_pos(term_canvas_, 0, 0);
        lv_obj_set_style_bg_color(term_canvas_, palette(DEFAULT_BG), 0);
        lv_obj_set_style_bg_opa(term_canvas_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(term_canvas_, 0, 0);
        lv_obj_set_style_pad_all(term_canvas_, 0, 0);
        lv_obj_set_style_radius(term_canvas_, 0, 0);
        lv_obj_clear_flag(term_canvas_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        scrollbar_track_ = lv_obj_create(terminal_container_);
        lv_obj_remove_style_all(scrollbar_track_);
        lv_obj_set_size(scrollbar_track_, SCROLLBAR_W, TERM_H);
        lv_obj_set_pos(scrollbar_track_, TERM_W - SCROLLBAR_W - 1, 0);
        lv_obj_set_style_bg_color(scrollbar_track_, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_bg_opa(scrollbar_track_, LV_OPA_COVER, 0);
        lv_obj_clear_flag(scrollbar_track_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_add_flag(scrollbar_track_, LV_OBJ_FLAG_HIDDEN);

        scrollbar_thumb_ = lv_obj_create(terminal_container_);
        lv_obj_remove_style_all(scrollbar_thumb_);
        lv_obj_set_size(scrollbar_thumb_, SCROLLBAR_W, 8);
        lv_obj_set_pos(scrollbar_thumb_, TERM_W - SCROLLBAR_W - 1, TERM_H - 8);
        lv_obj_set_style_bg_color(scrollbar_thumb_, lv_color_hex(0x8B949E), 0);
        lv_obj_set_style_bg_opa(scrollbar_thumb_, LV_OPA_COVER, 0);
        lv_obj_clear_flag(scrollbar_thumb_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_add_flag(scrollbar_thumb_, LV_OBJ_FLAG_HIDDEN);

        hscrollbar_track_ = lv_obj_create(terminal_container_);
        lv_obj_remove_style_all(hscrollbar_track_);
        lv_obj_set_size(hscrollbar_track_, TERM_W - SCROLLBAR_W - 2, 3);
        lv_obj_set_pos(hscrollbar_track_, 0, BIG_VIEW_ROWS * CHAR_H);
        lv_obj_set_style_bg_color(hscrollbar_track_, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_bg_opa(hscrollbar_track_, LV_OPA_COVER, 0);
        lv_obj_clear_flag(hscrollbar_track_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_add_flag(hscrollbar_track_, LV_OBJ_FLAG_HIDDEN);

        hscrollbar_thumb_ = lv_obj_create(terminal_container_);
        lv_obj_remove_style_all(hscrollbar_thumb_);
        lv_obj_set_size(hscrollbar_thumb_, 18, 3);
        lv_obj_set_pos(hscrollbar_thumb_, 0, BIG_VIEW_ROWS * CHAR_H);
        lv_obj_set_style_bg_color(hscrollbar_thumb_, lv_color_hex(0x8B949E), 0);
        lv_obj_set_style_bg_opa(hscrollbar_thumb_, LV_OPA_COVER, 0);
        lv_obj_clear_flag(hscrollbar_thumb_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_add_flag(hscrollbar_thumb_, LV_OBJ_FLAG_HIDDEN);

        static const char *bottom_text[BOTTOM_BAR_SLOTS] = {"F4 <", "F5 up", "F6 normal", "F7 down", "F8 >"};
        constexpr int slot_w = TERM_W / BOTTOM_BAR_SLOTS;
        for (int i = 0; i < BOTTOM_BAR_SLOTS; ++i) {
            bottom_labels_[(size_t)i] = lv_label_create(terminal_container_);
            lv_obj_set_pos(bottom_labels_[(size_t)i], i * slot_w, TERM_H - BIG_BOTTOM_H);
            lv_obj_set_size(bottom_labels_[(size_t)i], slot_w, BIG_BOTTOM_H);
            lv_obj_set_style_text_font(bottom_labels_[(size_t)i], &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(bottom_labels_[(size_t)i], lv_color_hex(0xF0F6FC), 0);
            lv_obj_set_style_text_align(bottom_labels_[(size_t)i], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_pad_top(bottom_labels_[(size_t)i], 1, 0);
            lv_label_set_text(bottom_labels_[(size_t)i], bottom_text[i]);
            lv_obj_add_flag(bottom_labels_[(size_t)i], LV_OBJ_FLAG_HIDDEN);

            bottom_indicators_[(size_t)i] = lv_label_create(terminal_container_);
            lv_obj_set_pos(bottom_indicators_[(size_t)i], i * slot_w, TERM_H - 4);
            lv_obj_set_size(bottom_indicators_[(size_t)i], slot_w, 4);
            lv_obj_set_style_text_font(bottom_indicators_[(size_t)i], &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(bottom_indicators_[(size_t)i], lv_color_hex(0x8B949E), 0);
            lv_obj_set_style_text_align(bottom_indicators_[(size_t)i], LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(bottom_indicators_[(size_t)i], "|");
            lv_obj_add_flag(bottom_indicators_[(size_t)i], LV_OBJ_FLAG_HIDDEN);
        }

        mono_font_ = launcher_fonts().get_mono("LiberationMono-Regular.ttf", 11, LV_FREETYPE_FONT_STYLE_NORMAL);

        cursor_label_ = lv_label_create(term_canvas_);
        lv_obj_set_style_text_font(cursor_label_, mono_font_, 0);
        lv_obj_set_style_text_color(cursor_label_, palette(DEFAULT_BG), 0);
        lv_obj_set_style_bg_color(cursor_label_, palette(DEFAULT_FG), 0);
        lv_obj_set_style_bg_opa(cursor_label_, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(cursor_label_, 0, 0);
        lv_obj_set_style_pad_top(cursor_label_, TEXT_Y_PAD, 0);
        lv_obj_set_style_text_letter_space(cursor_label_, 0, 0);
        lv_label_set_long_mode(cursor_label_, LV_LABEL_LONG_CLIP);
        lv_obj_set_size(cursor_label_, CHAR_W, CHAR_H);
        lv_label_set_text(cursor_label_, " ");
        lv_obj_add_flag(cursor_label_, LV_OBJ_FLAG_HIDDEN);
    }

    void bind_events()
    {
        lv_obj_add_event_cb(root_screen_, UISTPage::static_event_cb, LV_EVENT_ALL, this);
    }

    static void static_event_cb(lv_event_t *e)
    {
        auto *self = static_cast<UISTPage *>(lv_event_get_user_data(e));
        if (self)
            self->event_cb(e);
    }

    void event_cb(lv_event_t *e)
    {
        if (lv_event_get_code(e) != LV_EVENT_KEYBOARD)
            return;
        struct key_item *elm = (struct key_item *)lv_event_get_param(e);
        if (!elm)
            return;

        if (waiting_key_to_exit_) {
            if (elm->key_state == 0) {
                if (terminal_sysplause) {
                    terminal_sysplause = false;
                } else {
                    waiting_key_to_exit_ = false;
                    if (navigate_home)
                        navigate_home();
                }
            }
            return;
        }

        if (elm->key_code == KEY_LEFTSHIFT || elm->key_code == KEY_RIGHTSHIFT) {
            shift_down_ = elm->key_state != KBD_KEY_RELEASED;
            return;
        }

        if (elm->key_state && elm->key_code == KEY_F6) {
            switch_big_mode(!big_mode_);
            return;
        }
        if (elm->key_state && big_mode_) {
            switch (elm->key_code) {
            case KEY_F4:
                pan_big_view(-8, 0);
                render_all();
                return;
            case KEY_F8:
                pan_big_view(8, 0);
                render_all();
                return;
            case KEY_F5:
                pan_big_view(0, -4);
                render_all();
                return;
            case KEY_F7:
                pan_big_view(0, 4);
                render_all();
                return;
            default:
                break;
            }
        }

        bool shift = shift_down_ || ((elm->mods & KBD_MOD_SHIFT) != 0);
        if (elm->key_state && shift && elm->key_code == KEY_PAGEUP) {
            scrollback_page(1);
            render_all();
            return;
        }
        if (elm->key_state && shift && elm->key_code == KEY_PAGEDOWN) {
            scrollback_page(-1);
            render_all();
            return;
        }

        if (terminal_active_ && !pty_handle_.empty() && elm->key_state) {
            leave_scrollback();
            follow_cursor_in_big_mode();
            write_key(elm->key_code, elm->utf8);
        }
    }

    static void effective_colors(const Glyph &g, uint32_t *fg, uint32_t *bg)
    {
        uint32_t out_fg = g.fg;
        uint32_t out_bg = g.bg;
        if (g.attr & ATTR_REVERSE)
            std::swap(out_fg, out_bg);
        if (g.attr & ATTR_BOLD)
            out_fg = out_fg < 8 ? out_fg + 8 : out_fg;
        if (fg)
            *fg = out_fg;
        if (bg)
            *bg = out_bg;
    }

    bool meaningful_cell(const Glyph &g) const
    {
        uint32_t fg = DEFAULT_FG;
        uint32_t bg = DEFAULT_BG;
        effective_colors(g, &fg, &bg);
        char ch = (g.attr & ATTR_INVISIBLE) ? ' ' : printable(g.u);
        return ch != ' ' || fg != DEFAULT_FG || bg != DEFAULT_BG;
    }

    lv_obj_t *create_segment_label()
    {
        lv_obj_t *lbl = lv_label_create(term_canvas_);
        lv_obj_set_style_text_font(lbl, mono_font_, 0);
        lv_obj_set_style_text_color(lbl, palette(DEFAULT_FG), 0);
        lv_obj_set_style_bg_color(lbl, palette(DEFAULT_BG), 0);
        lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(lbl, 0, 0);
        lv_obj_set_style_pad_top(lbl, TEXT_Y_PAD, 0);
        lv_obj_set_style_text_letter_space(lbl, 0, 0);
        lv_obj_set_style_text_line_space(lbl, 0, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_size(lbl, CHAR_W, CHAR_H);
        lv_label_set_text(lbl, " ");
        lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
        return lbl;
    }

    std::vector<SegmentData> build_row_segments(int r)
    {
        std::vector<SegmentData> out;
        const auto &line = display_row(r);
        int last = -1;
        int first_col = big_mode_ ? viewport_x_ : 0;
        int cols = visible_cols();
        for (int c = cols - 1; c >= 0; --c) {
            if (meaningful_cell(line[(size_t)(first_col + c)])) {
                last = c;
                break;
            }
        }
        if (last < 0)
            return out;

        SegmentData current;
        bool has_current = false;
        for (int c = 0; c <= last; ++c) {
            const Glyph &g = line[(size_t)(first_col + c)];
            uint32_t fg = DEFAULT_FG;
            uint32_t bg = DEFAULT_BG;
            effective_colors(g, &fg, &bg);
            char ch = (g.attr & ATTR_INVISIBLE) ? ' ' : printable(g.u);

            if (!has_current || current.fg != fg || current.bg != bg) {
                if (has_current)
                    out.push_back(current);
                current = SegmentData{};
                current.x = c;
                current.fg = fg;
                current.bg = bg;
                has_current = true;
            }
            current.text.push_back(ch);
        }
        if (has_current)
            out.push_back(current);
        return out;
    }

    void render_row(int r)
    {
        std::vector<SegmentData> desired = build_row_segments(r);
        std::vector<RenderSegment> &rendered = row_segments_[r];
        if (rendered.size() < desired.size())
            rendered.resize(desired.size());

        for (size_t i = 0; i < desired.size(); ++i) {
            SegmentData &want = desired[i];
            RenderSegment &have = rendered[i];
            if (!have.label)
                have.label = create_segment_label();

            int width = (int)want.text.size() * CHAR_W;
            bool changed = have.hidden || have.x != want.x || have.width != width ||
                           have.fg != want.fg || have.bg != want.bg || have.text != want.text;
            if (!changed)
                continue;

            have.hidden = false;
            have.x = want.x;
            have.width = width;
            have.fg = want.fg;
            have.bg = want.bg;
            have.text = want.text;

            lv_obj_clear_flag(have.label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(have.label, want.x * CHAR_W, r * CHAR_H);
            lv_obj_set_size(have.label, width, CHAR_H);
            lv_obj_set_style_text_color(have.label, palette(want.fg), 0);
            lv_obj_set_style_bg_color(have.label, palette(want.bg), 0);
            lv_label_set_text(have.label, want.text.c_str());
        }

        for (size_t i = desired.size(); i < rendered.size(); ++i) {
            RenderSegment &segment = rendered[i];
            if (!segment.hidden && segment.label) {
                lv_obj_add_flag(segment.label, LV_OBJ_FLAG_HIDDEN);
                segment.hidden = true;
                segment.text.clear();
            }
        }
    }

    void render_all()
    {
        int rows = visible_rows();
        for (int r = 0; r < ROWS; ++r) {
            if (r >= rows) {
                if (dirty_[r]) {
                    for (auto &segment : row_segments_[r]) {
                        if (segment.label)
                            lv_obj_add_flag(segment.label, LV_OBJ_FLAG_HIDDEN);
                        segment.hidden = true;
                    }
                    dirty_[r] = false;
                }
                continue;
            }
            if (dirty_[r]) {
                render_row(r);
                dirty_[r] = false;
            }
        }
        update_cursor();
        update_hscrollbar();
        update_scrollbar();
    }

    void update_cursor()
    {
        if (!cursor_label_)
            return;
        if (scrollback_offset_ > 0) {
            lv_obj_add_flag(cursor_label_, LV_OBJ_FLAG_HIDDEN);
            cursor_blink_visible_ = false;
            return;
        }
        int x = clamp(cursor_.x - (big_mode_ ? viewport_x_ : 0), 0, visible_cols() - 1);
        int y = clamp(cursor_.y - (big_mode_ ? viewport_y_ : 0), 0, visible_rows() - 1);
        if (big_mode_ && (cursor_.x < viewport_x_ || cursor_.x >= viewport_x_ + visible_cols() ||
                          cursor_.y < viewport_y_ || cursor_.y >= viewport_y_ + visible_rows())) {
            lv_obj_add_flag(cursor_label_, LV_OBJ_FLAG_HIDDEN);
            cursor_blink_visible_ = false;
            return;
        }
        const Glyph &g = screen_[cursor_.y][cursor_.x];
        uint32_t fg = DEFAULT_FG;
        uint32_t bg = DEFAULT_BG;
        effective_colors(g, &fg, &bg);
        char under = (g.attr & ATTR_INVISIBLE) ? ' ' : printable(g.u);
        char text[2] = {under, '\0'};
        lv_label_set_text(cursor_label_, text);
        lv_obj_set_style_text_color(cursor_label_, palette(bg), 0);
        lv_obj_set_style_bg_color(cursor_label_, palette(fg), 0);
        lv_obj_set_pos(cursor_label_, x * CHAR_W, y * CHAR_H);
        lv_obj_move_foreground(cursor_label_);
    }

    void show_cursor(bool show)
    {
        if (!cursor_label_)
            return;
        if (scrollback_offset_ > 0)
            show = false;
        if (big_mode_ && (cursor_.x < viewport_x_ || cursor_.x >= viewport_x_ + visible_cols() ||
                          cursor_.y < viewport_y_ || cursor_.y >= viewport_y_ + visible_rows()))
            show = false;
        if (show)
            lv_obj_clear_flag(cursor_label_, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(cursor_label_, LV_OBJ_FLAG_HIDDEN);
        cursor_blink_visible_ = show;
    }

    std::string pty_open(const std::string &cmd, const std::list<std::string> &args)
    {
        int code = -1;
        std::string handle;
        std::list<std::string> api_args = {
            "Open",
            cmd,
            std::to_string(term_cols_),
            std::to_string(term_rows_),
            cmd,
        };
        for (const auto &arg : args)
            api_args.push_back(arg);

        cp0_signal_pty_api(std::move(api_args), [&](int c, std::string data) {
            code = c;
            if (code == 0)
                handle = std::move(data);
        });
        return handle;
    }

    void stop_timers()
    {
        if (poll_timer_) {
            lv_timer_delete(poll_timer_);
            poll_timer_ = nullptr;
        }
        if (cursor_timer_) {
            lv_timer_delete(cursor_timer_);
            cursor_timer_ = nullptr;
        }
    }

    void start_timers()
    {
        if (!poll_timer_)
            poll_timer_ = lv_timer_create(UISTPage::static_poll_cb, 30, this);
        if (!cursor_timer_)
            cursor_timer_ = lv_timer_create(UISTPage::static_cursor_cb, 500, this);
    }

    void start_command(const std::string &cmd, const std::list<std::string> &args,
                       const char *title, const char *err_msg)
    {
        stop_pty();
        pty_handle_ = pty_open(cmd, args);
        terminal_active_ = !pty_handle_.empty();
        if (!terminal_active_) {
            process_bytes(err_msg, (int)strlen(err_msg));
            render_all();
            waiting_key_to_exit_ = true;
            return;
        }
        if (title && title[0])
            set_page_title(title);
        start_timers();
        render_all();
    }

    void start_shell()
    {
        start_command("bash", {
            "-c",
            "cd ~ && "
            "if [ -r ~/.bashrc ]; then "
            "exec env TERM=st-256color COLORTERM=truecolor bash --rcfile ~/.bashrc -i; "
            "else "
            "exec env TERM=st-256color COLORTERM=truecolor bash -i; "
            "fi"
        }, "ST", "st: failed to open PTY\r\n");
    }

    void stop_pty()
    {
        if (!pty_handle_.empty()) {
            cp0_signal_pty_api({"Close", pty_handle_}, nullptr);
            pty_handle_.clear();
        }
    }

    int pty_read(char *buf, size_t buf_size)
    {
        int code = -1;
        std::string data;
        cp0_signal_pty_api({"Read", pty_handle_, std::to_string(buf_size)}, [&](int c, std::string d) {
            code = c;
            data = std::move(d);
        });
        if (code < 0)
            return -1;
        size_t n = std::min(data.size(), buf_size);
        if (n > 0)
            memcpy(buf, data.data(), n);
        return (int)n;
    }

    int pty_write(const char *buf, size_t len)
    {
        if (pty_handle_.empty() || !buf || len == 0)
            return -1;
        int code = -1;
        cp0_signal_pty_api({"Write", pty_handle_, std::string(buf, len)}, [&](int c, std::string) {
            code = c;
        });
        return code;
    }

    int pty_check_child(int *status)
    {
        int code = -1;
        std::string data;
        cp0_signal_pty_api({"CheckChild", pty_handle_}, [&](int c, std::string d) {
            code = c;
            data = std::move(d);
        });
        if (status)
            *status = atoi(data.c_str());
        return code;
    }

    static void static_poll_cb(lv_timer_t *timer)
    {
        auto *self = static_cast<UISTPage *>(lv_timer_get_user_data(timer));
        if (self)
            self->poll_cb();
    }

    void poll_cb()
    {
        if (!terminal_active_ || pty_handle_.empty())
            return;

        char buf[1024];
        int n = 0;
        bool changed = false;
        while ((n = pty_read(buf, sizeof(buf))) > 0) {
            process_bytes(buf, n);
            changed = true;
        }

        if (changed) {
            if (scrollback_offset_ > 0)
                dirty_all();
            follow_cursor_in_big_mode();
            if (big_mode_)
                dirty_all();
            render_all();
        }

        bool child_exited = n < 0;
        if (!child_exited && !pty_handle_.empty()) {
            int status = 0;
            child_exited = pty_check_child(&status) == 1;
        }
        if (child_exited) {
            terminal_active_ = false;
            stop_pty();
            const char *hint = "\r\n-- Press any key to exit --";
            process_bytes(hint, (int)strlen(hint));
            render_all();
            waiting_key_to_exit_ = true;
        }
    }

    static void static_cursor_cb(lv_timer_t *timer)
    {
        auto *self = static_cast<UISTPage *>(lv_timer_get_user_data(timer));
        if (self)
            self->cursor_cb();
    }

    void cursor_cb()
    {
        handle_home_hold_exit();
        update_cursor();
        if (!terminal_active_ || !cursor_visible_mode_) {
            show_cursor(false);
            return;
        }
        show_cursor(!cursor_blink_visible_);
    }

    void handle_home_hold_exit()
    {
        if (home_hold_status_ == 0) {
            if (LVGL_HOME_KEY_FLAG) {
                home_hold_status_ = 1;
                home_hold_start_ = std::chrono::steady_clock::now();
            }
            return;
        }

        if (!LVGL_HOME_KEY_FLAG) {
            home_hold_status_ = 0;
            return;
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - home_hold_start_).count() < 5)
            return;

        home_hold_status_ = 0;
        stop_pty();
        terminal_active_ = false;
        if (navigate_home)
            navigate_home();
    }

    void write_key(uint32_t evdev_key, const char *utf8)
    {
        char buf[16];
        int len = 0;

        switch (evdev_key) {
        case 28:
            buf[0] = '\r';
            len = 1;
            break;
        case 14:
            buf[0] = 0x7f;
            len = 1;
            break;
        case 1:
            buf[0] = 0x1b;
            len = 1;
            break;
        case 103:
            len = snprintf(buf, sizeof(buf), "%s", (mode_ & MODE_APPCURSOR) ? "\033OA" : "\033[A");
            break;
        case 108:
            len = snprintf(buf, sizeof(buf), "%s", (mode_ & MODE_APPCURSOR) ? "\033OB" : "\033[B");
            break;
        case 106:
            len = snprintf(buf, sizeof(buf), "%s", (mode_ & MODE_APPCURSOR) ? "\033OC" : "\033[C");
            break;
        case 105:
            len = snprintf(buf, sizeof(buf), "%s", (mode_ & MODE_APPCURSOR) ? "\033OD" : "\033[D");
            break;
        default:
            len = utf8 ? (int)strlen(utf8) : 0;
            if (len > 0 && len < (int)sizeof(buf))
                memcpy(buf, utf8, (size_t)len);
            else
                len = 0;
            break;
        }

        if (len > 0)
            pty_write(buf, (size_t)len);
    }
};
