#pragma once
#include "../ui_app_page.hpp"
#include "compat/input_keys.h"

// ============================================================
//  MFT Unified Racer launch page
//  Screen: 320 x 170 (ui_root 320x170)
//
//  Game summary:
//    MFT2023 Unified Racer is a small M5Unified/M5GFX racing demo.
//    It renders a pseudo-3D road with generated course textures,
//    fixed-point car physics, lap timing, map/speed panels, and
//    SDL2 desktop output. The desktop build runs locally and does
//    not require I2C synchronization with other devices.
//
//  Keys:
//    ESC   - return to home
//    ENTER - highlight the launch hint
// ============================================================
class UILovyanPage : public app_
{
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 170;

    static constexpr uint32_t COLOR_BG        = 0x070A12;
    static constexpr uint32_t COLOR_TOP       = 0x10192A;
    static constexpr uint32_t COLOR_PANEL     = 0x121B2D;
    static constexpr uint32_t COLOR_PANEL_2   = 0x0B1220;
    static constexpr uint32_t COLOR_TEXT      = 0xEAF2FF;
    static constexpr uint32_t COLOR_MUTED     = 0x91A6C6;
    static constexpr uint32_t COLOR_ACCENT    = 0x34D399;
    static constexpr uint32_t COLOR_WARN      = 0xFBBF24;
    static constexpr uint32_t COLOR_RED       = 0xEF4444;
    static constexpr uint32_t COLOR_ROAD      = 0x24293A;
    static constexpr uint32_t COLOR_ROAD_EDGE = 0xF8FAFC;
    static constexpr uint32_t COLOR_GRASS     = 0x14532D;

    lv_obj_t *bg_ = nullptr;
    lv_obj_t *hint_label_ = nullptr;

public:
    UILovyanPage() : app_()
    {
        creat_UI();
        event_handler_init();
    }
    ~UILovyanPage()
    {
        // mft2023_unified_racer;
        system("kill -9  `ps aux | grep mft2023 | grep -v grep | awk '{print $2}'`");
    }

private:
    void creat_UI()
    {
        bg_ = lv_obj_create(ui_root);
        lv_obj_set_size(bg_, SCREEN_W, SCREEN_H);
        lv_obj_set_pos(bg_, 0, 0);
        lv_obj_set_style_radius(bg_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg_, lv_color_hex(COLOR_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg_, LV_OBJ_FLAG_SCROLLABLE);

        create_header();
        create_track_preview();
        create_summary_panel();
        create_footer();
    }

    void create_header()
    {
        lv_obj_t *bar = make_rect(bg_, 0, 0, SCREEN_W, 26, COLOR_TOP, 0);
        lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(bar, lv_color_hex(0x23314D), LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *title = make_label(bar, 8, 4, "MFT Unified Racer", COLOR_TEXT, &lv_font_montserrat_14);
        lv_obj_set_width(title, 190);

        lv_obj_t *tag = make_rect(bar, 235, 5, 76, 16, 0x1E293B, 8);
        lv_obj_set_style_border_width(tag, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(tag, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_t *tag_text = make_label(tag, 0, 1, "SDL2 READY", COLOR_ACCENT, &lv_font_montserrat_10);
        lv_obj_set_width(tag_text, 76);
        lv_obj_set_style_text_align(tag_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void create_track_preview()
    {
        lv_obj_t *stage = make_rect(bg_, 8, 34, 160, 104, COLOR_GRASS, 8);
        lv_obj_set_style_border_width(stage, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(stage, lv_color_hex(0x1F7A3E), LV_PART_MAIN | LV_STATE_DEFAULT);

        make_rect(stage, 0, 0, 160, 24, 0x2563EB, 8);
        make_rect(stage, 0, 18, 160, 24, 0x1D4ED8, 0);
        make_rect(stage, 0, 40, 160, 16, 0x166534, 0);

        make_rect(stage, 54, 36, 52, 68, COLOR_ROAD, 0);
        make_rect(stage, 36, 54, 88, 18, COLOR_ROAD, 0);
        make_rect(stage, 24, 72, 112, 16, COLOR_ROAD, 0);
        make_rect(stage, 12, 88, 136, 16, COLOR_ROAD, 0);

        make_rect(stage, 50, 36, 4, 68, COLOR_ROAD_EDGE, 0);
        make_rect(stage, 106, 36, 4, 68, COLOR_ROAD_EDGE, 0);
        make_rect(stage, 78, 42, 4, 10, COLOR_WARN, 1);
        make_rect(stage, 78, 60, 4, 12, COLOR_WARN, 1);
        make_rect(stage, 78, 82, 4, 16, COLOR_WARN, 1);

        create_car(stage, 67, 73);

        make_rect(stage, 10, 10, 20, 4, COLOR_TEXT, 1);
        make_rect(stage, 10, 16, 20, 4, 0x111827, 1);
        make_rect(stage, 130, 10, 20, 4, COLOR_TEXT, 1);
        make_rect(stage, 130, 16, 20, 4, 0x111827, 1);
    }

    void create_summary_panel()
    {
        lv_obj_t *panel = make_rect(bg_, 176, 34, 136, 104, COLOR_PANEL, 8);
        lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(panel, lv_color_hex(0x26344F), LV_PART_MAIN | LV_STATE_DEFAULT);

        make_label(panel, 10, 8, "Racing Demo", COLOR_TEXT, &lv_font_montserrat_14);

        make_badge(panel, 10, 31, "Pseudo-3D road", COLOR_ACCENT);
        make_badge(panel, 10, 51, "Drift physics", COLOR_WARN);
        make_badge(panel, 10, 71, "Lap + map HUD", 0x60A5FA);

        lv_obj_t *note = make_label(panel, 10, 91, "Local SDL2 mode, no sync devices", COLOR_MUTED, &lv_font_montserrat_10);
        lv_obj_set_width(note, 116);
        lv_label_set_long_mode(note, LV_LABEL_LONG_SCROLL_CIRCULAR);
    }

    void create_footer()
    {
        lv_obj_t *footer = make_rect(bg_, 8, 144, 304, 18, COLOR_PANEL_2, 6);
        lv_obj_set_style_border_width(footer, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(footer, lv_color_hex(0x1E293B), LV_PART_MAIN | LV_STATE_DEFAULT);

        hint_label_ = make_label(footer, 9, 2, "OK: launch package   ESC: home", COLOR_MUTED, &lv_font_montserrat_10);
        lv_obj_set_width(hint_label_, 286);
        lv_obj_set_style_text_align(hint_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void create_car(lv_obj_t *parent, int x, int y)
    {
        make_rect(parent, x + 5, y, 16, 6, COLOR_RED, 2);
        make_rect(parent, x, y + 5, 26, 12, COLOR_RED, 3);
        make_rect(parent, x + 8, y + 2, 10, 5, 0x93C5FD, 2);
        make_rect(parent, x + 3, y + 15, 6, 4, 0x020617, 2);
        make_rect(parent, x + 17, y + 15, 6, 4, 0x020617, 2);
        make_rect(parent, x + 11, y + 17, 4, 9, 0xF97316, 2);
    }

    void make_badge(lv_obj_t *parent, int x, int y, const char *text, uint32_t color)
    {
        lv_obj_t *dot = make_rect(parent, x, y + 4, 6, 6, color, 3);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *label = make_label(parent, x + 12, y, text, COLOR_TEXT, &lv_font_montserrat_10);
        lv_obj_set_width(label, 110);
    }

    lv_obj_t *make_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
    {
        lv_obj_t *obj = lv_obj_create(parent);
        lv_obj_set_size(obj, w, h);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_style_radius(obj, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
        return obj;
    }

    lv_obj_t *make_label(lv_obj_t *parent, int x, int y, const char *text, uint32_t color, const lv_font_t *font)
    {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, text);
        lv_obj_set_pos(label, x, y);
        lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
        return label;
    }

    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UILovyanPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UILovyanPage *self = static_cast<UILovyanPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }
    int asdasdas = 0;
    void event_handler(lv_event_t *e)
    {
        if (!IS_KEY_RELEASED(e)) return;

        uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
        if (key == KEY_ESC) {
            if (go_back_home) go_back_home();
            return;
        }
        if (key == KEY_ENTER)
        {
            if (asdasdas == 0) {
                asdasdas = 1;
                system("/home/pi/start_mft2023_unified_racer.sh &");
            }
        }

        if (key == KEY_ENTER && hint_label_) {
            lv_label_set_text(hint_label_, "Package ready: run Linux SDL2 build on target");
            lv_obj_set_style_text_color(hint_label_, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
};
