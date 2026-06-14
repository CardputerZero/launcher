#pragma once
#include "../ui_app_page.hpp"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <unordered_map>
#include <list>
#include <memory>
#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <vector>
#include <sstream>
#include <keyboard_input.h>
#include "hal/hal_pty.h"

// ============================================================
//  Terminal console  UIConsolePage
//  Screen resolution: 320 x 170  (top bar20px, ui_APP_Container 320x150)
//
//  Features:
//    - Full VT100/ANSI terminal emulation (ported from the src/vt100.c state machine)
//      Supports cursor movement, erase, SGR attributes, insert/delete lines/characters,
//            DEC private modes (DECTCEM, DECAWM, DECSCNMetc.),
//            SCP/RCP cursor save/restore, scrolling region, device attributesetc.
//    - PTY child-process management (fork + openpty)
//    - line-level dirty rendering to reduce LVGL refresh overhead
//    - cursor blink (500ms timer)
//    - keyboard input forwarded to PTY (evdev keycode + utf8 / LV_KEY_*)
//
//  Public API:
//    exec(std::string cmd)  — start a command (supports command strings with arguments)
// ============================================================
class UIConsolePage : public app_base
{
    /* ------------------------------------------------------------------ */
    /*  Terminal geometry                                                            */
    /* ------------------------------------------------------------------ */
    static constexpr int TERM_W = 320;
    static constexpr int TERM_H = 150;
    static constexpr int CHAR_W = 7;
    static constexpr int CHAR_H = 12;
    static constexpr int COLS = TERM_W / CHAR_W; /* 45 */
    static constexpr int ROWS = TERM_H / CHAR_H; /* 12 */

    /* green text on black background */
    static constexpr uint32_t FIXED_FG = 0x00FF00u;
    static constexpr uint32_t FIXED_BG = 0x000000u;

    /* ------------------------------------------------------------------ */
    /*  UI objects                                                             */
    /* ------------------------------------------------------------------ */
    lv_obj_t *terminal_container = nullptr;
    lv_obj_t *term_canvas = nullptr;

    /* Full-row rendering: ROWS labels instead of many cell labels */
    lv_obj_t *row_labels[ROWS] = {};
    /* Cursor: a separate inverted label; style is set only once at creation */
    lv_obj_t *cursor_label = nullptr;

    /* line-level dirty comparison cache (including trailing '\0') */
    char row_rendered[ROWS][COLS + 1] = {};

public:
    bool terminal_sysplause = true;

public:
    UIConsolePage() : app_base()
    {
        console_data_init();
        creat_console_UI();
        event_handler_init();
    }

    ~UIConsolePage()
    {
        terminal_active = false;
        if (poll_timer)
        {
            lv_timer_delete(poll_timer);
            poll_timer = nullptr;
        }
        if (cursor_timer)
        {
            lv_timer_delete(cursor_timer);
            cursor_timer = nullptr;
        }
        stop_pty();
    }

    // ==================== Public API ====================

    /**
     * Start a command.
     * The command string is split on spaces; the first token is the executable path and the rest are arguments.
     * If a child process is already running, terminate it before restarting.
     */
    void exec(std::string cmd)
    {
        if (pty_handle != NULL)
            stop_pty();

        terminal_active = true;
        vt100_cur_row = 0;
        vt100_cur_col = 0;
        vt100_cur_attr = 0;
        vt100_cur_fg = 7;
        vt100_cur_bg = 0;
        vt100_saved_row = 0; vt100_saved_col = 0;
        vt100_saved_attr = 0; vt100_saved_fg = 7; vt100_saved_bg = 0;
        vt100_auto_wrap = true;
        vt100_cursor_visible_flag = true;
        vt100_decckm = false;
        vt100_esc_state = VT100_ST_NORMAL;
        vt100_esc_len = 0;
        vt100_param_count = 0;
        vt100_param_val = 0;
        vt100_priv_mode = false;
        vt100_sec_mode = false;
        vt100_skip_until_st = false;
        memset(vt100_params, 0, sizeof(vt100_params));
        waiting_key_to_exit = false;

        vt100_screen_clear_all();
        /* Force the first full render */
        memset(row_rendered, 0, sizeof(row_rendered));
        vt100_render_all();

        /* Split the command string on spaces */
        std::vector<std::string> tokens;
        std::istringstream iss(cmd);
        std::string token;
        while (iss >> token)
            tokens.push_back(token);

        if (tokens.empty())
        {
            const char *err = "Error: empty command\r\n";
            vt100_process_bytes(err, (int)strlen(err));
            vt100_render_all();
            terminal_active = false;
            return;
        }

        std::string executable = tokens[0];
        std::vector<std::string> args(tokens.begin() + 1, tokens.end());

        if (!start_pty(executable, args))
        {
            const char *err = "Error: openpty/fork failed\r\n";
            vt100_process_bytes(err, (int)strlen(err));
            vt100_render_all();
            terminal_active = false;
            return;
        }

        if (!poll_timer)
            poll_timer = lv_timer_create(UIConsolePage::s_poll_cb, 30, this);
        if (!cursor_timer)
            cursor_timer = lv_timer_create(UIConsolePage::s_cursor_blink_cb, 500, this);
        set_page_title(executable);
    }

private:
    /* ================================================================== */
    /*  VT100 character grid state                                                  */
    /* ================================================================== */
    char vt100_screen[ROWS][COLS] = {};

    /* Cursor */
    int vt100_cur_row = 0;
    int vt100_cur_col = 0;

    /* Current SGR attributes (tracked by parser; rendering remains monochrome) */
    int vt100_cur_attr = 0;   /* ATTR_BOLD=1, ATTR_UNDERLINE=2, ATTR_BLINK=4, ATTR_REVERSE=8 */
    int vt100_cur_fg = 7;     /* ANSI 8colors: 0-7, COLOR_DEFAULT=9 */
    int vt100_cur_bg = 0;

    /* Cursor save/restore (SCP/RCP, DECSC/DECRC) */
    int vt100_saved_row = 0, vt100_saved_col = 0;
    int vt100_saved_attr = 0, vt100_saved_fg = 7, vt100_saved_bg = 0;

    /* DECAWM — automatic wrap */
    bool vt100_auto_wrap = true;

    /* DECTCEM — cursor visibility */
    bool vt100_cursor_visible_flag = true;

    /* DECCKM — application cursor key mode (affects what sequence keyboard sends) */
    bool vt100_decckm = false;

    /* ── Full VT100 state machine ─────────────────────────────────── */
    enum vt_state {
        VT100_ST_NORMAL,    /* print characters */
        VT100_ST_ESC,       /* received ESC */
        VT100_ST_CSI,       /* ESC [ — collect parameters */
        VT100_ST_CSI_QM,    /* ESC [? — DEC private modes */
        VT100_ST_CSI_GT,    /* ESC [> — Secondary DA / xterm extension */
        VT100_ST_OSC,       /* ESC ] — operating system command */
        VT100_ST_DCS,       /* ESC P — device control string */
    };
    vt_state vt100_esc_state = VT100_ST_NORMAL;
    char vt100_esc_buf[64] = {};
    int vt100_esc_len = 0;

    /* CSI parameters */
    static constexpr int VT100_MAX_PARAMS = 16;
    int vt100_params[VT100_MAX_PARAMS] = {};
    int vt100_param_count = 0;
    int vt100_param_val = 0;
    bool vt100_priv_mode = false;
    bool vt100_sec_mode = false;
    bool vt100_skip_until_st = false;

    /* ── PTY ──────────────────────────────────────────────── */
    hal_pty_t pty_handle = NULL;

    lv_timer_t *poll_timer = nullptr;
    lv_timer_t *cursor_timer = nullptr;
    bool vt100_cursor_vis = false;
    bool terminal_active = false;
    bool waiting_key_to_exit = false;

    /* ================================================================== */
    /*  Initialize                                                              */
    /* ================================================================== */
    void console_data_init()
    {
        memset(vt100_screen, ' ', sizeof(vt100_screen));
        memset(row_rendered, 0, sizeof(row_rendered));
        memset(row_labels, 0, sizeof(row_labels));
        memset(vt100_esc_buf, 0, sizeof(vt100_esc_buf));
        memset(vt100_params, 0, sizeof(vt100_params));
    }

    /* ================================================================== */
    /*  UI construction(terminal area mounted under ui_APP_Container)                        */
    /* ================================================================== */
    void creat_console_UI()
    {
        terminal_container = lv_obj_create(ui_APP_Container);
        lv_obj_remove_style_all(terminal_container);
        lv_obj_set_size(terminal_container, TERM_W, TERM_H);
        lv_obj_set_pos(terminal_container, 0, 0);
        lv_obj_set_style_bg_color(terminal_container, lv_color_hex(FIXED_BG), 0);
        lv_obj_set_style_bg_opa(terminal_container, LV_OPA_COVER, 0);
        lv_obj_clear_flag(terminal_container,
                          (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        term_canvas = lv_obj_create(terminal_container);
        lv_obj_set_size(term_canvas, TERM_W, TERM_H);
        lv_obj_set_pos(term_canvas, 0, 0);
        lv_obj_set_style_bg_color(term_canvas, lv_color_hex(FIXED_BG), 0);
        lv_obj_set_style_bg_opa(term_canvas, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(term_canvas, 0, 0);
        lv_obj_set_style_pad_all(term_canvas, 0, 0);
        lv_obj_set_style_radius(term_canvas, 0, 0);
        lv_obj_remove_flag(term_canvas,
                           (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        /* --------------------- line-level label ------------------------ */
        const lv_font_t *mono_font = g_font_mono_12 ? g_font_mono_12 : g_font_cn_12;
        for (int r = 0; r < ROWS; r++)
        {
            lv_obj_t *lbl = lv_label_create(term_canvas);
            lv_obj_set_style_text_font(lbl, mono_font, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(FIXED_FG), 0);
            lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(lbl, 0, 0);
            lv_obj_set_style_text_letter_space(lbl, 0, 0);
            lv_obj_set_style_text_line_space(lbl, 0, 0);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
            lv_obj_set_size(lbl, TERM_W, CHAR_H);
            lv_obj_set_pos(lbl, 0, r * CHAR_H);
            lv_label_set_text(lbl, "");
            row_labels[r] = lbl;
        }
        memset(row_rendered, 0, sizeof(row_rendered));

        /* --------------------- cursor label (inverted block)---------------- */
        cursor_label = lv_label_create(term_canvas);
        lv_obj_set_style_text_font(cursor_label, mono_font, 0);
        lv_obj_set_style_text_color(cursor_label, lv_color_hex(FIXED_BG), 0);
        lv_obj_set_style_bg_color(cursor_label, lv_color_hex(FIXED_FG), 0);
        lv_obj_set_style_bg_opa(cursor_label, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(cursor_label, 0, 0);
        lv_obj_set_style_text_letter_space(cursor_label, 0, 0);
        lv_label_set_long_mode(cursor_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_size(cursor_label, CHAR_W, CHAR_H);
        lv_label_set_text(cursor_label, " ");
        lv_obj_set_pos(cursor_label, 0, 0);
        lv_obj_add_flag(cursor_label, LV_OBJ_FLAG_HIDDEN);
    }

    /* ================================================================== */
    /*  Event binding                                                            */
    /* ================================================================== */
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIConsolePage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIConsolePage *self = static_cast<UIConsolePage *>(lv_event_get_user_data(e));
        if (self)
            self->event_handler(e);
    }

    void event_handler(lv_event_t *e)
    {
        if (lv_event_get_code(e) == LV_EVENT_KEYBOARD)
        {
            struct key_item *elm = (struct key_item *)lv_event_get_param(e);
            printf("[CONSOLE] code=%u state=%s sym=%s utf8_len=%zu pty_active=%d waiting_exit=%d\n",
                   elm->key_code, kbd_state_name(elm->key_state), elm->sym_name,
                   strlen(elm->utf8), (int)terminal_active, (int)waiting_key_to_exit);
            if (waiting_key_to_exit && (elm->key_state == 0))
            {
                if (terminal_sysplause)
                {
                    terminal_sysplause = false;
                }
                else
                {
                    waiting_key_to_exit = false;
                    if (go_back_home)
                        go_back_home();
                }
            }
            else
            {
                if (pty_handle != NULL && terminal_active)
                {
                    if (elm->key_state) {
                        printf("[CONSOLE] -> PTY write (state=%s)\n", kbd_state_name(elm->key_state));
                        write_key_to_pty(elm->key_code, elm->utf8);
                    }
                }
            }
        }
    }

    /* ================================================================== */
    /*  LVGL timer static wrapper                                                 */
    /* ================================================================== */
    static void s_poll_cb(lv_timer_t *t)
    {
        auto self = (UIConsolePage *)lv_timer_get_user_data(t);
        if (self)
            self->vt100_poll_cb(t);
    }

    static void s_cursor_blink_cb(lv_timer_t *t)
    {
        auto self = (UIConsolePage *)lv_timer_get_user_data(t);
        if (self)
            self->vt100_cursor_blink_cb(t);

        static int end_status = 0;
        static std::chrono::time_point<std::chrono::steady_clock> start_time;
        static std::chrono::time_point<std::chrono::steady_clock> end_time;
        pid_t pid_ret;
        if (end_status == 0)
        {
            if (LVGL_HOME_KEY_FLAG)
            {
                end_status = 1;
                start_time = std::chrono::steady_clock::now();
            }
        }
        if (end_status == 1)
        {
            if (LVGL_HOME_KEY_FLAG)
            {
                end_time = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count() >= 5)
                {
                    end_status = 0;
                    printf("[CONSOLE] ESC held 5s -> kill PTY and go back home\n");
                    self->stop_pty();
                    self->terminal_active = false;
                    if (self->go_back_home)
                        self->go_back_home();
                }
            }
            else
            {
                end_status = 0;
            }
        }
    }

    /* ================================================================== */
    /*  VT100 helper functions                                                      */
    /* ================================================================== */

    static int clamp(int v, int lo, int hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    /** defparam: Default parameters or explicit 0 both return the default value.
     *  idx >= param_count -> parameter missing -> dfl
     *  params[idx] == 0 -> explicit 0 or missing -> dfl
     */
    int defparam(int idx, int dfl)
    {
        return (idx >= vt100_param_count || vt100_params[idx] == 0) ? dfl : vt100_params[idx];
    }

    /* ================================================================== */
    /*  Character grid operations                                                        */
    /* ================================================================== */
    void vt100_screen_clear_all()
    {
        for (int r = 0; r < ROWS; r++)
            memset(vt100_screen[r], ' ', COLS);
    }

    void vt100_clear_row_from(int row, int from_col)
    {
        if (row < 0 || row >= ROWS) return;
        if (from_col < 0) from_col = 0;
        if (from_col >= COLS) return;
        memset(&vt100_screen[row][from_col], ' ', COLS - from_col);
    }

    void vt100_scroll_up(int top, int bot, int n)
    {
        if (top < 0) top = 0;
        if (bot >= ROWS) bot = ROWS - 1;
        if (n <= 0 || n > bot - top + 1) return;
        for (int r = top; r <= bot - n; r++)
            memcpy(vt100_screen[r], vt100_screen[r + n], COLS);
        for (int r = bot - n + 1; r <= bot; r++)
            memset(vt100_screen[r], ' ', COLS);
    }

    void vt100_scroll_down(int top, int bot, int n)
    {
        if (top < 0) top = 0;
        if (bot >= ROWS) bot = ROWS - 1;
        if (n <= 0 || n > bot - top + 1) return;
        for (int r = bot; r >= top + n; r--)
            memcpy(vt100_screen[r], vt100_screen[r - n], COLS);
        for (int r = top; r < top + n; r++)
            memset(vt100_screen[r], ' ', COLS);
    }

    void vt100_erase_display(int mode)
    {
        if (mode == 0) {
            vt100_clear_row_from(vt100_cur_row, vt100_cur_col);
            for (int r = vt100_cur_row + 1; r < ROWS; r++)
                vt100_clear_row_from(r, 0);
        } else if (mode == 1) {
            for (int r = 0; r < vt100_cur_row; r++)
                vt100_clear_row_from(r, 0);
            for (int c = 0; c <= vt100_cur_col && c < COLS; c++)
                vt100_screen[vt100_cur_row][c] = ' ';
        } else {
            vt100_screen_clear_all();
        }
    }

    void vt100_erase_line(int mode)
    {
        int start, end;
        if (mode == 0) {
            start = vt100_cur_col; end = COLS - 1;
        } else if (mode == 1) {
            start = 0; end = vt100_cur_col;
        } else {
            start = 0; end = COLS - 1;
        }
        for (int c = start; c <= end && c < COLS; c++)
            vt100_screen[vt100_cur_row][c] = ' ';
    }

    void vt100_insert_lines(int n)
    {
        if (n < 1) n = 1;
        vt100_scroll_down(vt100_cur_row, ROWS - 1, n);
    }

    void vt100_delete_lines(int n)
    {
        if (n < 1) n = 1;
        vt100_scroll_up(vt100_cur_row, ROWS - 1, n);
    }

    void vt100_insert_chars(int n)
    {
        if (n < 1) n = 1;
        for (int i = COLS - 1; i >= vt100_cur_col + n; i--)
            vt100_screen[vt100_cur_row][i] = vt100_screen[vt100_cur_row][i - n];
        for (int i = 0; i < n && vt100_cur_col + i < COLS; i++)
            vt100_screen[vt100_cur_row][vt100_cur_col + i] = ' ';
    }

    void vt100_delete_chars(int n)
    {
        if (n < 1) n = 1;
        for (int i = vt100_cur_col; i < COLS - n; i++)
            vt100_screen[vt100_cur_row][i] = vt100_screen[vt100_cur_row][i + n];
        for (int i = COLS - n; i < COLS; i++)
            vt100_screen[vt100_cur_row][i] = ' ';
    }

    /* ================================================================== */
    /*  Character output (including DECAWM automatic wrap)                                       */
    /* ================================================================== */
    void vt100_put_char(char ch)
    {
        if (ch == '\r')
        {
            vt100_cur_col = 0;
            return;
        }
        if (ch == '\n')
        {
            vt100_cur_col = 0;
            if (++vt100_cur_row >= ROWS)
            {
                vt100_scroll_up(0, ROWS - 1, 1);
                vt100_cur_row = ROWS - 1;
            }
            return;
        }
        if (ch == '\b')
        {
            if (vt100_cur_col > 0)
                vt100_cur_col--;
            return;
        }
        if (ch == '\t')
        {
            int next_tab = (vt100_cur_col / 8 + 1) * 8;
            if (next_tab >= COLS) next_tab = COLS - 1;
            while (vt100_cur_col < next_tab)
                vt100_put_char(' ');
            return;
        }
        /* C0 controls (0x00-0x1F) — ignore */
        if ((unsigned char)ch < 0x20)
            return;
        /* DEL (0x7F) */
        if ((unsigned char)ch == 0x7F)
            return;

        /* ── DECAWM automatic-wrap check ── */
        if (vt100_cur_col >= COLS)
        {
            if (vt100_auto_wrap)
            {
                vt100_cur_col = 0;
                if (++vt100_cur_row >= ROWS)
                {
                    vt100_scroll_up(0, ROWS - 1, 1);
                    vt100_cur_row = ROWS - 1;
                }
            }
            else
            {
                vt100_cur_col = COLS - 1;  /* overwrite the last column */
            }
        }
        vt100_screen[vt100_cur_row][vt100_cur_col++] = ch;
    }

    /* ================================================================== */
    /*  ESC sequence handling (non-CSI)                                               */
    /* ================================================================== */
    void vt100_handle_esc(char c)
    {
        switch (c) {
        case 'D':  /* IND — Index: move cursor down and scroll if needed */
            if (vt100_cur_row == ROWS - 1)
                vt100_scroll_up(0, ROWS - 1, 1);
            else
                vt100_cur_row++;
            break;
        case 'M':  /* RI — Reverse Index: move cursor up */
            if (vt100_cur_row == 0)
                vt100_scroll_down(0, ROWS - 1, 1);
            else
                vt100_cur_row--;
            break;
        case 'E':  /* NEL — Next Line */
            vt100_cur_col = 0;
            if (vt100_cur_row == ROWS - 1)
                vt100_scroll_up(0, ROWS - 1, 1);
            else
                vt100_cur_row++;
            break;
        case 'H':  /* HTS — Horizontal Tab Set (acknowledge) */
            break;
        case '7':  /* DECSC — Save Cursor */
            vt100_saved_row  = vt100_cur_row;
            vt100_saved_col  = vt100_cur_col;
            vt100_saved_attr = vt100_cur_attr;
            vt100_saved_fg   = vt100_cur_fg;
            vt100_saved_bg   = vt100_cur_bg;
            break;
        case '8':  /* DECRC — Restore Cursor */
            vt100_cur_row  = clamp(vt100_saved_row, 0, ROWS - 1);
            vt100_cur_col  = clamp(vt100_saved_col, 0, COLS - 1);
            vt100_cur_attr = vt100_saved_attr;
            vt100_cur_fg   = vt100_saved_fg;
            vt100_cur_bg   = vt100_saved_bg;
            break;
        case 'c':  /* RIS — Reset to Initial State */
            vt100_screen_clear_all();
            vt100_cur_row = 0;
            vt100_cur_col = 0;
            vt100_cur_attr = 0;
            vt100_cur_fg = 7;
            vt100_cur_bg = 0;
            vt100_auto_wrap = true;
            vt100_cursor_visible_flag = true;
            vt100_decckm = false;
            break;
        case '=':  /* DECKPAM — Keypad Application Mode */
        case '>':  /* DECKPNM — Keypad Numeric Mode */
            break;
        case '(':  /* SCS — Designate G0 charset */
        case ')':  /* SCS — Designate G1 charset */
        case '*':  /* SCS — Designate G2 charset */
        case '+':  /* SCS — Designate G3 charset */
        case '#':  /* DEC line attributes */
            /* consume next byte; we ignore */
            vt100_esc_state = VT100_ST_NORMAL;
            break;
        default:
            break;
        }
    }

    /* ================================================================== */
    /*  CSI sequence dispatch (complete VT100 instruction set)                                    */
    /* ================================================================== */
    void vt100_handle_csi(char final)
    {
        /* ── DEC private modes (ESC [? ... h/l) ── */
        if (vt100_priv_mode) {
            switch (final) {
            case 'h': /* DECSET */
                if (vt100_params[0] == 1)  { vt100_decckm = true; fprintf(stderr, "[VT100-DBG] DECCKM ON (app cursor keys)\n"); }  /* DECCKM */
                if (vt100_params[0] == 5)  {}  /* DECSCNM — reverse video */
                if (vt100_params[0] == 6)  {}  /* DECOM — origin mode */
                if (vt100_params[0] == 7)  { vt100_auto_wrap = true; }  /* DECAWM */
                if (vt100_params[0] == 25) { vt100_cursor_visible_flag = true; }  /* DECTCEM */
                if (vt100_params[0] == 1049) {} /* alt screen */
                break;
            case 'l': /* DECRST */
                if (vt100_params[0] == 1)  { vt100_decckm = false; }
                if (vt100_params[0] == 5)  {}
                if (vt100_params[0] == 6)  {}
                if (vt100_params[0] == 7)  { vt100_auto_wrap = false; }  /* DECAWM off */
                if (vt100_params[0] == 25) { vt100_cursor_visible_flag = false; } /* DECTCEM hide */
                if (vt100_params[0] == 1049) {}
                break;
            default:
                break;
            }
            return;
        }

        /* ── Secondary DA / xterm query (ESC [> ... c) ── */
        if (vt100_sec_mode) {
            fprintf(stderr, "[VT100-DBG] handle_csi SEC_MODE final='%c'(0x%02X) param[0]=%d\n",
                    final, (unsigned char)final, vt100_params[0]);
            switch (final) {
            case 'c': /* Secondary Device Attributes */
                /* Reply: VT100 (type 0), firmware v10, no options */
                fprintf(stderr, "[VT100-DBG] SDA reply: \\033[>0;10;0c\n");
                if (pty_handle != NULL)
                    hal_pty_write(pty_handle, "\033[>0;10;0c", 10);
                break;
            case 'm': /* xterm set-modifyOtherKeys — ignore */
                fprintf(stderr, "[VT100-DBG] SDA: set-modifyOtherKeys ignored\n");
                break;
            default:
                fprintf(stderr, "[VT100-DBG] SDA: unhandled final byte\n");
                break;
            }
            return;
        }

        switch (final) {
        /* ── Cursor movement ───────────────────── */
        case 'A': /* CUU */
            vt100_cur_row -= defparam(0, 1);
            if (vt100_cur_row < 0) vt100_cur_row = 0;
            break;
        case 'B': /* CUD */
            vt100_cur_row += defparam(0, 1);
            if (vt100_cur_row >= ROWS) vt100_cur_row = ROWS - 1;
            break;
        case 'C': /* CUF */
            vt100_cur_col += defparam(0, 1);
            if (vt100_cur_col >= COLS) vt100_cur_col = COLS - 1;
            break;
        case 'D': /* CUB */
            vt100_cur_col -= defparam(0, 1);
            if (vt100_cur_col < 0) vt100_cur_col = 0;
            break;
        case 'H': /* CUP — Cursor Position */
        case 'f': /* HVP — Horizontal Vertical Position */
            vt100_cur_row = defparam(0, 1) - 1;  /* 1-based -> 0-based */
            vt100_cur_col = defparam(1, 1) - 1;
            if (vt100_cur_row < 0) vt100_cur_row = 0;
            if (vt100_cur_row >= ROWS) vt100_cur_row = ROWS - 1;
            if (vt100_cur_col < 0) vt100_cur_col = 0;
            if (vt100_cur_col >= COLS) vt100_cur_col = COLS - 1;
            break;
        case 'G': /* CHA — Cursor Horizontal Absolute */
            vt100_cur_col = defparam(0, 1) - 1;
            if (vt100_cur_col < 0) vt100_cur_col = 0;
            if (vt100_cur_col >= COLS) vt100_cur_col = COLS - 1;
            break;
        case 'd': /* VPA — Vertical Position Absolute */
            vt100_cur_row = defparam(0, 1) - 1;
            if (vt100_cur_row < 0) vt100_cur_row = 0;
            if (vt100_cur_row >= ROWS) vt100_cur_row = ROWS - 1;
            break;
        case 's': /* SCP — Save Cursor Position (ANSI) */
            vt100_saved_row  = vt100_cur_row;
            vt100_saved_col  = vt100_cur_col;
            vt100_saved_attr = vt100_cur_attr;
            vt100_saved_fg   = vt100_cur_fg;
            vt100_saved_bg   = vt100_cur_bg;
            break;
        case 'u': /* RCP — Restore Cursor Position (ANSI) */
            vt100_cur_row  = clamp(vt100_saved_row, 0, ROWS - 1);
            vt100_cur_col  = clamp(vt100_saved_col, 0, COLS - 1);
            vt100_cur_attr = vt100_saved_attr;
            vt100_cur_fg   = vt100_saved_fg;
            vt100_cur_bg   = vt100_saved_bg;
            break;

        /* ── Erase ───────────────────────── */
        case 'J': /* ED — Erase Display */
            vt100_erase_display(defparam(0, 0));
            break;
        case 'K': /* EL — Erase Line */
            vt100_erase_line(defparam(0, 0));
            break;

        /* ── Insert / delete ────────────────── */
        case 'L': /* IL — Insert Lines */
            vt100_insert_lines(defparam(0, 1));
            break;
        case 'M': /* DL — Delete Lines (note: CSI M ≠ ESC M) */
            vt100_delete_lines(defparam(0, 1));
            break;
        case '@': /* ICH — Insert Characters */
            vt100_insert_chars(defparam(0, 1));
            break;
        case 'P': /* DCH — Delete Characters */
            vt100_delete_chars(defparam(0, 1));
            break;

        /* ── SGR — Select Graphic Rendition ── */
        case 'm':
            for (int i = 0; i < vt100_param_count; i++) {
                int val = vt100_params[i];
                if (val == 0) {
                    vt100_cur_attr = 0;
                    vt100_cur_fg = 7;   /* white (rendered as green) */
                    vt100_cur_bg = 0;
                } else if (val == 1) {
                    vt100_cur_attr |= 1;   /* ATTR_BOLD */
                } else if (val == 4) {
                    vt100_cur_attr |= 2;   /* ATTR_UNDERLINE */
                } else if (val == 5) {
                    vt100_cur_attr |= 4;   /* ATTR_BLINK */
                } else if (val == 7) {
                    vt100_cur_attr |= 8;   /* ATTR_REVERSE */
                } else if (val == 22) {
                    vt100_cur_attr &= ~1;
                } else if (val == 24) {
                    vt100_cur_attr &= ~2;
                } else if (val == 25) {
                    vt100_cur_attr &= ~4;
                } else if (val == 27) {
                    vt100_cur_attr &= ~8;
                } else if (30 <= val && val <= 37) {
                    vt100_cur_fg = val - 30;
                } else if (40 <= val && val <= 47) {
                    vt100_cur_bg = val - 40;
                } else if (val == 39) {
                    vt100_cur_fg = 7;
                } else if (val == 49) {
                    vt100_cur_bg = 0;
                }
            }
            break;

        /* ── scrolling region ────────────────────── */
        case 'r': /* DECSTBM — Set Top and Bottom Margins */
            /* acknowledged but not fully implemented */
            break;

        /* ── Device control ────────────────────── */
        case 'c': /* DA — Device Attributes: reply with \033[?1;0c (VT100) */
            if (pty_handle != NULL) {
                const char *reply = "\033[?1;0c";
                hal_pty_write(pty_handle, reply, strlen(reply));
            }
            break;
        case 'n': /* DSR — Device Status Report */
            fprintf(stderr, "[VT100-DBG] DSR query param[0]=%d\n", vt100_params[0]);
            if (vt100_params[0] == 5) {
                fprintf(stderr, "[VT100-DBG] DSR 5: reply \\033[0n (OK)\n");
                if (pty_handle != NULL) hal_pty_write(pty_handle, "\033[0n", 4);
            } else if (vt100_params[0] == 6) {
                /* Cursor Position Report */
                char buf[32];
                int len = snprintf(buf, sizeof(buf), "\033[%d;%dR",
                                   vt100_cur_row + 1, vt100_cur_col + 1);
                fprintf(stderr, "[VT100-DBG] DSR 6: cursor=(%d,%d) reply=%s\n",
                        vt100_cur_row + 1, vt100_cur_col + 1, buf);
                if (pty_handle != NULL) hal_pty_write(pty_handle, buf, len);
            }
            break;

        /* ── Mode settings ────────────────────── */
        case 'h': /* SM — Set Mode (non-private) */
        case 'l': /* RM — Reset Mode (non-private) */
            break;

        default:
            break;
        }
    }

    /* ================================================================== */
    /*  Main byte-stream parser (full VT100 state machine, ported from src/vt100.c)                  */
    /* ================================================================== */
    void vt100_process_bytes(const char *data, int len)
    {
        /* ── DEBUG: dump raw PTY data ── */
        if (len > 0 && len < 256) {
            fprintf(stderr, "[VT100-DBG] process_bytes len=%d hex=", len);
            for (int di = 0; di < len && di < 80; di++)
                fprintf(stderr, "%02X ", (unsigned char)data[di]);
            fprintf(stderr, "\n");
        }
        for (int i = 0; i < len; i++)
        {
            unsigned char c = (unsigned char)data[i];

            /* ── string terminator detection ───────────── */
            /* skip_until_st during vt100_esc_state remains unchanged.
             * OSC: BEL(0x07), ST(0x9C), or a bare backslash can terminate.
             * DCS: only BEL or ST terminates; backslash is not a DCS terminator
             *      (DCS uses a two-byte ESC \ sequence).
             * Therefore the vt100_esc_state == VT100_ST_OSC guard is required. */
            if (vt100_skip_until_st) {
                if (c == 0x07 || c == 0x9C ||
                    (c == '\\' && vt100_esc_state == VT100_ST_OSC)) {
                    vt100_skip_until_st = false;
                    vt100_esc_state = VT100_ST_NORMAL;
                }
                continue;
            }

            switch (vt100_esc_state) {

            case VT100_ST_NORMAL:
                if (c == 0x1B) {
                    vt100_esc_state = VT100_ST_ESC;
                    vt100_param_count = 0;
                    vt100_param_val = 0;
                    vt100_priv_mode = false;
                    vt100_sec_mode = false;
                    memset(vt100_params, 0, sizeof(vt100_params));
                } else {
                    vt100_put_char((char)c);
                }
                break;

            case VT100_ST_ESC:
                if (c == '[') {
                    vt100_esc_state = VT100_ST_CSI;
                } else if (c == ']') {
                    vt100_esc_state = VT100_ST_OSC;
                    vt100_skip_until_st = true;
                } else if (c == 'P') {
                    vt100_esc_state = VT100_ST_DCS;
                    vt100_skip_until_st = true;
                } else {
                    vt100_handle_esc((char)c);
                    vt100_esc_state = VT100_ST_NORMAL;
                }
                break;

            case VT100_ST_CSI:
                if (c == '?') {
                    vt100_priv_mode = true;
                    vt100_esc_state = VT100_ST_CSI_QM;
                } else if (c == '>') {
                    fprintf(stderr, "[VT100-DBG] CSI '>' prefix detected! sec_mode=1\n");
                    vt100_sec_mode = true;
                    vt100_esc_state = VT100_ST_CSI_GT;
                } else if (c >= '0' && c <= '9') {
                    vt100_param_val = vt100_param_val * 10 + (c - '0');
                } else if (c == ';') {
                    if (vt100_param_count < VT100_MAX_PARAMS)
                        vt100_params[vt100_param_count++] = vt100_param_val;
                    vt100_param_val = 0;
                } else if (c >= 0x20 && c <= 0x2F) {
                    /* Intermediate byte — ignored */
                } else {
                    /* Final byte (0x40-0x7E) */
                    if (vt100_param_count < VT100_MAX_PARAMS)
                        vt100_params[vt100_param_count++] = vt100_param_val;
                    vt100_handle_csi((char)c);
                    vt100_esc_state = VT100_ST_NORMAL;
                }
                break;

            case VT100_ST_CSI_QM:
                if (c >= '0' && c <= '9') {
                    vt100_param_val = vt100_param_val * 10 + (c - '0');
                } else if (c == ';') {
                    if (vt100_param_count < VT100_MAX_PARAMS)
                        vt100_params[vt100_param_count++] = vt100_param_val;
                    vt100_param_val = 0;
                } else if (c >= 0x20 && c <= 0x2F) {
                    /* intermediate */
                } else {
                    /* Final byte */
                    if (vt100_param_count < VT100_MAX_PARAMS)
                        vt100_params[vt100_param_count++] = vt100_param_val;
                    vt100_handle_csi((char)c);
                    vt100_esc_state = VT100_ST_NORMAL;
                }
                break;

            case VT100_ST_CSI_GT:
                /* ESC [> — Secondary DA / xterm extensions */
                if (c >= '0' && c <= '9') {
                    vt100_param_val = vt100_param_val * 10 + (c - '0');
                } else if (c == ';') {
                    if (vt100_param_count < VT100_MAX_PARAMS)
                        vt100_params[vt100_param_count++] = vt100_param_val;
                    vt100_param_val = 0;
                } else if (c >= 0x20 && c <= 0x2F) {
                    /* intermediate */
                } else {
                    /* Final byte */
                    if (vt100_param_count < VT100_MAX_PARAMS)
                        vt100_params[vt100_param_count++] = vt100_param_val;
                    vt100_handle_csi((char)c);
                    vt100_esc_state = VT100_ST_NORMAL;
                }
                break;

            case VT100_ST_OSC:
            case VT100_ST_DCS:
                /* Handled by skip_until_st at top of loop */
                vt100_esc_state = VT100_ST_NORMAL;
                break;

            default:
                vt100_esc_state = VT100_ST_NORMAL;
                break;
            }
        }
    }

    /* ================================================================== */
    /*  Line-level rendering: call lv_label_set_text only when that row changes                */
    /* ================================================================== */
    static inline char sanitize_ch(char ch)
    {
        unsigned char c = (unsigned char)ch;
        if (c < 32 || c > 126)
            return ' ';
        return (char)c;
    }

    void vt100_render_row(int r)
    {
        if (r < 0 || r >= ROWS)
            return;

        char buf[COLS + 1];
        for (int c = 0; c < COLS; c++)
            buf[c] = sanitize_ch(vt100_screen[r][c]);
        buf[COLS] = '\0';

        if (memcmp(buf, row_rendered[r], COLS + 1) == 0)
            return; /* row unchanged, skip */

        memcpy(row_rendered[r], buf, COLS + 1);
        lv_label_set_text(row_labels[r], buf);
    }

    void vt100_render_all()
    {
        for (int r = 0; r < ROWS; r++)
            vt100_render_row(r);
        update_cursor_position_only();
    }

    /** Only update the cursor label position and character; do not change show/hide state */
    void update_cursor_position_only()
    {
        if (!cursor_label)
            return;
        int row = vt100_cur_row;
        int col = vt100_cur_col;
        if (row < 0) row = 0;
        if (row >= ROWS) row = ROWS - 1;
        if (col < 0) col = 0;
        if (col >= COLS) col = COLS - 1;

        char under = sanitize_ch(vt100_screen[row][col]);
        char s[2] = {under == ' ' ? ' ' : under, '\0'};
        const char *old = lv_label_get_text(cursor_label);
        if (!old || old[0] != s[0] || old[1] != s[1])
            lv_label_set_text(cursor_label, s);

        lv_obj_set_pos(cursor_label, col * CHAR_W, row * CHAR_H);
    }

    void show_cursor(bool show)
    {
        if (!cursor_label)
            return;
        if (show)
        {
            if (lv_obj_has_flag(cursor_label, LV_OBJ_FLAG_HIDDEN))
                lv_obj_clear_flag(cursor_label, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            if (!lv_obj_has_flag(cursor_label, LV_OBJ_FLAG_HIDDEN))
                lv_obj_add_flag(cursor_label, LV_OBJ_FLAG_HIDDEN);
        }
        vt100_cursor_vis = show;
    }

    /* ================================================================== */
    /*  PTY management                                                            */
    /* ================================================================== */
    bool start_pty(const std::string &cmd, const std::vector<std::string> &args = {})
    {
        std::vector<const char *> argv;
        argv.push_back(cmd.c_str());
        for (const auto &a : args)
            argv.push_back(a.c_str());
        argv.push_back(nullptr);
        pty_handle = hal_pty_open(cmd.c_str(), argv.data(), COLS, ROWS);
        return pty_handle != NULL;
    }

    void stop_pty()
    {
        if (pty_handle) {
            hal_pty_close(pty_handle);
            pty_handle = NULL;
        }
    }

    /* ================================================================== */
    /*  Timer callback                                                          */
    /* ================================================================== */
    void vt100_poll_cb(lv_timer_t *t)
    {
        (void)t;
        if (pty_handle == NULL || !terminal_active)
            return;

        char buf[1024];
        int n;
        bool changed = false;

        while ((n = hal_pty_read(pty_handle, buf, sizeof(buf))) > 0)
        {
            vt100_process_bytes(buf, n);
            changed = true;
        }

        if (changed)
            vt100_render_all();

        bool child_exited = false;
        if (n < 0)
        {
            child_exited = true;
        }
        else if (pty_handle != NULL)
        {
            int status = 0;
            if (hal_pty_check_child(pty_handle, &status) == 1)
                child_exited = true;
        }

        if (child_exited)
        {
            terminal_active = false;
            const char *hint = "\r\n-- Press any key to exit --";
            vt100_process_bytes(hint, (int)strlen(hint));
            vt100_render_all();
            waiting_key_to_exit = true;
            hal_pty_close(pty_handle);
            pty_handle = NULL;
        }
    }

    void vt100_cursor_blink_cb(lv_timer_t *t)
    {
        (void)t;
        if (!cursor_label)
            return;

        update_cursor_position_only();

        if (!terminal_active)
        {
            show_cursor(false);
            return;
        }

        /* Honor the DECTCEM cursor visibility setting */
        if (!vt100_cursor_visible_flag)
        {
            show_cursor(false);
            return;
        }

        show_cursor(!vt100_cursor_vis);
    }

    /* ================================================================== */
    /*  Key handling                                                            */
    /* ================================================================== */

    /**
     * Convert physical keyboard input (evdev keycode + utf8) to terminal byte sequences and write them to the PTY.
     * evdev keycode reference (linux/input-event-codes.h):
     *   KEY_ESC=1  KEY_BACKSPACE=14  KEY_ENTER=28
     *   KEY_UP=103 KEY_LEFT=105 KEY_RIGHT=106 KEY_DOWN=108
     */
    void write_key_to_pty(uint32_t evdev_key, const char *utf8_str)
    {
        if (!terminal_active || pty_handle == NULL)
            return;
        char buf[8];
        int len = 0;

        switch (evdev_key)
        {
        case 28:
            buf[0] = '\r'; len = 1; break;  /* KEY_ENTER      */
        case 14:
            buf[0] = 0x7f; len = 1; break;  /* KEY_BACKSPACE  */
        case 1:
            buf[0] = 0x1b; len = 1; break;  /* KEY_ESC        */
        case 103:
            /* KEY_UP: normal=\033[A, application=\033OA */
            if (vt100_decckm) {
                buf[0]=0x1b; buf[1]='O'; buf[2]='A'; len=3;
            } else {
                buf[0]=0x1b; buf[1]='['; buf[2]='A'; len=3;
            }
            break;
        case 108:
            if (vt100_decckm) {
                buf[0]=0x1b; buf[1]='O'; buf[2]='B'; len=3;
            } else {
                buf[0]=0x1b; buf[1]='['; buf[2]='B'; len=3;
            }
            break;
        case 106:
            if (vt100_decckm) {
                buf[0]=0x1b; buf[1]='O'; buf[2]='C'; len=3;
            } else {
                buf[0]=0x1b; buf[1]='['; buf[2]='C'; len=3;
            }
            break;
        case 105:
            if (vt100_decckm) {
                buf[0]=0x1b; buf[1]='O'; buf[2]='D'; len=3;
            } else {
                buf[0]=0x1b; buf[1]='['; buf[2]='D'; len=3;
            }
            break;
        default:
            len = (int)strlen(utf8_str);
            if (len > 0 && len <= (int)sizeof(buf))
                memcpy(buf, utf8_str, (size_t)len);
            else
                len = 0;
            break;
        }

        if (len > 0) {
            fprintf(stderr, "[VT100-DBG] write_key evdev=%u decckm=%d len=%d hex=",
                    evdev_key, vt100_decckm, len);
            for (int ki = 0; ki < len; ki++)
                fprintf(stderr, "%02X ", (unsigned char)buf[ki]);
            fprintf(stderr, "\n");
            hal_pty_write(pty_handle, buf, (size_t)len);
        }
    }

};
