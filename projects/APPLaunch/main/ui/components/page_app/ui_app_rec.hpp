#pragma once
#include "../ui_app_page.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include "hal/hal_process.h"
#include "compat/input_keys.h"

// ============================================================
//  Audio Recorder  UIRecPage
//  Screen: 320 x 170  (top bar 20px, ui_APP_Container 320x150)
//
//  States: IDLE -> RECORDING -> IDLE, IDLE -> PLAYING -> IDLE
//
//  Keys:
//    R          Start recording (arecord)
//    S          Stop recording / playback
//    P          Play last recording (aplay)
//    D          Delete selected recording
//    UP/DOWN    Navigate recording list
//    ESC        Stop active process, go back home
// ============================================================
class UIRecPage : public app_base
{
    enum class RecState { IDLE, RECORDING, PLAYING };

public:
    UIRecPage() : app_base()
    {
        set_page_title("REC");
        creat_UI();
        event_handler_init();
    }

    ~UIRecPage()
    {
        stop_process();
        if (elapsed_timer_) lv_timer_delete(elapsed_timer_);
        if (blink_timer_)   lv_timer_delete(blink_timer_);
    }

private:
    // ==================== data members ====================
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    std::vector<std::string> recordings_;
    int              selected_idx_  = 0;
    int              rec_counter_   = 0;
    RecState         state_         = RecState::IDLE;
    hal_pid_t        active_pid_    = -1;
    std::string      current_file_;
    int              elapsed_sec_   = 0;
    lv_timer_t      *elapsed_timer_ = nullptr;
    lv_timer_t      *blink_timer_   = nullptr;
    bool             blink_visible_ = true;

    // ==================== UI construction ====================
    void creat_UI()
    {
        // background
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, 320, 150);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        // title bar
        lv_obj_t *title_bar = lv_obj_create(bg);
        lv_obj_set_size(title_bar, 320, 22);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title_bar, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_title = lv_label_create(title_bar);
        lv_label_set_text(lbl_title, "Recorder");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, "R:Rec  S:Stop  P:Play  ESC:Back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // content area
        lv_obj_t *content = lv_obj_create(bg);
        lv_obj_set_size(content, 320, 128);
        lv_obj_set_pos(content, 0, 22);
        lv_obj_set_style_radius(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["content"] = content;

        // red dot (for blinking during recording)
        lv_obj_t *red_dot = lv_obj_create(content);
        lv_obj_set_size(red_dot, 10, 10);
        lv_obj_set_pos(red_dot, 10, 10);
        lv_obj_set_style_radius(red_dot, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(red_dot, lv_color_hex(0xE74C3C), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(red_dot, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(red_dot, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(red_dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(red_dot, LV_OBJ_FLAG_HIDDEN);
        ui_obj_["red_dot"] = red_dot;

        // status label (READY / RECORDING / PLAYING)
        lv_obj_t *lbl_status = lv_label_create(content);
        lv_label_set_text(lbl_status, "READY");
        lv_obj_set_pos(lbl_status, 26, 4);
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xE6EDF3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_status"] = lbl_status;

        // elapsed time label
        lv_obj_t *lbl_time = lv_label_create(content);
        lv_label_set_text(lbl_time, "00:00");
        lv_obj_set_pos(lbl_time, 120, 4);
        lv_obj_set_style_text_color(lbl_time, lv_color_hex(0xE6EDF3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_time"] = lbl_time;

        // file info label
        lv_obj_t *lbl_file = lv_label_create(content);
        lv_label_set_text(lbl_file, "File: (none)");
        lv_obj_set_pos(lbl_file, 10, 24);
        lv_obj_set_width(lbl_file, 300);
        lv_label_set_long_mode(lbl_file, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_color(lbl_file, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_file, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_file"] = lbl_file;

        // separator line
        lv_obj_t *sep = lv_obj_create(content);
        lv_obj_set_size(sep, 300, 1);
        lv_obj_set_pos(sep, 10, 38);
        lv_obj_set_style_radius(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(sep, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

        // recordings list label
        lv_obj_t *lbl_list_title = lv_label_create(content);
        lv_label_set_text(lbl_list_title, "Recordings:");
        lv_obj_set_pos(lbl_list_title, 10, 42);
        lv_obj_set_style_text_color(lbl_list_title, lv_color_hex(0xE6EDF3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_list_title, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        // recordings list container (shows up to 5 items)
        lv_obj_t *list_cont = lv_obj_create(content);
        lv_obj_set_size(list_cont, 300, 70);
        lv_obj_set_pos(list_cont, 10, 56);
        lv_obj_set_style_radius(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["list_cont"] = list_cont;

        build_rec_list();
    }

    // ==================== recording list rows ====================
    void build_rec_list()
    {
        lv_obj_t *list_cont = ui_obj_["list_cont"];
        lv_obj_clean(list_cont);

        if (recordings_.empty())
        {
            lv_obj_t *lbl = lv_label_create(list_cont);
            lv_label_set_text(lbl, "(no recordings yet)");
            lv_obj_set_pos(lbl, 0, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
            return;
        }

        static constexpr int ROW_H = 14;
        static constexpr int MAX_VISIBLE = 5;

        int count = (int)recordings_.size();
        int visible = (count < MAX_VISIBLE) ? count : MAX_VISIBLE;
        int offset = selected_idx_ - visible / 2;
        if (offset < 0) offset = 0;
        if (offset > count - visible) offset = count - visible;
        if (offset < 0) offset = 0;

        for (int vi = 0; vi < visible; ++vi)
        {
            int ri = vi + offset;
            bool is_sel = (ri == selected_idx_);

            lv_obj_t *lbl = lv_label_create(list_cont);
            // extract just the filename for display
            const std::string &path = recordings_[ri];
            std::string display = path;
            size_t slash = path.rfind('/');
            if (slash != std::string::npos)
                display = path.substr(slash + 1);

            char buf[64];
            snprintf(buf, sizeof(buf), "%s %s", is_sel ? ">" : " ", display.c_str());
            lv_label_set_text(lbl, buf);
            lv_obj_set_pos(lbl, 0, vi * ROW_H);
            lv_obj_set_width(lbl, 300);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_color(lbl,
                is_sel ? lv_color_hex(0x1F6FEB) : lv_color_hex(0xE6EDF3),
                LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    // ==================== state update UI ====================
    void update_status_ui()
    {
        lv_obj_t *lbl_status = ui_obj_["lbl_status"];
        lv_obj_t *red_dot    = ui_obj_["red_dot"];

        switch (state_)
        {
        case RecState::IDLE:
            lv_label_set_text(lbl_status, "READY");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xE6EDF3), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_flag(red_dot, LV_OBJ_FLAG_HIDDEN);
            stop_blink_timer();
            break;
        case RecState::RECORDING:
            lv_label_set_text(lbl_status, "RECORDING");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xE74C3C), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(red_dot, LV_OBJ_FLAG_HIDDEN);
            start_blink_timer();
            break;
        case RecState::PLAYING:
            lv_label_set_text(lbl_status, "PLAYING");
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x2ECC71), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_flag(red_dot, LV_OBJ_FLAG_HIDDEN);
            stop_blink_timer();
            break;
        }
    }

    void update_time_label()
    {
        char buf[16];
        int mins = elapsed_sec_ / 60;
        int secs = elapsed_sec_ % 60;
        snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        lv_label_set_text(ui_obj_["lbl_time"], buf);
    }

    void update_file_label()
    {
        if (current_file_.empty())
            lv_label_set_text(ui_obj_["lbl_file"], "File: (none)");
        else
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "File: %s", current_file_.c_str());
            lv_label_set_text(ui_obj_["lbl_file"], buf);
        }
    }

    // ==================== timers ====================
    // elapsed time timer (1 second interval)
    static void elapsed_timer_cb(lv_timer_t *t)
    {
        UIRecPage *self = static_cast<UIRecPage *>(lv_timer_get_user_data(t));
        if (self) self->on_elapsed_tick();
    }

    void on_elapsed_tick()
    {
        if (state_ == RecState::RECORDING || state_ == RecState::PLAYING)
        {
            elapsed_sec_++;
            update_time_label();
        }
    }

    void start_elapsed_timer()
    {
        elapsed_sec_ = 0;
        update_time_label();
        if (!elapsed_timer_)
            elapsed_timer_ = lv_timer_create(elapsed_timer_cb, 1000, this);
        else
            lv_timer_reset(elapsed_timer_);
    }

    void stop_elapsed_timer()
    {
        if (elapsed_timer_)
        {
            lv_timer_delete(elapsed_timer_);
            elapsed_timer_ = nullptr;
        }
    }

    // blink timer (500ms interval for red dot)
    static void blink_timer_cb(lv_timer_t *t)
    {
        UIRecPage *self = static_cast<UIRecPage *>(lv_timer_get_user_data(t));
        if (self) self->on_blink_tick();
    }

    void on_blink_tick()
    {
        lv_obj_t *red_dot = ui_obj_["red_dot"];
        blink_visible_ = !blink_visible_;
        if (blink_visible_)
            lv_obj_clear_flag(red_dot, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(red_dot, LV_OBJ_FLAG_HIDDEN);
    }

    void start_blink_timer()
    {
        blink_visible_ = true;
        if (!blink_timer_)
            blink_timer_ = lv_timer_create(blink_timer_cb, 500, this);
        else
            lv_timer_reset(blink_timer_);
    }

    void stop_blink_timer()
    {
        if (blink_timer_)
        {
            lv_timer_delete(blink_timer_);
            blink_timer_ = nullptr;
        }
        blink_visible_ = true;
    }

    // ==================== recording actions ====================
    void start_recording()
    {
        if (state_ != RecState::IDLE) return;

        rec_counter_++;
        char fname[64];
        snprintf(fname, sizeof(fname), "/tmp/rec_%03d.wav", rec_counter_);
        current_file_ = fname;

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "arecord -f cd -t wav %s", fname);
        active_pid_ = hal_process_spawn(cmd);

        state_ = RecState::RECORDING;
        start_elapsed_timer();
        update_status_ui();
        update_file_label();
        printf("[Rec] Start recording: %s  pid=%d\n", fname, active_pid_);
    }

    void stop_process()
    {
        if (active_pid_ > 0)
        {
            hal_process_stop(active_pid_);
            active_pid_ = -1;
        }
    }

    void stop_action()
    {
        if (state_ == RecState::RECORDING)
        {
            stop_process();
            // add the recording to the list
            recordings_.push_back(current_file_);
            selected_idx_ = (int)recordings_.size() - 1;
            build_rec_list();
            printf("[Rec] Stopped recording: %s\n", current_file_.c_str());
        }
        else if (state_ == RecState::PLAYING)
        {
            stop_process();
            printf("[Rec] Stopped playback\n");
        }
        state_ = RecState::IDLE;
        stop_elapsed_timer();
        update_status_ui();
    }

    void play_selected()
    {
        if (state_ != RecState::IDLE) return;
        if (recordings_.empty()) return;

        const std::string &file = recordings_[selected_idx_];
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "aplay %s", file.c_str());
        active_pid_ = hal_process_spawn(cmd);

        current_file_ = file;
        state_ = RecState::PLAYING;
        start_elapsed_timer();
        update_status_ui();
        update_file_label();
        printf("[Rec] Playing: %s  pid=%d\n", file.c_str(), active_pid_);
    }

    void delete_selected()
    {
        if (recordings_.empty()) return;
        if (state_ != RecState::IDLE) return;

        const std::string &file = recordings_[selected_idx_];
        ::unlink(file.c_str());
        printf("[Rec] Deleted: %s\n", file.c_str());

        recordings_.erase(recordings_.begin() + selected_idx_);
        if (selected_idx_ >= (int)recordings_.size() && selected_idx_ > 0)
            selected_idx_--;
        build_rec_list();
    }

    // ==================== event handling ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIRecPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIRecPage *self = static_cast<UIRecPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }

    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e))
        {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            handle_key(key);
        }
    }

    void handle_key(uint32_t key)
    {
        int count = (int)recordings_.size();
        switch (key)
        {
        case KEY_R:
            start_recording();
            break;
        case KEY_S:
            stop_action();
            break;
        case KEY_P:
            play_selected();
            break;
        case KEY_D:
            delete_selected();
            break;
        case KEY_UP:
            if (count > 0 && selected_idx_ > 0)
            {
                selected_idx_--;
                build_rec_list();
            }
            break;
        case KEY_DOWN:
            if (count > 0 && selected_idx_ < count - 1)
            {
                selected_idx_++;
                build_rec_list();
            }
            break;
        case KEY_ESC:
            stop_process();
            if (state_ != RecState::IDLE)
            {
                state_ = RecState::IDLE;
                stop_elapsed_timer();
                update_status_ui();
            }
            if (go_back_home) go_back_home();
            break;
        default:
            break;
        }
    }
};
