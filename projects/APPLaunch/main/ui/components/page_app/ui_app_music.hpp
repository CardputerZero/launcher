#pragma once

#include "../ui_app_page.hpp"
#include "compat/input_keys.h"

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <atomic>

#include "miniaudio.h"

// ============================================================
//  Music player UIMusicPage
//  Screen resolution: 320 x 170  top bar20px, ui_APP_Container 320x150
//
//  Features:
//    1. Select a folder
//    2. Scan mp3 / wav files in the current folder
//    3. Show the playlist
//    4. Play via miniaudio + PulseAudio
//
//  View state:
//    VIEW_MAIN       — main playback screen
//    VIEW_FOLDER_SEL — 'i'(Tab) directory browser popup
//    VIEW_PLAYLIST   — 'p' playlist popup
//
//  Key mapping:
//    LV_KEY_UP    -> play / move up in the list
//    LV_KEY_DOWN  -> pause / move down in the list
//    LV_KEY_LEFT  -> previous track / go to parent directory
//    LV_KEY_RIGHT -> next track / enter subdirectory
//    'i'(Tab)     -> open directory browser
//    'p'          -> open playlist
//    LV_KEY_ENTER -> directory browser: load audio from current directory / playlist: play selected track
//    LV_KEY_ESC   -> return to main screen / exit app
// ============================================================

class UIMusicPage : public app_base
{
    enum class PlayState
    {
        STOPPED,
        PLAYING,
        PAUSED
    };

    enum class ViewState
    {
        MAIN,
        FOLDER_SEL,
        PLAYLIST
    };

public:
    UIMusicPage() : app_base()
    {
        set_page_title("MUSIC");

        creat_UI();
        init_audio();
        event_handler_init();
        update_main_ui();
    }

    ~UIMusicPage()
    {
        if (audio_timer_)
        {
            lv_timer_del(audio_timer_);
            audio_timer_ = nullptr;
        }

        stop_playback();
        uninit_audio();
    }

    // ==================== Public API ====================

    void prev_track()
    {
        if (playlist_.empty())
            return;

        bool was_playing = play_state_ == PlayState::PLAYING;

        if (current_track_ > 0)
            --current_track_;
        else
            current_track_ = static_cast<int>(playlist_.size()) - 1;

        if (was_playing)
        {
            if (start_playback())
                play_state_ = PlayState::PLAYING;
            else
                play_state_ = PlayState::STOPPED;
        }

        update_main_ui();
    }

    void next_track()
    {
        if (playlist_.empty())
            return;

        bool was_playing = play_state_ == PlayState::PLAYING;

        current_track_ = (current_track_ + 1) % static_cast<int>(playlist_.size());

        if (was_playing)
        {
            if (start_playback())
                play_state_ = PlayState::PLAYING;
            else
                play_state_ = PlayState::STOPPED;
        }

        update_main_ui();
    }

    void play()
    {
        if (playlist_.empty())
            return;

        if (!audio_ready_)
        {
            printf("[Music] Cannot play: PulseAudio/miniaudio not ready\n");
            play_state_ = PlayState::STOPPED;
            update_main_ui();
            return;
        }

        // If currently paused, resume playback
        if (play_state_ == PlayState::PAUSED &&
            sound_loaded_ &&
            loaded_track_ == current_track_)
        {
            ma_result r = ma_sound_start(&audio_sound_);

            if (r == MA_SUCCESS)
            {
                play_state_ = PlayState::PLAYING;
            }
            else
            {
                printf("[Music] resume failed, result=%d\n", static_cast<int>(r));
                stop_playback();
                play_state_ = PlayState::STOPPED;
            }

            update_main_ui();
            return;
        }

        // Otherwise restart the current track
        if (start_playback())
            play_state_ = PlayState::PLAYING;
        else
            play_state_ = PlayState::STOPPED;

        update_main_ui();
    }

    void pause()
    {
        if (play_state_ != PlayState::PLAYING)
            return;

        if (sound_loaded_)
        {
            ma_sound_stop(&audio_sound_);
            play_state_ = PlayState::PAUSED;
        }

        update_main_ui();
    }

private:
    // ==================== Data members ====================

    std::unordered_map<std::string, lv_obj_t *> ui_obj_;

    std::string              browse_dir_;
    std::vector<std::string> browse_entries_;

    std::string              music_dir_;
    std::vector<std::string> playlist_;

    int current_track_ = 0;

    PlayState play_state_ = PlayState::STOPPED;
    ViewState view_state_ = ViewState::MAIN;

    // ==================== miniaudio / PulseAudio ====================

    ma_context audio_ctx_{};
    ma_engine  audio_engine_{};
    ma_sound   audio_sound_{};

    bool audio_ready_  = false;
    bool sound_loaded_ = false;

    int loaded_track_ = -1;

    std::atomic_bool track_finished_{false};

    lv_timer_t *audio_timer_ = nullptr;

private:
    // ==================== POSIX path utilities ====================

    static std::string path_parent(const std::string &path)
    {
        if (path.empty())
            return "/home/pi";

        std::vector<char> buf(path.begin(), path.end());
        buf.push_back('\0');

        char *p = dirname(buf.data());
        return std::string(p ? p : "/");
    }

    static std::string path_basename(const std::string &path)
    {
        if (path.empty())
            return "";

        std::vector<char> buf(path.begin(), path.end());
        buf.push_back('\0');

        char *p = basename(buf.data());
        return std::string(p ? p : "");
    }

    static std::string path_join(const std::string &base, const std::string &name)
    {
        if (base.empty())
            return name;

        if (name.empty())
            return base;

        if (base == "/")
            return "/" + name;

        if (!base.empty() && base.back() == '/')
            return base + name;

        return base + "/" + name;
    }

    static bool is_supported_audio_file(const std::string &fname)
    {
        size_t pos = fname.rfind('.');
        if (pos == std::string::npos)
            return false;

        std::string ext = fname.substr(pos);

        for (auto &c : ext)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        return ext == ".mp3" || ext == ".wav";
    }

private:
    // ==================== UI construction ====================

    void creat_UI()
    {
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, 320, 150);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["ui_bg"] = bg;

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
        lv_label_set_text(lbl_title, "Music Player");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, "i:Folder  p:List  ESC:Back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *content = lv_obj_create(bg);
        lv_obj_set_size(content, 320, 128);
        lv_obj_set_pos(content, 0, 22);
        lv_obj_set_style_radius(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["ui_content"] = content;

        lv_obj_t *cover = lv_obj_create(content);
        lv_obj_set_size(cover, 96, 96);
        lv_obj_set_pos(cover, 8, 16);
        lv_obj_set_style_radius(cover, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(cover, lv_color_hex(0x1A2A4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(cover, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(cover, lv_color_hex(0x3A5A8A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(cover, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(cover, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_note = lv_label_create(cover);
        lv_label_set_text(lbl_note, "MUSIC");
        lv_obj_set_align(lbl_note, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(lbl_note, lv_color_hex(0x4A7ABF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_note, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_track = lv_label_create(content);
        lv_label_set_text(lbl_track, "No track");
        lv_obj_set_pos(lbl_track, 114, 8);
        lv_obj_set_width(lbl_track, 198);
        lv_label_set_long_mode(lbl_track, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_color(lbl_track, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_track, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["ui_lbl_track"] = lbl_track;

        lv_obj_t *lbl_count = lv_label_create(content);
        lv_label_set_text(lbl_count, "0 / 0");
        lv_obj_set_pos(lbl_count, 114, 30);
        lv_obj_set_style_text_color(lbl_count, lv_color_hex(0x8AABCF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_count, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["ui_lbl_count"] = lbl_count;

        lv_obj_t *lbl_dir = lv_label_create(content);
        lv_label_set_text(lbl_dir, "Dir: (none)");
        lv_obj_set_pos(lbl_dir, 114, 48);
        lv_obj_set_width(lbl_dir, 198);
        lv_label_set_long_mode(lbl_dir, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_color(lbl_dir, lv_color_hex(0x6A8FAF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_dir, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["ui_lbl_dir"] = lbl_dir;

        lv_obj_t *lbl_state = lv_label_create(content);
        lv_label_set_text(lbl_state, "[STOPPED]");
        lv_obj_set_pos(lbl_state, 114, 65);
        lv_obj_set_style_text_color(lbl_state, lv_color_hex(0xFFD700), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["ui_lbl_state"] = lbl_state;

        lv_obj_t *lbl_keys = lv_label_create(content);
        lv_label_set_text(lbl_keys, "UP:Play  DOWN:Pause  LEFT/RIGHT:Prev/Next");
        lv_obj_set_pos(lbl_keys, 4, 112);
        lv_obj_set_width(lbl_keys, 312);
        lv_label_set_long_mode(lbl_keys, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_keys, lv_color_hex(0x4A5A6A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_keys, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

private:
    // ==================== Event binding ====================

    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIMusicPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIMusicPage *self = static_cast<UIMusicPage *>(lv_event_get_user_data(e));
        if (self)
            self->event_handler(e);
    }

    static uint32_t fzxc_to_lv_arrow(uint32_t key)
    {
        switch (key)
        {
        case KEY_F:
            return LV_KEY_UP;
        case KEY_X:
            return LV_KEY_DOWN;
        case KEY_Z:
            return LV_KEY_LEFT;
        case KEY_C:
            return LV_KEY_RIGHT;
        default:
            return key;
        }
    }

    void event_handler(lv_event_t *e)
    {
        lv_event_code_t ec = lv_event_get_code(e);

        if (ec == static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD))
        {
            struct key_item *elm = static_cast<struct key_item *>(lv_event_get_param(e));

            printf("[MUSIC][KEYBOARD] code=%u state=%s sym=%s view=%d\n",
                   elm->key_code,
                   kbd_state_name(elm->key_state),
                   elm->sym_name,
                   static_cast<int>(view_state_));
            return;
        }

        if (ec != LV_EVENT_KEY)
            return;

        uint32_t raw = lv_event_get_key(e);
        uint32_t key = fzxc_to_lv_arrow(raw);

        printf("[MUSIC][LV_KEY] raw=%u mapped=%u view=%d\n",
               raw,
               key,
               static_cast<int>(view_state_));

        switch (view_state_)
        {
        case ViewState::MAIN:
            handle_main_key(key);
            break;

        case ViewState::FOLDER_SEL:
            handle_folder_key(key);
            break;

        case ViewState::PLAYLIST:
            handle_playlist_key(key);
            break;
        }
    }

private:
    // ================================================================
    //  Main screen keys
    // ================================================================

    void handle_main_key(uint32_t key)
    {
        switch (key)
        {
        case LV_KEY_UP:
            play();
            break;

        case LV_KEY_DOWN:
            pause();
            break;

        case LV_KEY_LEFT:
            prev_track();
            break;

        case LV_KEY_RIGHT:
            next_track();
            break;

        case 15:
            open_folder_browser();
            break;

        case 'p':
            open_playlist();
            break;

        case LV_KEY_ESC:
            printf("[MUSIC] ESC -> go_back_home()\n");
            go_back_home();
            break;

        default:
            break;
        }
    }

private:
    // ================================================================
    //  Directory browser keys
    // ================================================================

    void handle_folder_key(uint32_t key)
    {
        lv_obj_t *roller = ui_obj_.count("ui_folder_roller") ? ui_obj_["ui_folder_roller"] : nullptr;
        if (!roller)
            return;

        switch (key)
        {
        case LV_KEY_UP:
        {
            uint16_t sel = lv_roller_get_selected(roller);
            if (sel > 0)
                lv_roller_set_selected(roller, sel - 1, LV_ANIM_ON);
            break;
        }

        case LV_KEY_DOWN:
        {
            uint16_t sel = lv_roller_get_selected(roller);
            uint16_t cnt = static_cast<uint16_t>(browse_entries_.size());

            if (cnt > 0 && sel + 1 < cnt)
                lv_roller_set_selected(roller, sel + 1, LV_ANIM_ON);

            break;
        }

        case LV_KEY_RIGHT:
        {
            uint16_t sel = lv_roller_get_selected(roller);

            if (sel < static_cast<uint16_t>(browse_entries_.size()))
            {
                const std::string &entry = browse_entries_[sel];

                if (entry != "..")
                {
                    std::string target = path_join(browse_dir_, entry);

                    struct stat st;
                    if (stat(target.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                    {
                        navigate_to(target);
                    }
                }
            }

            break;
        }

        case LV_KEY_LEFT:
        {
            navigate_to(path_parent(browse_dir_));
            break;
        }

        case LV_KEY_ENTER:
        {
            // ENTER selects the current directory and scans audio files
            load_music_from_folder(browse_dir_);
            close_folder_browser();
            break;
        }

        case LV_KEY_ESC:
            close_folder_browser();
            break;

        default:
            break;
        }
    }

private:
    // ================================================================
    //  Playlist keys
    // ================================================================

    void handle_playlist_key(uint32_t key)
    {
        lv_obj_t *roller = ui_obj_.count("ui_playlist_roller") ? ui_obj_["ui_playlist_roller"] : nullptr;

        switch (key)
        {
        case LV_KEY_UP:
        {
            if (roller)
            {
                uint16_t sel = lv_roller_get_selected(roller);
                if (sel > 0)
                    lv_roller_set_selected(roller, sel - 1, LV_ANIM_ON);
            }
            break;
        }

        case LV_KEY_DOWN:
        {
            if (roller)
            {
                uint16_t sel = lv_roller_get_selected(roller);
                uint16_t cnt = static_cast<uint16_t>(playlist_.size());

                if (cnt > 0 && sel + 1 < cnt)
                    lv_roller_set_selected(roller, sel + 1, LV_ANIM_ON);
            }
            break;
        }

        case LV_KEY_ENTER:
        {
            if (roller && !playlist_.empty())
            {
                uint16_t sel = lv_roller_get_selected(roller);

                if (sel < static_cast<uint16_t>(playlist_.size()))
                {
                    current_track_ = static_cast<int>(sel);

                    if (start_playback())
                        play_state_ = PlayState::PLAYING;
                    else
                        play_state_ = PlayState::STOPPED;
                }
            }

            close_playlist();
            update_main_ui();
            break;
        }

        case 'p':
        case LV_KEY_ESC:
            close_playlist();
            break;

        default:
            break;
        }
    }

private:
    // ================================================================
    //  Directory browser
    // ================================================================

    void open_folder_browser()
    {
        view_state_ = ViewState::FOLDER_SEL;
        browse_dir_ = "/";

        lv_obj_t *panel = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(panel, 316, 148);
        lv_obj_set_pos(panel, 2, 1);
        lv_obj_set_style_radius(panel, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x0D1B2A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel, 250, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(panel, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["ui_folder_panel"] = panel;

        lv_obj_t *lbl_path = lv_label_create(panel);
        lv_label_set_text(lbl_path, browse_dir_.c_str());
        lv_obj_set_pos(lbl_path, 4, 3);
        lv_obj_set_width(lbl_path, 308);
        lv_label_set_long_mode(lbl_path, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_color(lbl_path, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_path, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["ui_folder_path_lbl"] = lbl_path;

        lv_obj_t *lbl_h = lv_label_create(panel);
        lv_label_set_text(lbl_h, "UP/DN:sel  RIGHT:enter dir  LEFT:up  OK:load");
        lv_obj_set_pos(lbl_h, 2, 132);
        lv_obj_set_width(lbl_h, 312);
        lv_label_set_long_mode(lbl_h, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_h, lv_color_hex(0x3A5A7A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_h, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *roller = lv_roller_create(panel);
        lv_obj_set_size(roller, 308, 114);
        lv_obj_set_pos(roller, 4, 16);
        lv_obj_set_style_radius(roller, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(roller, lv_color_hex(0x0D1B2A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(roller, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(roller, lv_color_hex(0xCCDDEE), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(roller, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(roller, lv_color_hex(0x1F3A5F), LV_PART_SELECTED | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(roller, 220, LV_PART_SELECTED | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(roller, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(roller, &lv_font_montserrat_12, LV_PART_SELECTED | LV_STATE_DEFAULT);

        ui_obj_["ui_folder_roller"] = roller;

        refresh_folder_roller();
    }

    void navigate_to(const std::string &dir)
    {
        browse_dir_ = dir.empty() ? std::string("/") : dir;

        if (ui_obj_.count("ui_folder_path_lbl") && ui_obj_["ui_folder_path_lbl"])
            lv_label_set_text(ui_obj_["ui_folder_path_lbl"], browse_dir_.c_str());

        refresh_folder_roller();
    }

    void refresh_folder_roller()
    {
        browse_entries_.clear();

        if (browse_dir_ != "/")
            browse_entries_.push_back("..");

        DIR *dp = opendir(browse_dir_.c_str());

        if (dp)
        {
            struct dirent *ent;

            while ((ent = readdir(dp)) != nullptr)
            {
                if (ent->d_name[0] == '.')
                    continue;

                std::string full = path_join(browse_dir_, ent->d_name);

                struct stat st;
                if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                {
                    browse_entries_.push_back(ent->d_name);
                }
            }

            closedir(dp);
        }
        else
        {
            printf("[Music] opendir failed: %s\n", browse_dir_.c_str());
        }

        if (!browse_entries_.empty() && browse_entries_[0] == "..")
            std::sort(browse_entries_.begin() + 1, browse_entries_.end());
        else
            std::sort(browse_entries_.begin(), browse_entries_.end());

        std::string options;

        if (browse_entries_.empty())
        {
            options = "(empty)";
        }
        else
        {
            for (size_t i = 0; i < browse_entries_.size(); ++i)
            {
                if (i)
                    options += '\n';

                if (browse_entries_[i] == "..")
                    options += "../";
                else
                    options += browse_entries_[i] + "/";
            }
        }

        lv_obj_t *roller = ui_obj_.count("ui_folder_roller") ? ui_obj_["ui_folder_roller"] : nullptr;

        if (roller)
        {
            lv_roller_set_options(roller, options.c_str(), LV_ROLLER_MODE_NORMAL);
            lv_roller_set_visible_row_count(roller, 5);
            lv_roller_set_selected(roller, 0, LV_ANIM_OFF);
        }
    }

    void close_folder_browser()
    {
        if (ui_obj_.count("ui_folder_panel") && ui_obj_["ui_folder_panel"])
        {
            lv_obj_del(ui_obj_["ui_folder_panel"]);

            ui_obj_["ui_folder_panel"]    = nullptr;
            ui_obj_["ui_folder_roller"]   = nullptr;
            ui_obj_["ui_folder_path_lbl"] = nullptr;
        }

        view_state_ = ViewState::MAIN;
    }

private:
    // ================================================================
    //  Playlist popup
    // ================================================================

    void open_playlist()
    {
        if (playlist_.empty())
            return;

        view_state_ = ViewState::PLAYLIST;

        lv_obj_t *panel = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(panel, 316, 148);
        lv_obj_set_pos(panel, 2, 1);
        lv_obj_set_style_radius(panel, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x0D1B2A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel, 250, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(panel, lv_color_hex(0x00AA66), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["ui_playlist_panel"] = panel;

        lv_obj_t *lbl_t = lv_label_create(panel);

        char title_buf[64];
        snprintf(title_buf, sizeof(title_buf), "Playlist  %d audio", static_cast<int>(playlist_.size()));

        lv_label_set_text(lbl_t, title_buf);
        lv_obj_set_pos(lbl_t, 6, 3);
        lv_obj_set_style_text_color(lbl_t, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_t, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_h = lv_label_create(panel);
        lv_label_set_text(lbl_h, "UP/DOWN: select   OK: play   p/ESC: cancel");
        lv_obj_set_pos(lbl_h, 2, 132);
        lv_obj_set_style_text_color(lbl_h, lv_color_hex(0x2A6A4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_h, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        std::string options;

        for (size_t i = 0; i < playlist_.size(); ++i)
        {
            if (i)
                options += '\n';

            options += path_basename(playlist_[i]);
        }

        lv_obj_t *roller = lv_roller_create(panel);
        lv_roller_set_options(roller, options.c_str(), LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(roller, 5);

        uint16_t init_sel = 0;

        if (current_track_ >= 0 && current_track_ < static_cast<int>(playlist_.size()))
            init_sel = static_cast<uint16_t>(current_track_);

        lv_roller_set_selected(roller, init_sel, LV_ANIM_OFF);

        lv_obj_set_size(roller, 308, 114);
        lv_obj_set_pos(roller, 4, 16);
        lv_obj_set_style_radius(roller, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(roller, lv_color_hex(0x0D1B2A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(roller, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(roller, lv_color_hex(0xCCDDCC), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(roller, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(roller, lv_color_hex(0x1A4A2A), LV_PART_SELECTED | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(roller, 220, LV_PART_SELECTED | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(roller, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(roller, &lv_font_montserrat_12, LV_PART_SELECTED | LV_STATE_DEFAULT);

        ui_obj_["ui_playlist_roller"] = roller;
    }

    void close_playlist()
    {
        if (ui_obj_.count("ui_playlist_panel") && ui_obj_["ui_playlist_panel"])
        {
            lv_obj_del(ui_obj_["ui_playlist_panel"]);

            ui_obj_["ui_playlist_panel"]  = nullptr;
            ui_obj_["ui_playlist_roller"] = nullptr;
        }

        view_state_ = ViewState::MAIN;
    }

private:
    // ================================================================
    //  Scan mp3 / wav files in the specified directory
    // ================================================================

    void load_music_from_folder(const std::string &dir)
    {
        stop_playback();

        playlist_.clear();
        music_dir_ = dir;
        current_track_ = 0;
        play_state_ = PlayState::STOPPED;

        DIR *dp = opendir(dir.c_str());

        if (!dp)
        {
            printf("[Music] Cannot open dir: %s\n", dir.c_str());
            update_main_ui();
            return;
        }

        struct dirent *ent;

        while ((ent = readdir(dp)) != nullptr)
        {
            std::string fname = ent->d_name;

            if (fname.empty())
                continue;

            if (fname[0] == '.')
                continue;

            if (!is_supported_audio_file(fname))
                continue;

            std::string full = path_join(dir, fname);

            struct stat st;
            if (stat(full.c_str(), &st) != 0)
                continue;

            if (!S_ISREG(st.st_mode))
                continue;

            playlist_.push_back(full);
        }

        closedir(dp);

        std::sort(playlist_.begin(), playlist_.end());

        printf("[Music] Loaded %d audio files from %s\n",
               static_cast<int>(playlist_.size()),
               dir.c_str());

        update_main_ui();
    }

private:
    // ================================================================
    //  miniaudio initialization: PulseAudio backend
    // ================================================================

    void init_audio()
    {
        ma_backend backends[] = {
            ma_backend_pulseaudio
        };

        ma_result r = ma_context_init(backends, 1, nullptr, &audio_ctx_);

        if (r != MA_SUCCESS)
        {
            printf("[Music] ma_context_init PulseAudio failed, result=%d\n", static_cast<int>(r));
            audio_ready_ = false;
            return;
        }

        ma_engine_config engine_config = ma_engine_config_init();
        engine_config.pContext = &audio_ctx_;

        r = ma_engine_init(&engine_config, &audio_engine_);

        if (r != MA_SUCCESS)
        {
            printf("[Music] ma_engine_init failed, result=%d\n", static_cast<int>(r));

            ma_context_uninit(&audio_ctx_);

            audio_ready_ = false;
            return;
        }

        audio_ready_ = true;

        audio_timer_ = lv_timer_create(UIMusicPage::static_audio_timer_cb, 200, this);

        printf("[Music] miniaudio initialized with PulseAudio backend\n");
    }

    void uninit_audio()
    {
        if (audio_ready_)
        {
            ma_engine_uninit(&audio_engine_);
            ma_context_uninit(&audio_ctx_);
            audio_ready_ = false;
        }
    }

private:
    // ================================================================
    //  Playback completion callback
    //  Note:This callback runs on the audio thread; do not operate LVGL directly
    // ================================================================

    static void static_sound_end_cb(void *pUserData, ma_sound *pSound)
    {
        (void)pSound;

        UIMusicPage *self = static_cast<UIMusicPage *>(pUserData);
        if (!self)
            return;

        self->track_finished_.store(true);
    }

    static void static_audio_timer_cb(lv_timer_t *timer)
    {
#if defined(LVGL_VERSION_MAJOR) && LVGL_VERSION_MAJOR >= 9
        UIMusicPage *self = static_cast<UIMusicPage *>(lv_timer_get_user_data(timer));
#else
        UIMusicPage *self = static_cast<UIMusicPage *>(timer->user_data);
#endif

        if (!self)
            return;

        self->audio_timer_cb();
    }

    void audio_timer_cb()
    {
        if (!track_finished_.exchange(false))
            return;

        if (play_state_ != PlayState::PLAYING)
            return;

        if (playlist_.empty())
            return;

        current_track_ = (current_track_ + 1) % static_cast<int>(playlist_.size());

        if (start_playback())
            play_state_ = PlayState::PLAYING;
        else
            play_state_ = PlayState::STOPPED;

        update_main_ui();
    }

private:
    // ================================================================
    //  Playback control: miniaudio + PulseAudio
    // ================================================================

    bool start_playback()
    {
        stop_playback();

        if (!audio_ready_)
        {
            printf("[Music] Audio not ready. PulseAudio backend unavailable.\n");
            return false;
        }

        if (playlist_.empty())
            return false;

        if (current_track_ < 0 || current_track_ >= static_cast<int>(playlist_.size()))
            return false;

        const std::string &file = playlist_[current_track_];

        printf("[Music] Playing by miniaudio: %s\n", file.c_str());

        track_finished_.store(false);

        ma_uint32 flags = MA_SOUND_FLAG_STREAM;

        ma_result r = ma_sound_init_from_file(
            &audio_engine_,
            file.c_str(),
            flags,
            nullptr,
            nullptr,
            &audio_sound_);

        if (r != MA_SUCCESS)
        {
            printf("[Music] ma_sound_init_from_file failed, result=%d, file=%s\n",
                   static_cast<int>(r),
                   file.c_str());

            sound_loaded_ = false;
            loaded_track_ = -1;
            return false;
        }

        sound_loaded_ = true;
        loaded_track_ = current_track_;

        ma_sound_set_end_callback(
            &audio_sound_,
            UIMusicPage::static_sound_end_cb,
            this);

        r = ma_sound_start(&audio_sound_);

        if (r != MA_SUCCESS)
        {
            printf("[Music] ma_sound_start failed, result=%d\n", static_cast<int>(r));
            stop_playback();
            return false;
        }

        return true;
    }

    void stop_playback()
    {
        track_finished_.store(false);

        if (sound_loaded_)
        {
            ma_sound_set_end_callback(&audio_sound_, nullptr, nullptr);

            ma_sound_stop(&audio_sound_);
            ma_sound_uninit(&audio_sound_);

            sound_loaded_ = false;
            loaded_track_ = -1;
        }
    }

private:
    // ================================================================
    //  Refresh main screen labels
    // ================================================================

    void update_main_ui()
    {
        if (!playlist_.empty() &&
            current_track_ >= 0 &&
            current_track_ < static_cast<int>(playlist_.size()))
        {
            std::string fname = path_basename(playlist_[current_track_]);
            lv_label_set_text(ui_obj_["ui_lbl_track"], fname.c_str());
        }
        else
        {
            lv_label_set_text(ui_obj_["ui_lbl_track"], "No track");
        }

        char buf[32];

        snprintf(buf,
                 sizeof(buf),
                 "%d / %d",
                 playlist_.empty() ? 0 : current_track_ + 1,
                 static_cast<int>(playlist_.size()));

        lv_label_set_text(ui_obj_["ui_lbl_count"], buf);

        std::string dir_show = "Dir: " + (music_dir_.empty() ? std::string("(none)") : music_dir_);
        lv_label_set_text(ui_obj_["ui_lbl_dir"], dir_show.c_str());

        const char *state_str = "[STOPPED]";

        if (!audio_ready_)
        {
            state_str = "[NO PULSE]";
        }
        else if (play_state_ == PlayState::PLAYING)
        {
            state_str = "[ PLAYING ]";
        }
        else if (play_state_ == PlayState::PAUSED)
        {
            state_str = "[ PAUSED  ]";
        }

        lv_label_set_text(ui_obj_["ui_lbl_state"], state_str);
    }
};