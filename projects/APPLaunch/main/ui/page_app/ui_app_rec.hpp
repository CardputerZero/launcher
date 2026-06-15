/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "sample_log.h"

#include "../ui_app_page.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "compat/input_keys.h"
#include "hal_lvgl_bsp.h"

class rec_page : public AppPage
{
public:
    lv_font_t *svg_font = NULL;
    static constexpr uint32_t kColorText = 0xFFFFFF;
    static constexpr uint32_t kColorIconStop = 0xFF0000;
    static constexpr uint32_t kColorIconRecord = 0xFF3399;
    static constexpr uint32_t kColorIconSampleRate = 0xFFCC00;
    static constexpr uint32_t kColorIconList = 0x33CC33;

    typedef enum
    {
        ICON_EXIT = 0,
        ICON_FAST_FORWARD ,
        ICON_FAST_REWIND,
        ICON_LIST,
        ICON_PAUSE,
        ICON_PLAY,
        ICON_RECORD,
        ICON_SAMPLE_RATE,
        ICON_SPEED,
        ICON_STOP,
        ICON_COUNT,
    } ICON_t;

    std::array<std::pair<ICON_t, std::string>, ICON_COUNT> icon_map = {{
        {ICON_EXIT, "\uEA01"},
        {ICON_FAST_FORWARD, "\uEA02"},
        {ICON_FAST_REWIND, "\uEA03"},
        {ICON_LIST, "\uEA04"},
        {ICON_PAUSE, "\uEA05"},
        {ICON_PLAY, "\uEA06"},
        {ICON_RECORD, "\uEA07"},
        {ICON_SAMPLE_RATE, "\uEA08"},
        {ICON_SPEED, "\uEA09"},
        {ICON_STOP, "\uEA0A"},
    }};

public:
    lv_obj_t *ui_BOTTOM_Container;
    lv_obj_t *but[5];

public:
    rec_page() : AppPage()
    {
        page_title_ = "Recorder";
        set_page_title(page_title_);
        svg_font = lv_freetype_font_create(
            cp0_file_path("svgfont.ttf").c_str(), LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 16,
            LV_FREETYPE_FONT_STYLE_NORMAL);
        init_APP_UI();
        creat_BOTTOM_UI();
    }
    ~rec_page()
    {
        if (svg_font)
            lv_freetype_font_delete(svg_font);
    }

    struct lvgl_call_d_t
    {
        void *p1;
        std::function<void(lv_event_code_t, void *, void *)> callback;
    };

    static void lvgl_event_handler(lv_event_t *e)
    {
        lvgl_call_d_t *t = static_cast<lvgl_call_d_t *>(lv_event_get_user_data(e));
        if (!t)
            return;
        lv_event_code_t c = lv_event_get_code(e);
        if (c == LV_EVENT_DELETE)
        {
            delete t;
            return;
        }

        auto callback = t->callback;
        void *event_param = lv_event_get_param(e);
        void *user_data = t->p1;
        if (callback)
        {
            try
            {
                callback(c, event_param, user_data);
            }
            catch (...)
            {
                fprintf(stderr, "[LVGL] C++ event callback threw\n");
            }
        }
    }

    void lvgl_add_call(lv_obj_t *obj, std::function<void(lv_event_code_t, void *, void *)> callback, void *d)
    {
        if (!obj || !callback)
            return;
        {
            uint32_t event_cnt = lv_obj_get_event_count(obj);
            int32_t i;
            if (event_cnt != 0)
                for (i = event_cnt - 1; i >= 0; i--)
                {
                    lv_event_dsc_t *dsc = lv_obj_get_event_dsc(obj, i);
                    if (dsc && (lv_event_dsc_get_cb(dsc) == rec_page::lvgl_event_handler))
                    {
                        lvgl_call_d_t *data = static_cast<lvgl_call_d_t *>(lv_event_dsc_get_user_data(dsc));
                        lv_obj_remove_event(obj, i);
                        delete data;
                    }
                }
        }
        lvgl_call_d_t *t = new lvgl_call_d_t;
        t->p1 = d;
        t->callback = std::move(callback);
        lv_obj_add_event_cb(obj, rec_page::lvgl_event_handler, LV_EVENT_ALL, t);
    }
    const char *icon_text(ICON_t icon)
    {
        const size_t index = static_cast<size_t>(icon);
        return index < icon_map.size() ? icon_map[index].second.c_str() : "";
    }
private:
    static uint8_t idle_wave_height(int index)
    {
        const float phase = static_cast<float>(index) * 0.72f;
        const float envelope = 0.55f + 0.45f * std::fabs(std::sin(phase * 0.37f));
        const float pulse = 0.5f + 0.5f * std::sin(phase);
        return static_cast<uint8_t>(4.0f + envelope * pulse * 34.0f);
    }

    void init_APP_UI()
    {
        lv_obj_set_height(ui_APP_Container, 125);
        lv_obj_set_y(ui_APP_Container, 20);
        lv_obj_clear_flag(ui_APP_Container, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_remove_style_all(bg);
        lv_obj_set_size(bg, 320, 125);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1117), 0);
        lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
        lv_obj_clear_flag(bg, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        lv_obj_t *file = lv_label_create(bg);
        lv_obj_set_pos(file, 8, 6);
        lv_obj_set_width(file, 304);
        lv_label_set_long_mode(file, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_font(file, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(file, lv_color_hex(0xAAB6C4), 0);
        lv_label_set_text(file, "rec_0001.wav");

        lv_obj_t *wave_bg = lv_obj_create(bg);
        lv_obj_remove_style_all(wave_bg);
        lv_obj_set_size(wave_bg, 304, 56);
        lv_obj_set_pos(wave_bg, 8, 25);
        lv_obj_set_style_radius(wave_bg, 4, 0);
        lv_obj_set_style_bg_color(wave_bg, lv_color_hex(0x161B22), 0);
        lv_obj_set_style_bg_opa(wave_bg, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(wave_bg, lv_color_hex(0x30363D), 0);
        lv_obj_set_style_border_width(wave_bg, 1, 0);
        lv_obj_clear_flag(wave_bg, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        constexpr int bar_count = 40;
        const int bar_w = 4;
        const int gap = 3;
        const int start_x = 9;
        const int mid_y = 28;
        for (int i = 0; i < bar_count; ++i)
        {
            int h = idle_wave_height(i);
            lv_obj_t *bar = lv_obj_create(wave_bg);
            lv_obj_remove_style_all(bar);
            lv_obj_set_size(bar, bar_w, h);
            lv_obj_set_pos(bar, start_x + i * (bar_w + gap), mid_y - h / 2);
            lv_obj_set_style_radius(bar, 2, 0);
            lv_obj_set_style_bg_color(bar, lv_color_hex(0xFF8800), 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_clear_flag(bar, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        }

        lv_obj_t *timer = lv_label_create(bg);
        lv_obj_set_pos(timer, 0, 85);
        lv_obj_set_width(timer, 320);
        lv_obj_set_style_text_font(timer, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(timer, lv_color_hex(kColorText), 0);
        lv_obj_set_style_text_align(timer, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(timer, "00:00");

        lv_obj_t *sample_rate = lv_label_create(bg);
        lv_obj_set_pos(sample_rate, 8, 101);
        lv_obj_set_width(sample_rate, 90);
        lv_obj_set_style_text_font(sample_rate, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(sample_rate, lv_color_hex(0x8B949E), 0);
        lv_label_set_text(sample_rate, "44.1kHz");

        lv_obj_t *hint = lv_label_create(bg);
        lv_obj_set_pos(hint, 218, 101);
        lv_obj_set_width(hint, 94);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x8B949E), 0);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(hint, "REC / LIST");
    }



    void creat_BOTTOM_UI()
    {
        ui_BOTTOM_Container = lv_obj_create(root_screen_);
        lv_obj_remove_style_all(ui_BOTTOM_Container);
        lv_obj_set_width(ui_BOTTOM_Container, 320);
        lv_obj_set_height(ui_BOTTOM_Container, 25);
        lv_obj_set_x(ui_BOTTOM_Container, 0);
        lv_obj_set_y(ui_BOTTOM_Container, 145);
        lv_obj_set_align(ui_BOTTOM_Container, LV_ALIGN_TOP_LEFT);
        lv_obj_clear_flag(ui_BOTTOM_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE)); /// Flags

        const char *icons[5] = {
            icon_text(ICON_EXIT),
            icon_text(ICON_STOP),
            icon_text(ICON_RECORD),
            icon_text(ICON_SAMPLE_RATE),
            icon_text(ICON_LIST),
        };
        const uint32_t colors[5] = {
            kColorText,
            kColorIconStop,
            kColorIconRecord,
            kColorIconSampleRate,
            kColorIconList,
        };
        for (int i = 0; i < 5; i++)
        {
            but[i] = lv_btn_create(ui_BOTTOM_Container);
            lv_obj_remove_style_all(but[i]);
            lv_obj_clear_flag(but[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(but[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(but[i], 0, 0);
            lv_obj_set_style_shadow_width(but[i], 0, 0);
            lv_obj_set_style_pad_all(but[i], 0, 0);

            lv_obj_t *label = lv_label_create(but[i]);
            lv_obj_set_style_text_font(label, svg_font ? svg_font : &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(label, lv_color_hex(colors[i]), 0);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(label, icons[i]);
            lv_obj_update_layout(label);

            lv_coord_t label_w = lv_obj_get_width(label);
            lv_coord_t label_h = lv_obj_get_height(label);
            lv_obj_set_size(but[i], label_w, label_h);
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_pos(but[i], i * 64 + (64 - label_w) / 2, (25 - label_h) / 2);
        }

        for (int i = 0; i < 5; i++)
        {
            lv_obj_add_flag(but[i], LV_OBJ_FLAG_CLICKABLE);
        }
    }
};

namespace rec_ui2
{
static constexpr int kScreenW = 320;
static constexpr int kContentH = 125;
static constexpr int kBtnCount = 5;
static constexpr int kWaveBarCount = 40;
static constexpr uint32_t kBg = 0x0D1117;
static constexpr uint32_t kPanel = 0x161B22;
static constexpr uint32_t kBorder = 0x30363D;
static constexpr uint32_t kText = 0xFFFFFF;
static constexpr uint32_t kMuted = 0x8B949E;
static constexpr uint32_t kAccent = 0x1F9DFF;
static constexpr uint32_t kWave = 0xFF8800;

static inline lv_color_t color(uint32_t hex)
{
    return lv_color_hex(hex);
}

static inline void prep_page(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, kScreenW, kContentH);
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, color(kBg), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}
}

class RecHomeView
{
public:
    static constexpr size_t kWavePointCount = 128;

    void create(lv_obj_t *page)
    {
        lbl_file_ = lv_label_create(page);
        lv_obj_set_pos(lbl_file_, 8, 6);
        lv_obj_set_width(lbl_file_, 304);
        lv_label_set_long_mode(lbl_file_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_font(lbl_file_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_file_, rec_ui2::color(0xAAB6C4), 0);

        wave_bg_ = lv_obj_create(page);
        lv_obj_remove_style_all(wave_bg_);
        lv_obj_set_size(wave_bg_, 304, 56);
        lv_obj_set_pos(wave_bg_, 8, 25);
        lv_obj_set_style_radius(wave_bg_, 4, 0);
        lv_obj_set_style_bg_color(wave_bg_, rec_ui2::color(rec_ui2::kPanel), 0);
        lv_obj_set_style_bg_opa(wave_bg_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(wave_bg_, rec_ui2::color(rec_ui2::kBorder), 0);
        lv_obj_set_style_border_width(wave_bg_, 1, 0);
        lv_obj_clear_flag(wave_bg_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        for (int i = 0; i < rec_ui2::kWaveBarCount; ++i)
        {
            bars_[i] = lv_obj_create(wave_bg_);
            lv_obj_remove_style_all(bars_[i]);
            lv_obj_set_size(bars_[i], 4, 1);
            lv_obj_set_style_radius(bars_[i], 2, 0);
            lv_obj_set_style_bg_color(bars_[i], rec_ui2::color(rec_ui2::kWave), 0);
            lv_obj_set_style_bg_opa(bars_[i], LV_OPA_COVER, 0);
            lv_obj_clear_flag(bars_[i], (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        }

        lbl_time_ = lv_label_create(page);
        lv_obj_set_pos(lbl_time_, 0, 85);
        lv_obj_set_width(lbl_time_, 320);
        lv_obj_set_style_text_font(lbl_time_, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_time_, rec_ui2::color(rec_ui2::kText), 0);
        lv_obj_set_style_text_align(lbl_time_, LV_TEXT_ALIGN_CENTER, 0);

        lbl_rate_ = lv_label_create(page);
        lv_obj_set_pos(lbl_rate_, 8, 101);
        lv_obj_set_width(lbl_rate_, 90);
        lv_obj_set_style_text_font(lbl_rate_, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lbl_rate_, rec_ui2::color(rec_ui2::kMuted), 0);

        lbl_hint_ = lv_label_create(page);
        lv_obj_set_pos(lbl_hint_, 178, 101);
        lv_obj_set_width(lbl_hint_, 134);
        lv_obj_set_style_text_font(lbl_hint_, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lbl_hint_, rec_ui2::color(rec_ui2::kMuted), 0);
        lv_obj_set_style_text_align(lbl_hint_, LV_TEXT_ALIGN_RIGHT, 0);
    }

    void refresh(bool recording,
                 const std::string &file,
                 const std::string &time,
                 const std::string &rate)
    {
        if (!lbl_file_)
            return;

        lv_label_set_text(lbl_file_, file.empty() ? "No recordings" : file.c_str());
        lv_label_set_text(lbl_time_, recording ? time.c_str() : "00:00");
        lv_label_set_text(lbl_rate_, rate.c_str());
        lv_label_set_text(lbl_hint_, recording ? "RECORDING" : "REC / LIST");
        refresh_bars(recording);
    }

    void set_waveform(const std::string &data)
    {
        if (data.size() < sizeof(float) * waveform_.size())
            return;
        std::lock_guard<std::mutex> lock(waveform_mutex_);
        std::memcpy(waveform_.data(), data.data(), sizeof(float) * waveform_.size());
    }

    void reset_waveform()
    {
        std::lock_guard<std::mutex> lock(waveform_mutex_);
        waveform_.fill(0.0f);
    }

    size_t waveform_byte_size() const
    {
        return sizeof(float) * waveform_.size();
    }

private:
    void refresh_bars(bool recording)
    {
        std::array<float, kWavePointCount> snapshot{};
        if (recording)
        {
            std::lock_guard<std::mutex> lock(waveform_mutex_);
            snapshot = waveform_;
        }

        constexpr int bar_w = 4;
        constexpr int gap = 3;
        constexpr int start_x = 9;
        constexpr int mid_y = 28;
        constexpr int max_h = 48;
        for (int i = 0; i < rec_ui2::kWaveBarCount; ++i)
        {
            float max_val = 0.02f;
            if (recording)
            {
                int start = i * static_cast<int>(kWavePointCount) / rec_ui2::kWaveBarCount;
                int end = (i + 1) * static_cast<int>(kWavePointCount) / rec_ui2::kWaveBarCount;
                for (int j = start; j < end; ++j)
                {
                    max_val = std::max(max_val, std::min(1.0f, std::fabs(snapshot[j])));
                }
            }
            else
            {
                max_val = 0.05f + static_cast<float>((i * 7) % 9) / 28.0f;
            }
            int h = std::max(2, static_cast<int>(max_val * max_h));
            lv_obj_set_size(bars_[i], bar_w, h);
            lv_obj_set_pos(bars_[i], start_x + i * (bar_w + gap), mid_y - h / 2);
        }
    }

    lv_obj_t *lbl_file_ = nullptr;
    lv_obj_t *wave_bg_ = nullptr;
    std::array<lv_obj_t *, rec_ui2::kWaveBarCount> bars_{};
    lv_obj_t *lbl_time_ = nullptr;
    lv_obj_t *lbl_rate_ = nullptr;
    lv_obj_t *lbl_hint_ = nullptr;
    std::array<float, kWavePointCount> waveform_{};
    std::mutex waveform_mutex_;
};

class RecSaveConfirmView
{
public:
    void create(lv_obj_t *page)
    {
        lbl_title_ = lv_label_create(page);
        lv_obj_set_pos(lbl_title_, 8, 10);
        lv_obj_set_width(lbl_title_, 304);
        lv_obj_set_style_text_font(lbl_title_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_title_, rec_ui2::color(rec_ui2::kMuted), 0);
        lv_label_set_text(lbl_title_, "Save recording as");

        ta_name_ = lv_textarea_create(page);
        lv_obj_set_pos(ta_name_, 8, 34);
        lv_obj_set_size(ta_name_, 304, 34);
        lv_obj_set_style_text_font(ta_name_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(ta_name_, rec_ui2::color(rec_ui2::kText), 0);
        lv_obj_set_style_bg_color(ta_name_, rec_ui2::color(rec_ui2::kPanel), 0);
        lv_obj_set_style_bg_opa(ta_name_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(ta_name_, rec_ui2::color(rec_ui2::kAccent), 0);
        lv_obj_set_style_border_width(ta_name_, 1, 0);
        lv_obj_set_style_radius(ta_name_, 4, 0);
        lv_textarea_set_one_line(ta_name_, true);

        lbl_hint_ = lv_label_create(page);
        lv_obj_set_pos(lbl_hint_, 8, 84);
        lv_obj_set_width(lbl_hint_, 304);
        lv_obj_set_style_text_font(lbl_hint_, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lbl_hint_, rec_ui2::color(rec_ui2::kMuted), 0);
        lv_label_set_text(lbl_hint_, "Back: discard  Prev/Next: rename  Rec: reset  List: save");
    }

    void set_name(const std::string &name)
    {
        if (ta_name_)
            lv_textarea_set_text(ta_name_, name.c_str());
    }

    std::string name() const
    {
        return ta_name_ ? lv_textarea_get_text(ta_name_) : "";
    }

private:
    lv_obj_t *lbl_title_ = nullptr;
    lv_obj_t *ta_name_ = nullptr;
    lv_obj_t *lbl_hint_ = nullptr;
};

class RecFileListView
{
public:
    void create(lv_obj_t *page)
    {
        for (int i = 0; i < 5; ++i)
        {
            items_[i] = lv_label_create(page);
            lv_obj_set_pos(items_[i], 8, 8 + i * 21);
            lv_obj_set_width(items_[i], 304);
            lv_obj_set_style_text_font(items_[i], &lv_font_montserrat_12, 0);
            lv_label_set_long_mode(items_[i], LV_LABEL_LONG_CLIP);
        }

        empty_ = lv_label_create(page);
        lv_obj_set_width(empty_, 320);
        lv_obj_set_style_text_font(empty_, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(empty_, rec_ui2::color(rec_ui2::kMuted), 0);
        lv_obj_set_style_text_align(empty_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(empty_, "Empty");
        lv_obj_center(empty_);
    }

    void refresh(const std::vector<std::string> &files, int selected)
    {
        if (files.empty())
        {
            lv_obj_clear_flag(empty_, LV_OBJ_FLAG_HIDDEN);
            for (auto *item : items_)
                lv_obj_add_flag(item, LV_OBJ_FLAG_HIDDEN);
            offset_ = 0;
            return;
        }

        lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
        if (selected < offset_)
            offset_ = selected;
        if (selected >= offset_ + 5)
            offset_ = selected - 4;
        if (offset_ < 0)
            offset_ = 0;

        for (int i = 0; i < 5; ++i)
        {
            int idx = offset_ + i;
            if (idx >= static_cast<int>(files.size()))
            {
                lv_obj_add_flag(items_[i], LV_OBJ_FLAG_HIDDEN);
                continue;
            }

            std::string text = (idx == selected ? "> " : "  ") + files[idx];
            lv_label_set_text(items_[i], text.c_str());
            lv_obj_set_style_text_color(items_[i],
                                        rec_ui2::color(idx == selected ? rec_ui2::kAccent : rec_ui2::kText),
                                        0);
            lv_obj_clear_flag(items_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

private:
    lv_obj_t *empty_ = nullptr;
    std::array<lv_obj_t *, 5> items_{};
    int offset_ = 0;
};

class RecPlaybackView
{
public:
    void create(lv_obj_t *page)
    {
        lbl_file_ = lv_label_create(page);
        lv_obj_set_pos(lbl_file_, 8, 6);
        lv_obj_set_width(lbl_file_, 304);
        lv_obj_set_style_text_font(lbl_file_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_file_, rec_ui2::color(rec_ui2::kText), 0);
        lv_label_set_long_mode(lbl_file_, LV_LABEL_LONG_SCROLL_CIRCULAR);

        for (int i = 0; i < rec_ui2::kWaveBarCount; ++i)
        {
            bars_[i] = lv_obj_create(page);
            lv_obj_remove_style_all(bars_[i]);
            lv_obj_set_style_bg_color(bars_[i], rec_ui2::color(0x888888), 0);
            lv_obj_set_style_bg_opa(bars_[i], LV_OPA_COVER, 0);
            lv_obj_set_style_radius(bars_[i], 1, 0);
            lv_obj_clear_flag(bars_[i], (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        }

        progress_ = lv_obj_create(page);
        lv_obj_remove_style_all(progress_);
        lv_obj_set_size(progress_, 2, 50);
        lv_obj_set_style_bg_color(progress_, rec_ui2::color(0xFF3333), 0);
        lv_obj_set_style_bg_opa(progress_, LV_OPA_COVER, 0);

        lbl_time_ = lv_label_create(page);
        lv_obj_set_pos(lbl_time_, 0, 96);
        lv_obj_set_width(lbl_time_, 320);
        lv_obj_set_style_text_font(lbl_time_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_time_, rec_ui2::color(rec_ui2::kMuted), 0);
        lv_obj_set_style_text_align(lbl_time_, LV_TEXT_ALIGN_CENTER, 0);
    }

    void refresh(const std::string &file, const std::string &time, float progress, bool paused)
    {
        lv_label_set_text(lbl_file_, file.c_str());
        std::string time_text = paused ? time + " paused" : time;
        lv_label_set_text(lbl_time_, time_text.c_str());

        constexpr int bar_w = 5;
        constexpr int gap = 2;
        constexpr int start_x = 20;
        constexpr int base_y = 82;
        constexpr int max_h = 42;
        for (int i = 0; i < rec_ui2::kWaveBarCount; ++i)
        {
            int h = 4 + ((i * 13 + 7) % max_h);
            lv_obj_set_size(bars_[i], bar_w, h);
            lv_obj_set_pos(bars_[i], start_x + i * (bar_w + gap), base_y - h);
        }

        if (progress < 0.0f)
        {
            lv_obj_add_flag(progress_, LV_OBJ_FLAG_HIDDEN);
            return;
        }

        lv_obj_clear_flag(progress_, LV_OBJ_FLAG_HIDDEN);
        progress = std::max(0.0f, std::min(1.0f, progress));
        int x = start_x + static_cast<int>(progress * (rec_ui2::kWaveBarCount * (bar_w + gap) - gap));
        lv_obj_set_pos(progress_, x, 35);
    }

private:
    lv_obj_t *lbl_file_ = nullptr;
    std::array<lv_obj_t *, rec_ui2::kWaveBarCount> bars_{};
    lv_obj_t *progress_ = nullptr;
    lv_obj_t *lbl_time_ = nullptr;
};

class RecorderAudioClient
{
public:
    using Callback = std::function<void(int, std::string)>;

    void set_status_callback(Callback callback)
    {
        request({"SetCallback"}, std::move(callback), false);
    }

    void clear_status_callback()
    {
        request({"SetCallback"}, nullptr, false);
    }

    void set_waveform_enabled(bool enabled)
    {
        cp0_signal_audio_setup({"set_waveform", enabled ? "on" : "off"}, nullptr);
    }

    void start_capture(Callback callback)
    {
        request({"Cap"}, std::move(callback));
    }

    void stop_capture(Callback callback = nullptr)
    {
        request({"CapEnd"}, std::move(callback));
    }

    void save_capture_file(const std::string &path, Callback callback)
    {
        request({"CapFileSave", path}, std::move(callback));
    }

    void play_file(const std::string &path, Callback callback)
    {
        request({"PlayFile", path}, std::move(callback));
    }

    void pause_playback()
    {
        request({"PlayPause"});
    }

    void continue_playback()
    {
        request({"PlayContinue"});
    }

    void stop_playback()
    {
        request({"PlayEnd"});
    }

private:
    void request(std::initializer_list<std::string> args,
                 Callback callback = nullptr,
                 bool swallow_result = true)
    {
        if (!callback && swallow_result)
            callback = [](int, std::string) {};
        cp0_signal_audio_api(std::list<std::string>(args), std::move(callback));
    }
};

class RecorderFileStore
{
public:
    std::string recordings_dir() const
    {
        const char *home = getenv("HOME");
        std::string base = home ? std::string(home) : std::string("/tmp");
        std::string music = base + "/Music";
        std::string dir = music + "/Recorder";

        struct stat st;
        if (stat(music.c_str(), &st) != 0)
            mkdir(music.c_str(), 0755);
        if (stat(dir.c_str(), &st) != 0)
            mkdir(dir.c_str(), 0755);
        return dir;
    }

    std::vector<std::string> scan() const
    {
        std::vector<std::string> recordings;
        std::string dir = recordings_dir();
        DIR *d = opendir(dir.c_str());
        if (!d)
            return recordings;

        struct dirent *entry = nullptr;
        while ((entry = readdir(d)) != nullptr)
        {
            size_t len = std::strlen(entry->d_name);
            if (len >= 5 && std::strcmp(entry->d_name + len - 4, ".wav") == 0)
                recordings.push_back(entry->d_name);
        }
        closedir(d);

        std::sort(recordings.begin(), recordings.end(), std::greater<std::string>());
        return recordings;
    }

    std::string generate_filename() const
    {
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&tt, &tm);

        std::ostringstream oss;
        oss << "rec_"
            << (tm.tm_year + 1900)
            << std::setfill('0') << std::setw(2) << (tm.tm_mon + 1)
            << std::setw(2) << tm.tm_mday << "_"
            << std::setw(2) << tm.tm_hour
            << std::setw(2) << tm.tm_min
            << std::setw(2) << tm.tm_sec
            << ".wav";
        return recordings_dir() + "/" + oss.str();
    }

    std::string make_serial_name(int serial) const
    {
        std::ostringstream oss;
        oss << "rec_take_" << std::setfill('0') << std::setw(3) << serial << ".wav";
        return oss.str();
    }

    std::string normalized_wav_name(std::string name) const
    {
        while (!name.empty() && (name.front() == ' ' || name.front() == '/'))
            name.erase(name.begin());
        if (name.empty())
            return "";
        if (name.size() < 4 || name.substr(name.size() - 4) != ".wav")
            name += ".wav";
        return filename_only(name);
    }

    static std::string filename_only(const std::string &path)
    {
        size_t slash = path.find_last_of('/');
        return slash == std::string::npos ? path : path.substr(slash + 1);
    }
};

class UIRecPage : public rec_page
{
public:
    UIRecPage()
    {
        build_pages();
        scan_files();
        bind_audio_callback();
        bind_keyboard_shortcuts();
        switch_page(UiPage::Home);
        refresh_view();
    }

    ~UIRecPage()
    {
        alive_->store(false);
        stop_process(false);
        audio_.clear_status_callback();
        if (elapsed_timer_)
            lv_timer_delete(elapsed_timer_);
        if (poll_timer_)
            lv_timer_delete(poll_timer_);
    }

private:
    enum class RecState
    {
        Idle,
        Recording,
        Playing,
    };

    enum class UiPage
    {
        Home,
        SaveConfirm,
        FileList,
        Playback,
        Count,
    };

    using AudioCallback = RecorderAudioClient::Callback;

    void build_pages()
    {
        lv_obj_clean(ui_APP_Container);
        lv_obj_set_height(ui_APP_Container, rec_ui2::kContentH);
        lv_obj_set_y(ui_APP_Container, 20);
        lv_obj_clear_flag(ui_APP_Container, LV_OBJ_FLAG_SCROLLABLE);

        for (size_t i = 0; i < pages_.size(); ++i)
        {
            pages_[i] = lv_obj_create(ui_APP_Container);
            rec_ui2::prep_page(pages_[i]);
            lv_obj_add_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
        }

        page_home_.create(pages_[static_cast<size_t>(UiPage::Home)]);
        page_save_.create(pages_[static_cast<size_t>(UiPage::SaveConfirm)]);
        page_files_.create(pages_[static_cast<size_t>(UiPage::FileList)]);
        page_playback_.create(pages_[static_cast<size_t>(UiPage::Playback)]);

        poll_timer_ = lv_timer_create(&UIRecPage::poll_timer_cb, 200, this);
    }

    void bind_audio_callback()
    {
        audio_.set_status_callback(make_audio_callback([this](int code, std::string data) {
            on_audio_status(code, data);
        }));
    }

    AudioCallback make_audio_callback(AudioCallback callback)
    {
        auto alive = alive_;
        return [alive, callback = std::move(callback)](int code, std::string data) mutable {
            if (!alive->load() || !callback)
                return;
            callback(code, std::move(data));
        };
    }

    void bind_button(int index, std::function<void()> callback)
    {
        if (index < 0 || index >= rec_ui2::kBtnCount)
            return;
        button_actions_[index] = callback;
        lvgl_add_call(but[index], [callback](lv_event_code_t c, void *event_param, void *user_data) {
            (void)user_data;
            if (!callback)
                return;
            if (c == LV_EVENT_CLICKED)
            {
                callback();
                return;
            }
            if (c == static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD))
            {
                struct key_item *key = static_cast<struct key_item *>(event_param);
                if (key && key->key_state == 0)
                    callback();
            }
        }, NULL);
    }

    void bind_keyboard_shortcuts()
    {
        lvgl_add_call(root_screen_, [this](lv_event_code_t c, void *event_param, void *user_data) {
            (void)user_data;
            if (c != static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD))
                return;

            struct key_item *key = static_cast<struct key_item *>(event_param);
            if (!key || key->key_state != 0)
                return;

            if (key->key_code == KEY_ESC)
            {
                if (button_actions_[0])
                    button_actions_[0]();
                return;
            }

            int index = -1;
            switch (key->key_code)
            {
            case KEY_1:
                index = 0;
                break;
            case KEY_2:
                index = 1;
                break;
            case KEY_3:
                index = 2;
                break;
            case KEY_4:
                index = 3;
                break;
            case KEY_5:
                index = 4;
                break;
            default:
                break;
            }

            if (index >= 0 && index < rec_ui2::kBtnCount && button_actions_[index])
                button_actions_[index]();
        }, NULL);
    }

    lv_obj_t *button_label(int index)
    {
        if (index < 0 || index >= rec_ui2::kBtnCount)
            return nullptr;
        return but[index] ? lv_obj_get_child(but[index], 0) : nullptr;
    }

    void set_button_icon(int index, ICON_t icon, lv_opa_t opa = LV_OPA_COVER)
    {
        lv_obj_t *label = button_label(index);
        if (!label)
            return;
        lv_label_set_text(label, icon_text(icon));
        lv_obj_set_style_text_opa(label, opa, 0);
    }

    void switch_page(UiPage page)
    {
        for (size_t i = 0; i < pages_.size(); ++i)
        {
            if (!pages_[i])
                continue;
            if (i == static_cast<size_t>(page))
                lv_obj_clear_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
        }
        current_page_ = page;
        bind_buttons_for_page();
        refresh_view();
    }

    void bind_buttons_for_page()
    {
        switch (current_page_)
        {
        case UiPage::Home:
            set_button_icon(0, ICON_EXIT);
            set_button_icon(1, ICON_STOP, state_ == RecState::Recording ? LV_OPA_COVER : LV_OPA_40);
            set_button_icon(2, state_ == RecState::Recording ? ICON_SAMPLE_RATE : ICON_RECORD);
            set_button_icon(3, ICON_SAMPLE_RATE, LV_OPA_40);
            set_button_icon(4, ICON_LIST, state_ == RecState::Recording ? LV_OPA_40 : LV_OPA_COVER);
            bind_button(0, [this]() { on_home_back(); });
            bind_button(1, [this]() { if (state_ == RecState::Recording) stop_recording_to_save(); });
            bind_button(2, [this]() { toggle_record(); });
            bind_button(3, []() {});
            bind_button(4, [this]() { if (state_ == RecState::Idle) switch_page(UiPage::FileList); });
            break;
        case UiPage::SaveConfirm:
            set_button_icon(0, ICON_EXIT);
            set_button_icon(1, ICON_FAST_REWIND);
            set_button_icon(2, ICON_RECORD);
            set_button_icon(3, ICON_FAST_FORWARD);
            set_button_icon(4, ICON_LIST);
            bind_button(0, [this]() { discard_pending_recording(); });
            bind_button(1, [this]() { bump_save_name(-1); });
            bind_button(2, [this]() { reset_save_name(); });
            bind_button(3, [this]() { bump_save_name(1); });
            bind_button(4, [this]() { save_pending_recording(); });
            break;
        case UiPage::FileList:
            set_button_icon(0, ICON_EXIT);
            set_button_icon(1, ICON_FAST_REWIND);
            set_button_icon(2, ICON_PLAY);
            set_button_icon(3, ICON_FAST_FORWARD);
            set_button_icon(4, ICON_LIST);
            bind_button(0, [this]() { switch_page(UiPage::Home); });
            bind_button(1, [this]() { prev_file(); });
            bind_button(2, [this]() { start_playback(); });
            bind_button(3, [this]() { next_file(); });
            bind_button(4, [this]() { switch_page(UiPage::Home); });
            break;
        case UiPage::Playback:
            set_button_icon(0, ICON_EXIT);
            set_button_icon(1, ICON_SPEED, LV_OPA_40);
            set_button_icon(2, playback_paused_ ? ICON_PLAY : ICON_PAUSE);
            set_button_icon(3, ICON_FAST_REWIND, LV_OPA_40);
            set_button_icon(4, ICON_FAST_FORWARD, LV_OPA_40);
            bind_button(0, [this]() { stop_playback(UiPage::FileList); });
            bind_button(1, []() {});
            bind_button(2, [this]() { toggle_play_pause(); });
            bind_button(3, []() {});
            bind_button(4, []() {});
            break;
        case UiPage::Count:
            break;
        }
    }

    void on_home_back()
    {
        if (state_ == RecState::Recording)
        {
            stop_recording_to_save();
            return;
        }
        if (navigate_home)
            navigate_home();
    }

    void toggle_record()
    {
        if (state_ == RecState::Recording)
        {
            stop_recording_to_save();
            return;
        }

        stop_process(false);
        current_file_ = files_.generate_filename();
        pending_save_name_ = RecorderFileStore::filename_only(current_file_);
        page_home_.reset_waveform();
        elapsed_sec_ = 0;
        audio_.set_waveform_enabled(true);
        audio_.start_capture(make_audio_callback([this](int code, std::string data) {
            (void)data;
            if (code != 0)
            {
                state_ = RecState::Idle;
                refresh_view();
                bind_buttons_for_page();
                return;
            }
            state_ = RecState::Recording;
            start_elapsed_timer();
            switch_page(UiPage::Home);
        }));
    }

    void stop_recording_to_save()
    {
        if (state_ != RecState::Recording || audio_requesting_)
            return;

        audio_requesting_ = true;
        audio_.set_waveform_enabled(false);
        audio_.stop_capture(make_audio_callback([this](int code, std::string data) {
            (void)data;
            audio_requesting_ = false;
            if (code != 0)
                return;
            state_ = RecState::Idle;
            stop_elapsed_timer();
            page_save_.set_name(pending_save_name_);
            switch_page(UiPage::SaveConfirm);
        }));
    }

    void discard_pending_recording()
    {
        std::remove("/tmp/rec.tmp.wav");
        current_file_.clear();
        pending_save_name_.clear();
        switch_page(UiPage::Home);
    }

    void save_pending_recording()
    {
        std::string name = files_.normalized_wav_name(page_save_.name());
        if (name.empty())
            name = pending_save_name_;

        std::string path = files_.recordings_dir() + "/" + name;
        current_file_ = path;
        audio_.save_capture_file(path, make_audio_callback([this, name](int code, std::string data) {
            (void)data;
            if (code == 0)
            {
                scan_files();
                select_file(name);
                pending_save_name_.clear();
                switch_page(UiPage::FileList);
            }
        }));
    }

    void bump_save_name(int delta)
    {
        save_name_serial_ = std::max(0, save_name_serial_ + delta);
        page_save_.set_name(files_.make_serial_name(save_name_serial_));
    }

    void reset_save_name()
    {
        page_save_.set_name(pending_save_name_);
    }

    void start_playback()
    {
        if (recordings_.empty())
            return;

        stop_process(false);
        current_file_ = files_.recordings_dir() + "/" + recordings_[selected_idx_];
        playback_paused_ = false;
        playback_finished_.store(false);
        elapsed_sec_ = 0;
        audio_.play_file(current_file_, make_audio_callback([this](int code, std::string data) {
            (void)data;
            if (code != 0)
                return;
            state_ = RecState::Playing;
            start_elapsed_timer();
            switch_page(UiPage::Playback);
        }));
    }

    void toggle_play_pause()
    {
        if (state_ != RecState::Playing)
            return;
        if (playback_paused_)
        {
            audio_.continue_playback();
            playback_paused_ = false;
            start_elapsed_timer();
        }
        else
        {
            audio_.pause_playback();
            playback_paused_ = true;
            stop_elapsed_timer();
        }
        bind_buttons_for_page();
        refresh_view();
    }

    void stop_playback(UiPage next)
    {
        if (state_ == RecState::Playing)
            audio_.stop_playback();
        state_ = RecState::Idle;
        playback_paused_ = false;
        stop_elapsed_timer();
        scan_files();
        switch_page(next);
    }

    void stop_process(bool refresh)
    {
        if (state_ == RecState::Recording)
        {
            audio_.set_waveform_enabled(false);
            audio_.stop_capture();
        }
        else if (state_ == RecState::Playing)
        {
            audio_.stop_playback();
        }

        state_ = RecState::Idle;
        playback_paused_ = false;
        stop_elapsed_timer();
        if (refresh)
            refresh_view();
    }

    void prev_file()
    {
        if (recordings_.empty())
            return;
        selected_idx_ = selected_idx_ > 0 ? selected_idx_ - 1 : static_cast<int>(recordings_.size()) - 1;
        refresh_view();
    }

    void next_file()
    {
        if (recordings_.empty())
            return;
        selected_idx_ = selected_idx_ < static_cast<int>(recordings_.size()) - 1 ? selected_idx_ + 1 : 0;
        refresh_view();
    }

    void refresh_view()
    {
        page_home_.refresh(state_ == RecState::Recording,
                           state_ == RecState::Recording ? RecorderFileStore::filename_only(current_file_) : current_home_file(),
                           format_time(elapsed_sec_),
                           sample_rate_text());
        page_files_.refresh(recordings_, selected_idx_);
        float progress = -1.0f;
        page_playback_.refresh(RecorderFileStore::filename_only(current_file_), format_time(elapsed_sec_), progress, playback_paused_);
    }

    void on_audio_status(int code, const std::string &data)
    {
        if (code == 0 && data.find("play over") != std::string::npos)
        {
            playback_finished_.store(true);
            return;
        }
        if (code == 1 && data.size() >= page_home_.waveform_byte_size())
            page_home_.set_waveform(data);
    }

    static void elapsed_timer_cb(lv_timer_t *t)
    {
        auto *self = static_cast<UIRecPage *>(lv_timer_get_user_data(t));
        if (!self)
            return;
        self->elapsed_sec_++;
        self->refresh_view();
    }

    static void poll_timer_cb(lv_timer_t *t)
    {
        auto *self = static_cast<UIRecPage *>(lv_timer_get_user_data(t));
        if (!self)
            return;
        if (self->playback_finished_.exchange(false) && self->state_ == RecState::Playing)
        {
            self->stop_playback(UiPage::FileList);
            return;
        }
        if (self->current_page_ == UiPage::Home || self->current_page_ == UiPage::Playback)
            self->refresh_view();
    }

    void start_elapsed_timer()
    {
        stop_elapsed_timer();
        elapsed_timer_ = lv_timer_create(&UIRecPage::elapsed_timer_cb, 1000, this);
    }

    void stop_elapsed_timer()
    {
        if (elapsed_timer_)
        {
            lv_timer_delete(elapsed_timer_);
            elapsed_timer_ = nullptr;
        }
    }

    std::string current_home_file() const
    {
        if (!recordings_.empty() && selected_idx_ >= 0 && selected_idx_ < static_cast<int>(recordings_.size()))
            return recordings_[selected_idx_];
        return "";
    }

    std::string sample_rate_text() const
    {
        return "48kHz";
    }

    std::string format_time(int seconds) const
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", seconds / 60, seconds % 60);
        return buf;
    }

    void scan_files()
    {
        recordings_ = files_.scan();
        if (selected_idx_ >= static_cast<int>(recordings_.size()))
            selected_idx_ = recordings_.empty() ? 0 : static_cast<int>(recordings_.size()) - 1;
    }

    void select_file(const std::string &name)
    {
        for (size_t i = 0; i < recordings_.size(); ++i)
        {
            if (recordings_[i] == name)
            {
                selected_idx_ = static_cast<int>(i);
                return;
            }
        }
    }

    std::array<lv_obj_t *, static_cast<size_t>(UiPage::Count)> pages_{};
    RecorderAudioClient audio_;
    RecorderFileStore files_;
    std::array<std::function<void()>, rec_ui2::kBtnCount> button_actions_{};
    RecHomeView page_home_;
    RecSaveConfirmView page_save_;
    RecFileListView page_files_;
    RecPlaybackView page_playback_;

    UiPage current_page_ = UiPage::Home;
    RecState state_ = RecState::Idle;
    std::vector<std::string> recordings_;
    int selected_idx_ = 0;
    std::string current_file_;
    std::string pending_save_name_;
    int save_name_serial_ = 1;
    int elapsed_sec_ = 0;
    bool playback_paused_ = false;
    bool audio_requesting_ = false;
    lv_timer_t *elapsed_timer_ = nullptr;
    lv_timer_t *poll_timer_ = nullptr;
    std::atomic<bool> playback_finished_{false};
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};
