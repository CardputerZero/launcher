#pragma once
#if !defined(HAL_PLATFORM_SDL)
#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#ifndef LAUNCHER_GIT_COMMIT_RAW
#define LAUNCHER_GIT_COMMIT_RAW unknown
#endif
#define LAUNCHER_GIT_COMMIT STRINGIFY(LAUNCHER_GIT_COMMIT_RAW)
#include "../ui_app_page.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <dirent.h>
#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#endif
#include "hal/hal_settings.h"
#include "hal/hal_process.h"
#include "hal/hal_config.h"

// ============================================================
//  系统设置界面  UISetupPage  (Carousel Design)
//  屏幕: 320x170 (顶栏20px, body 320x150)
//
//  菜单项 (设计稿): Launcher, Boot, Screen, WiFi, Speaker, Camera
//  实际对接 HAL: WiFi scan/connect, brightness, volume, power, reboot, about
// ============================================================

class UISetupPage : public app_base
{
    enum class ViewState { MAIN, SUB, VALUE_SELECT, WIFI_PW };

    struct SubItem {
        std::string label;
        bool is_toggle;
        bool toggle_state;
        std::function<void()> action;
    };

    struct MenuItem {
        std::string label;
        std::vector<SubItem> sub_items;
        std::function<void()> on_enter;
        std::function<void(uint32_t key)> custom_key_handler;
    };

public:
    UISetupPage() : app_base()
    {
        set_page_title("SETTING");
        cache_image_paths();
        menu_init();
        create_ui();
        event_handler_init();
    }
    ~UISetupPage() { stop_power_timer(); }

private:
    std::vector<MenuItem> menu_items_;
    int selected_idx_ = 2;
    int sub_selected_idx_ = 0;
    ViewState view_state_ = ViewState::MAIN;
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;

    // Image paths
    std::string img_arrow_up_;
    std::string img_arrow_down_;
    std::string img_right_arrow_;
    std::string img_ok_;
    std::string img_cross_;

    // WiFi state
    hal_wifi_ap_t wifi_aps_[WIFI_AP_MAX];
    int wifi_ap_count_ = 0;
    std::string wifi_pw_ssid_;
    std::string wifi_pw_buf_;
    lv_obj_t *pw_input_lbl_ = nullptr;
    lv_obj_t *pw_hint_lbl_ = nullptr;
    struct key_item *cur_elm_ = nullptr;

    // Brightness
    int bright_val_ = 75;

    // Value select (3rd level)
    int val_sel_idx_ = 0;
    std::vector<std::string> val_options_;
    std::string val_title_;

    // Volume
    int vol_val_ = 39;

    // Power timer
    lv_timer_t *pwr_timer_ = nullptr;

    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 150;
    static constexpr int LIST_H   = SCREEN_H;
    static constexpr int ROWS_VISIBLE = 7;
    static constexpr int ROW_CENTER   = 3;

    void cache_image_paths()
    {
        img_arrow_up_    = img_path("setting_red_up.png");
        img_arrow_down_  = img_path("setting_red_down.png");
        img_right_arrow_ = img_path("setting_right_arrow.png");
        img_ok_          = img_path("setting_ok.png");
        img_cross_       = img_path("setting_cross.png");
    }

    // ==================== Menu init ====================
    void menu_init()
    {
        // --- Launcher (app enable/disable, OX toggle) ---
        {
            MenuItem m;
            m.label = "Launcher";
            m.sub_items = {
                {"Python",   true, true, nullptr},
                {"Store",    true, true, nullptr},   // always enabled (enforced)
                {"CLI",      true, true, nullptr},   // always enabled (enforced)
                {"CLAW",     true, true, nullptr},
                {"Setting",  true, true, nullptr},   // always enabled (enforced)
                {"Music",    true, true, nullptr},
                {"Audio",    true, true, nullptr},
                {"Hack",     true, true, nullptr},
                {"Game",     true, true, nullptr},
                {"Math",     true, true, nullptr},
                {"IP Panel", true, true, nullptr},
                {"Stocks",   true, true, nullptr},
                {"Chat",     true, true, nullptr},
                {"e-Mail",   true, true, nullptr},
                {"File",     true, true, nullptr},
                {"AICli",    true, true, nullptr},
                {"SSH",      true, true, nullptr},
                {"Mesh",     true, true, nullptr},
                {"Rec",      true, true, nullptr},
                {"Camera",   true, true, nullptr},
                {"UnitEnv",  true, true, nullptr},
                {"Midi",     true, true, nullptr},
                {"Gpio",     true, true, nullptr},
                {"LoRa",     true, true, nullptr},
                {"Gallery",  true, true, nullptr},
                {"HikePod",  true, true, nullptr},
                {"Tank",     true, true, nullptr},
            };
            menu_items_.push_back(m);
        }
        // --- Boot ---
        {
            MenuItem m;
            m.label = "Boot";
            m.sub_items = {
                {"Startup",  false, false, nullptr},  // 3rd: Launcher / CLI
                {"Reboot",   false, false, [this]() { hal_system_reboot(); }},
                {"Shutdown", false, false, [this]() { hal_system_shutdown(); }},
            };
            menu_items_.push_back(m);
        }
        // --- Screen ---
        {
            MenuItem m;
            m.label = "Screen";
            m.sub_items = {
                {"Brightness", false, false, [this]() { enter_brightness_adjust(); }},
                {"DarkTime",   false, false, [this]() { enter_darktime_adjust(); }},
            };
            menu_items_.push_back(m);
        }
        // --- WiFi ---
        {
            MenuItem m;
            m.label = "WiFi";
            m.sub_items = {
                {"reset",   false, false, [this]() { hal_wifi_disconnect(); rebuild_view(); }},
                {"Scan",    false, false, [this]() { enter_wifi_scan(); }},
                {"Enable",  true, true,  [this]() { wifi_toggle_enable(); }},
            };
            menu_items_.push_back(m);
        }
        // --- Speaker ---
        {
            MenuItem m;
            m.label = "Speaker";
            m.sub_items = {
                {"Volume", false, false, [this]() { enter_volume_adjust(); }},
            };
            menu_items_.push_back(m);
        }
        // --- Camera ---
        {
            MenuItem m;
            m.label = "Camera";
            m.sub_items = {
                {"Resolution", false, false, [this]() { enter_camera_resolution(); }},
            };
            menu_items_.push_back(m);
        }
        // --- Info (display only — values refreshed on enter) ---
        {
            MenuItem m;
            m.label = "Info";
            m.sub_items = {
                {"Battery: --%",     false, false, nullptr},
                {"Temp: --C",        false, false, nullptr},
                {"Current: --mA",    false, false, nullptr},
            };
            m.on_enter = [this]() { refresh_info_values(); };
            menu_items_.push_back(m);
        }
        // --- ExtPort ---
        {
            MenuItem m;
            m.label = "ExtPort";
            m.sub_items = {
                {"USB",   true, true, nullptr},   // Enable/Disable toggle
                {"5VOUT", true, true, nullptr},   // Enable/Disable toggle
            };
            menu_items_.push_back(m);
        }
        // --- RTC ---
        {
            MenuItem m;
            m.label = "RTC";
            m.sub_items = {
                {"Year",   false, false, nullptr},  // +/- adjust
                {"Month",  false, false, nullptr},
                {"Day",    false, false, nullptr},
                {"Hour",   false, false, nullptr},
                {"Minute", false, false, nullptr},
                {"Second", false, false, nullptr},
            };
            menu_items_.push_back(m);
        }
        // --- Reset ---
        {
            MenuItem m;
            m.label = "Reset";
            m.sub_items = {
                {"Factory Reset", false, false, [this]() { factory_reset(); }},
            };
            menu_items_.push_back(m);
        }
    }

    // ==================== Placeholder functions for new menus ====================
    void enter_darktime_adjust()
    {
        val_title_ = "DarkTime";
        val_options_ = {"Never", "10S", "30S", "60S", "300S"};
        val_sel_idx_ = 2; // default 30S
        view_state_ = ViewState::VALUE_SELECT;
        build_value_view();
    }

    void enter_volume_adjust()
    {
        val_title_ = "Volume";
        val_options_ = {"100%", "75%", "50%", "25%", "0%"};
        vol_val_ = hal_config_get_int("volume", hal_volume_read());
        int pct = vol_val_ * 100 / 63;
        if (pct >= 87) val_sel_idx_ = 0;
        else if (pct >= 62) val_sel_idx_ = 1;
        else if (pct >= 37) val_sel_idx_ = 2;
        else if (pct >= 12) val_sel_idx_ = 3;
        else val_sel_idx_ = 4;
        view_state_ = ViewState::VALUE_SELECT;
        build_value_view();
    }

    void enter_camera_resolution()
    {
        val_title_ = "Resolution";
        val_options_ = {"1280x720", "640x480"};
        val_sel_idx_ = 0;
        view_state_ = ViewState::VALUE_SELECT;
        build_value_view();
    }

    void enter_wifi_scan()
    {
        // TODO: PSP-style WiFi scan list page
        wifi_do_scan();
        rebuild_view();
    }

    void refresh_info_values()
    {
        // Find the Info menu item and update its sub_items labels
        for (auto &m : menu_items_) {
            if (m.label != "Info") continue;
            hal_battery_info_t bat = hal_battery_read();
            char buf[64];
            snprintf(buf, sizeof(buf), "Battery: %d%%", bat.valid ? bat.soc : 0);
            m.sub_items[0].label = buf;
            snprintf(buf, sizeof(buf), "Temp: %.1fC", bat.valid ? bat.temperature_c10 / 10.0 : 0.0);
            m.sub_items[1].label = buf;
            snprintf(buf, sizeof(buf), "Current: %dmA", bat.valid ? bat.current_ma : 0);
            m.sub_items[2].label = buf;
            break;
        }
    }

    void factory_reset()
    {
        // TODO: delete settings.json and reload defaults
        // remove("/path/to/settings.json");
        // reload_settings();
    }

    // ==================== WiFi functions ====================
    void wifi_do_scan()
    {
        wifi_ap_count_ = hal_wifi_scan(wifi_aps_, WIFI_AP_MAX);
    }

    void wifi_toggle_enable()
    {
        // Toggle is handled by the generic sub_key handler (toggle_state flip)
        // TODO: actual wifi enable/disable HAL call
    }

    void handle_wifi_custom_key(uint32_t key)
    {
        (void)key;
    }

    void wifi_try_connect(int idx)
    {
        if (idx < 0 || idx >= wifi_ap_count_) return;
        hal_wifi_ap_t *ap = &wifi_aps_[idx];
        if (ap->in_use) return;

        if (strcmp(ap->security, "Open") == 0 || ap->security[0] == 0) {
            hal_wifi_connect(ap->ssid, NULL);
        } else {
            wifi_pw_ssid_ = ap->ssid;
            wifi_pw_buf_.clear();
            show_wifi_pw_input();
        }
    }

    void show_wifi_pw_input()
    {
        view_state_ = ViewState::WIFI_PW;
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);

        lv_obj_t *title = lv_label_create(cont);
        char buf[128];
        snprintf(buf, sizeof(buf), "Connect: %s", wifi_pw_ssid_.c_str());
        lv_label_set_text(title, buf);
        lv_obj_set_pos(title, 10, 10);
        lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_t *pw_label = lv_label_create(cont);
        lv_label_set_text(pw_label, "Password:");
        lv_obj_set_pos(pw_label, 10, 35);
        lv_obj_set_style_text_color(pw_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(pw_label, &lv_font_montserrat_12, LV_PART_MAIN);

        pw_input_lbl_ = lv_label_create(cont);
        lv_label_set_text(pw_input_lbl_, "_");
        lv_obj_set_pos(pw_input_lbl_, 90, 35);
        lv_obj_set_width(pw_input_lbl_, 200);
        lv_label_set_long_mode(pw_input_lbl_, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(pw_input_lbl_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(pw_input_lbl_, &lv_font_montserrat_14, LV_PART_MAIN);

        pw_hint_lbl_ = lv_label_create(cont);
        lv_label_set_text(pw_hint_lbl_, "Type pw, OK:connect, ESC:cancel");
        lv_obj_set_pos(pw_hint_lbl_, 10, 65);
        lv_obj_set_style_text_color(pw_hint_lbl_, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_text_font(pw_hint_lbl_, &lv_font_montserrat_10, LV_PART_MAIN);
    }

    void handle_wifi_pw_key(uint32_t key)
    {
        if (key == KEY_ESC) {
            view_state_ = ViewState::SUB;
            rebuild_view();
            return;
        }
        if (key == KEY_ENTER) {
            if (pw_hint_lbl_) lv_label_set_text(pw_hint_lbl_, "Connecting...");
            lv_refr_now(NULL);
            hal_wifi_connect(wifi_pw_ssid_.c_str(), wifi_pw_buf_.c_str());
            view_state_ = ViewState::SUB;
            rebuild_view();
            return;
        }
        if (key == KEY_BACKSPACE) {
            if (!wifi_pw_buf_.empty()) wifi_pw_buf_.pop_back();
            wifi_pw_update_display();
            return;
        }
        if (cur_elm_ && cur_elm_->utf8[0]) {
            wifi_pw_buf_ += cur_elm_->utf8;
            wifi_pw_update_display();
        }
    }

    void wifi_pw_update_display()
    {
        if (!pw_input_lbl_) return;
        std::string display = wifi_pw_buf_ + "_";
        lv_label_set_text(pw_input_lbl_, display.c_str());
    }

    // ==================== Volume (via value select) ====================
    void apply_volume()
    {
        int pcts[] = {100, 75, 50, 25, 0};
        int new_val = 63 * pcts[val_sel_idx_] / 100;
        hal_volume_write(new_val);
        hal_config_set_int("volume", new_val);
        hal_config_save();
    }

    // ==================== Brightness ====================
    void enter_brightness_adjust()
    {
        val_title_ = "Brightness";
        val_options_ = {"100%", "75%", "50%", "25%"};
        bright_val_ = hal_backlight_read();
        int mx = hal_backlight_max();
        int pct = mx > 0 ? bright_val_ * 100 / mx : 100;
        if (pct >= 87) val_sel_idx_ = 0;
        else if (pct >= 62) val_sel_idx_ = 1;
        else if (pct >= 37) val_sel_idx_ = 2;
        else val_sel_idx_ = 3;
        view_state_ = ViewState::VALUE_SELECT;
        build_value_view();
    }

    void apply_value_selection()
    {
        if (val_title_ == "Brightness") {
            int mx = hal_backlight_max();
            int pcts[] = {100, 75, 50, 25};
            int new_val = mx * pcts[val_sel_idx_] / 100;
            if (new_val < 1) new_val = 1;
            hal_backlight_write(new_val);
            hal_config_set_int("brightness", new_val);
            hal_config_save();
        } else if (val_title_ == "Volume") {
            apply_volume();
        } else if (val_title_ == "DarkTime") {
            // TODO: save dark time setting
            int times[] = {0, 10, 30, 60, 300};
            hal_config_set_int("dark_time", times[val_sel_idx_]);
            hal_config_save();
        } else if (val_title_ == "Resolution") {
            // TODO: save camera resolution
            hal_config_set_int("cam_resolution", val_sel_idx_);
            hal_config_save();
        }
    }

    // ==================== Power timer ====================
    void stop_power_timer()
    {
        if (pwr_timer_) { lv_timer_delete(pwr_timer_); pwr_timer_ = nullptr; }
    }

    // ==================== UI ====================
    void create_ui()
    {
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, SCREEN_W, SCREEN_H);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        // List container (full area — title is handled by system status bar)
        lv_obj_t *list_cont = lv_obj_create(bg);
        lv_obj_set_size(list_cont, SCREEN_W, LIST_H);
        lv_obj_set_pos(list_cont, 0, 0);
        lv_obj_set_style_radius(list_cont, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(list_cont, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(list_cont, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(list_cont, 0, LV_PART_MAIN);
        lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["list_cont"] = list_cont;

        build_main_view();
    }

    // ==================== Animation config ====================
    static constexpr int ANIM_TIME = 200;
    bool is_animating_ = false;
    lv_obj_t *row_labels_[ROWS_VISIBLE] = {};
    lv_obj_t *sel_bg_ = nullptr;
    lv_obj_t *hint_lbl_ = nullptr;
    lv_obj_t *arrow_up_obj_ = nullptr;
    lv_obj_t *arrow_down_obj_ = nullptr;

    int row_h() { return LIST_H / ROWS_VISIBLE; }
    int row_y(int vi) {
        // Center row (ROW_CENTER) must be vertically centered in the 150px area
        int center_y = (LIST_H - row_h()) / 2;
        return center_y + (vi - ROW_CENTER) * row_h();
    }

    struct RowStyle {
        const lv_font_t *font;
        uint32_t color;
        int x;
        int opa;
    };

    static constexpr int MENU_X = 60;

    RowStyle style_for_slot(int vi) {
        int dist = vi > ROW_CENTER ? vi - ROW_CENTER : ROW_CENTER - vi;
        if (dist == 0)
            return {g_font_bold_20 ? g_font_bold_20 : &lv_font_montserrat_20, 0xFFFFFF, MENU_X, 255};
        if (dist == 1)
            return {g_font_bold_14 ? g_font_bold_14 : &lv_font_montserrat_16, 0xAAAAAA, MENU_X, 220};
        if (dist == 2)
            return {g_font_bold_12 ? g_font_bold_12 : &lv_font_montserrat_14, 0x777777, MENU_X, 170};
        return {&lv_font_montserrat_12, 0x555555, MENU_X, 130};
    }

    // ==================== Shared: create a styled carousel label ====================
    // vi = visual slot (0..ROWS_VISIBLE-1), center_vi = which slot is "selected"
    // center_x = the pixel X where text center aligns
    // text = label string, hide if empty
    // smaller = true for sub-menu columns (one font size smaller)
    lv_obj_t *create_carousel_label(lv_obj_t *parent, int vi, int center_vi,
                                     const char *text, int center_x, bool smaller = false)
    {
        int dist = vi > center_vi ? vi - center_vi : center_vi - vi;
        const lv_font_t *font;
        uint32_t color;
        int opa;
        if (!smaller) {
            if (dist == 0) {
                font = g_font_bold_20 ? g_font_bold_20 : &lv_font_montserrat_18;
                color = 0xFFFFFF; opa = 255;
            } else if (dist == 1) {
                font = g_font_bold_14 ? g_font_bold_14 : &lv_font_montserrat_16;
                color = 0xAAAAAA; opa = 220;
            } else if (dist == 2) {
                font = g_font_bold_12 ? g_font_bold_12 : &lv_font_montserrat_14;
                color = 0x777777; opa = 170;
            } else {
                font = &lv_font_montserrat_12;
                color = 0x555555; opa = 130;
            }
        } else {
            // Smaller variant for sub-menu / right column
            if (dist == 0) {
                font = g_font_bold_14 ? g_font_bold_14 : &lv_font_montserrat_16;
                color = 0xFFFFFF; opa = 255;
            } else if (dist == 1) {
                font = g_font_bold_12 ? g_font_bold_12 : &lv_font_montserrat_14;
                color = 0xAAAAAA; opa = 220;
            } else if (dist == 2) {
                font = &lv_font_montserrat_12;
                color = 0x777777; opa = 170;
            } else {
                font = &lv_font_montserrat_10;
                color = 0x555555; opa = 130;
            }
        }

        lv_obj_t *lbl = lv_label_create(parent);
        lv_label_set_text(lbl, text ? text : "");
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
        lv_obj_set_style_opa(lbl, opa, LV_PART_MAIN);

        lv_obj_update_layout(lbl);
        int tw = lv_obj_get_width(lbl);
        int lx = center_x - tw / 2;
        if (lx < 4) lx = 4;
        int font_h = lv_font_get_line_height(font);
        lv_obj_set_pos(lbl, lx, row_y(vi) + (row_h() - font_h) / 2);

        if (!text || !text[0])
            lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
        return lbl;
    }

    static constexpr int ARROW_W = 18;

    // Place blue arrow between left_lbl and right_lbl (only at center row)
    void place_blue_arrow(lv_obj_t *parent, lv_obj_t *left_lbl, lv_obj_t *right_lbl)
    {
        if (!left_lbl || !right_lbl) return;
        lv_obj_update_layout(left_lbl);
        lv_obj_update_layout(right_lbl);

        int left_right_edge = lv_obj_get_x(left_lbl) + lv_obj_get_width(left_lbl);
        int center_left_edge = lv_obj_get_x(right_lbl);
        int gap = center_left_edge - left_right_edge;

        int arrow_x;
        if (gap >= ARROW_W) {
            arrow_x = left_right_edge + (gap - ARROW_W) / 2;
        } else {
            arrow_x = center_left_edge - ARROW_W;
        }
        if (arrow_x < left_right_edge + 2) arrow_x = left_right_edge + 2;

        // Vertically center the arrow (19px tall) within the row
        static constexpr int ARROW_H = 19;
        int arrow_y = row_y(ROW_CENTER) + (row_h() - ARROW_H) / 2;

        lv_obj_t *arrow = lv_img_create(parent);
        lv_img_set_src(arrow, img_right_arrow_.c_str());
        lv_obj_set_pos(arrow, arrow_x, arrow_y);
    }

    // Convenience: create a main-menu label at slot vi
    lv_obj_t *create_menu_label(lv_obj_t *parent, int vi, int item_idx, int count)
    {
        const char *text = (item_idx >= 0 && item_idx < count)
            ? menu_items_[item_idx].label.c_str() : "";
        lv_obj_t *lbl = create_carousel_label(parent, vi, ROW_CENTER, text, MENU_X);
        if (item_idx < 0 || item_idx >= count)
            lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
        return lbl;
    }

    // ==================== Main carousel view ====================
    void build_main_view()
    {
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);

        int count = (int)menu_items_.size();

        // Selected item background (312px wide, 22px tall, gray, no radius)
        static constexpr int SEL_BAR_H = 22;
        static constexpr int SEL_BAR_W = 312;
        sel_bg_ = lv_obj_create(cont);
        lv_obj_set_size(sel_bg_, SEL_BAR_W, SEL_BAR_H);
        lv_obj_set_pos(sel_bg_, (SCREEN_W - SEL_BAR_W) / 2, row_y(ROW_CENTER) + (row_h() - SEL_BAR_H) / 2);
        lv_obj_set_style_radius(sel_bg_, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sel_bg_, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sel_bg_, 255, LV_PART_MAIN);
        lv_obj_set_style_border_width(sel_bg_, 0, LV_PART_MAIN);
        lv_obj_clear_flag(sel_bg_, LV_OBJ_FLAG_SCROLLABLE);

        // Hint label (right side of selected bar)
        hint_lbl_ = lv_label_create(cont);
        lv_label_set_text(hint_lbl_, "ok:enter");
        lv_obj_set_pos(hint_lbl_, SCREEN_W - 80, row_y(ROW_CENTER) + (row_h() - 14) / 2);
        lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0x00CC66), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint_lbl_, &lv_font_montserrat_12, LV_PART_MAIN);

        // Row labels
        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            int item_idx = selected_idx_ - ROW_CENTER + vi;
            row_labels_[vi] = create_menu_label(cont, vi, item_idx, count);
        }

        // Arrows created last (on top), centered at x=MENU_X (arrow 16px wide)
        int arrow_x = MENU_X - 8;
        arrow_up_obj_ = lv_img_create(cont);
        lv_img_set_src(arrow_up_obj_, img_arrow_up_.c_str());
        lv_obj_set_pos(arrow_up_obj_, arrow_x, 2);
        if (selected_idx_ <= 0) lv_obj_add_flag(arrow_up_obj_, LV_OBJ_FLAG_HIDDEN);

        arrow_down_obj_ = lv_img_create(cont);
        lv_img_set_src(arrow_down_obj_, img_arrow_down_.c_str());
        lv_obj_set_pos(arrow_down_obj_, arrow_x, LIST_H - 14);
        if (selected_idx_ >= count - 1) lv_obj_add_flag(arrow_down_obj_, LV_OBJ_FLAG_HIDDEN);

        is_animating_ = false;
    }

    // Animate scroll: direction = -1 (up) or +1 (down)
    void animate_scroll(int direction)
    {
        int count = (int)menu_items_.size();
        int new_idx = selected_idx_ + direction;
        if (new_idx < 0 || new_idx >= count) return;
        if (is_animating_) return;

        is_animating_ = true;
        selected_idx_ = new_idx;

        // Update arrow visibility
        if (arrow_up_obj_) {
            if (selected_idx_ > 0) lv_obj_clear_flag(arrow_up_obj_, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(arrow_up_obj_, LV_OBJ_FLAG_HIDDEN);
        }
        if (arrow_down_obj_) {
            if (selected_idx_ < count - 1) lv_obj_clear_flag(arrow_down_obj_, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(arrow_down_obj_, LV_OBJ_FLAG_HIDDEN);
        }

        // Update text content for each slot
        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            int item_idx = selected_idx_ - ROW_CENTER + vi;
            if (item_idx >= 0 && item_idx < count) {
                lv_label_set_text(row_labels_[vi], menu_items_[item_idx].label.c_str());
                lv_obj_clear_flag(row_labels_[vi], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_label_set_text(row_labels_[vi], "");
                lv_obj_add_flag(row_labels_[vi], LV_OBJ_FLAG_HIDDEN);
            }
        }

        // Animate each label's Y position
        int rh = row_h();
        int offset = direction * rh;
        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            RowStyle s = style_for_slot(vi);
            int font_h = lv_font_get_line_height(s.font);
            int target_y = row_y(vi) + (rh - font_h) / 2;
            int start_y = target_y + offset;

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, row_labels_[vi]);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
            lv_anim_set_values(&a, start_y, target_y);
            lv_anim_set_time(&a, ANIM_TIME);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            if (vi == ROW_CENTER) {
                lv_anim_set_completed_cb(&a, anim_done_cb);
                lv_anim_set_user_data(&a, this);
            }
            lv_anim_start(&a);

            // Update style (font/color/opacity) and re-center X
            lv_obj_set_style_text_color(row_labels_[vi], lv_color_hex(s.color), LV_PART_MAIN);
            lv_obj_set_style_text_font(row_labels_[vi], s.font, LV_PART_MAIN);
            lv_obj_set_style_opa(row_labels_[vi], s.opa, LV_PART_MAIN);
            lv_obj_update_layout(row_labels_[vi]);
            int tw = lv_obj_get_width(row_labels_[vi]);
            int lx = MENU_X - tw / 2;
            if (lx < 4) lx = 4;
            lv_obj_set_x(row_labels_[vi], lx);
        }
    }

    static void anim_done_cb(lv_anim_t *a)
    {
        UISetupPage *self = (UISetupPage *)lv_anim_get_user_data(a);
        if (self) self->is_animating_ = false;
    }

    // ==================== Sub view ====================
    void build_sub_view()
    {
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);

        MenuItem &item = menu_items_[selected_idx_];
        int sub_count = (int)item.sub_items.size();
        int count = (int)menu_items_.size();

        // Gray highlight bar (same as main view, behind everything)
        static constexpr int SEL_BAR_H = 22;
        static constexpr int SEL_BAR_W = 312;
        lv_obj_t *bar = lv_obj_create(cont);
        lv_obj_set_size(bar, SEL_BAR_W, SEL_BAR_H);
        lv_obj_set_pos(bar, (SCREEN_W - SEL_BAR_W) / 2, row_y(ROW_CENTER) + (row_h() - SEL_BAR_H) / 2);
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, 255, LV_PART_MAIN);
        lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        // Left column: reuse shared label creation
        lv_obj_t *left_center_lbl = nullptr;
        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            int item_idx = selected_idx_ - ROW_CENTER + vi;
            if (item_idx < 0 || item_idx >= count) continue;

            lv_obj_t *lbl = create_menu_label(cont, vi, item_idx, count);
            if (vi == ROW_CENTER) left_center_lbl = lbl;
        }

        // Right column: sub items (same carousel style, centered at x=160)
        static constexpr int SUB_CENTER_X = 160;

        if (sub_count == 0) {
            create_carousel_label(cont, ROW_CENTER, ROW_CENTER, "(no options)", SUB_CENTER_X, true);
            return;
        }

        // Default sub_selected to position ~3 if enough items
        int sub_center_vi = ROW_CENTER;

        // Sub items using shared carousel label
        lv_obj_t *right_center_lbl = nullptr;
        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            int si = sub_selected_idx_ - sub_center_vi + vi;
            if (si < 0 || si >= sub_count) continue;

            SubItem &sub = item.sub_items[si];
            lv_obj_t *lbl = create_carousel_label(cont, vi, sub_center_vi,
                                                   sub.label.c_str(), SUB_CENTER_X, true);
            if (vi == sub_center_vi) right_center_lbl = lbl;

            // Toggle indicator (to the right of text)
            if (sub.is_toggle) {
                lv_obj_update_layout(lbl);
                int tw = lv_obj_get_width(lbl);
                int lx = lv_obj_get_x(lbl);
                lv_obj_t *ind = lv_img_create(cont);
                lv_img_set_src(ind, sub.toggle_state ? img_ok_.c_str() : img_cross_.c_str());
                lv_obj_set_pos(ind, lx + tw + 6, lv_obj_get_y(lbl) + 2);
            }
        }

        // Blue arrow between left and right center labels
        if (left_center_lbl && right_center_lbl)
            place_blue_arrow(cont, left_center_lbl, right_center_lbl);

        // Up/down arrows for sub (centered at SUB_CENTER_X)
        int sub_arrow_x = SUB_CENTER_X - 8;
        if (sub_selected_idx_ > 0) {
            lv_obj_t *a = lv_img_create(cont);
            lv_img_set_src(a, img_arrow_up_.c_str());
            lv_obj_set_pos(a, sub_arrow_x, 2);
        }
        if (sub_selected_idx_ < sub_count - 1) {
            lv_obj_t *a = lv_img_create(cont);
            lv_img_set_src(a, img_arrow_down_.c_str());
            lv_obj_set_pos(a, sub_arrow_x, LIST_H - 14);
        }

        // Hint for selected sub item
        SubItem &cur_sub = item.sub_items[sub_selected_idx_];
        lv_obj_t *hint = lv_label_create(cont);
        if (cur_sub.is_toggle)
            lv_label_set_text(hint, cur_sub.toggle_state ? "ok:unselect" : "ok:select");
        else
            lv_label_set_text(hint, "ok:enter");
        lv_obj_set_pos(hint, SCREEN_W - 80, row_y(sub_center_vi) + (row_h() - 14) / 2);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x00CC66), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, LV_PART_MAIN);
    }

    // ==================== Value select view (3rd level) ====================
    void build_value_view()
    {
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);

        int count = (int)menu_items_[selected_idx_].sub_items.size();
        int val_count = (int)val_options_.size();

        // Gray highlight bar
        static constexpr int SEL_BAR_H2 = 22;
        static constexpr int SEL_BAR_W2 = 312;
        lv_obj_t *bar = lv_obj_create(cont);
        lv_obj_set_size(bar, SEL_BAR_W2, SEL_BAR_H2);
        lv_obj_set_pos(bar, (SCREEN_W - SEL_BAR_W2) / 2, row_y(ROW_CENTER) + (row_h() - SEL_BAR_H2) / 2);
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, 255, LV_PART_MAIN);
        lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        // Left column: sub-items (Brightness/DarkTime) as carousel at MENU_X
        lv_obj_t *val_left_lbl = nullptr;
        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            int si = sub_selected_idx_ - ROW_CENTER + vi;
            if (si < 0 || si >= count) continue;
            const char *text = menu_items_[selected_idx_].sub_items[si].label.c_str();
            lv_obj_t *lbl = create_carousel_label(cont, vi, ROW_CENTER, text, MENU_X);
            if (vi == ROW_CENTER) val_left_lbl = lbl;
        }

        // Right column: value options as carousel at SUB_CENTER_X (160)
        static constexpr int VAL_CENTER_X = 160;
        lv_obj_t *val_right_lbl = nullptr;
        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            int val_i = val_sel_idx_ - ROW_CENTER + vi;
            if (val_i < 0 || val_i >= val_count) continue;
            lv_obj_t *lbl = create_carousel_label(cont, vi, ROW_CENTER,
                                                   val_options_[val_i].c_str(), VAL_CENTER_X, true);
            if (vi == ROW_CENTER) val_right_lbl = lbl;
        }

        // Blue arrow between left and right
        if (val_left_lbl && val_right_lbl)
            place_blue_arrow(cont, val_left_lbl, val_right_lbl);

        // Arrows for value column
        int val_arrow_x = VAL_CENTER_X - 8;
        if (val_sel_idx_ > 0) {
            lv_obj_t *a = lv_img_create(cont);
            lv_img_set_src(a, img_arrow_up_.c_str());
            lv_obj_set_pos(a, val_arrow_x, 2);
        }
        if (val_sel_idx_ < val_count - 1) {
            lv_obj_t *a = lv_img_create(cont);
            lv_img_set_src(a, img_arrow_down_.c_str());
            lv_obj_set_pos(a, val_arrow_x, LIST_H - 14);
        }

        // Hint
        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "ok:set");
        lv_obj_set_pos(hint, SCREEN_W - 70, row_y(ROW_CENTER) + (row_h() - 14) / 2);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x00CC66), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, LV_PART_MAIN);
    }

    void rebuild_view()
    {
        if (view_state_ == ViewState::MAIN) build_main_view();
        else if (view_state_ == ViewState::SUB) build_sub_view();
        else if (view_state_ == ViewState::VALUE_SELECT) build_value_view();
    }

    // ==================== Events ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UISetupPage::static_handler, LV_EVENT_ALL, this);
    }
    static void static_handler(lv_event_t *e)
    {
        UISetupPage *self = static_cast<UISetupPage *>(lv_event_get_user_data(e));
        if (self) self->on_event(e);
    }

    void on_event(lv_event_t *e)
    {
        if (!IS_KEY_RELEASED(e)) return;
        struct key_item *elm = (struct key_item *)lv_event_get_param(e);
        cur_elm_ = elm;
        uint32_t key = elm->key_code;
        key = remap_fzxc(key);

        switch (view_state_) {
        case ViewState::MAIN:         handle_main_key(key); break;
        case ViewState::SUB:          handle_sub_key(key);  break;
        case ViewState::VALUE_SELECT: handle_value_key(key); break;
        case ViewState::WIFI_PW:      handle_wifi_pw_key(key); break;
        }
    }

    static uint32_t remap_fzxc(uint32_t key)
    {
        switch (key) {
        case KEY_F: return KEY_UP;
        case KEY_X: return KEY_DOWN;
        case KEY_Z: return KEY_LEFT;
        case KEY_C: return KEY_RIGHT;
        default:    return key;
        }
    }

    void handle_main_key(uint32_t key)
    {
        switch (key) {
        case KEY_UP:
            animate_scroll(-1);
            break;
        case KEY_DOWN:
            animate_scroll(1);
            break;
        case KEY_ENTER:
        case KEY_RIGHT: {
            MenuItem &m = menu_items_[selected_idx_];
            if (m.on_enter) m.on_enter();
            if (!m.sub_items.empty()) {
                view_state_ = ViewState::SUB;
                int sc = (int)m.sub_items.size();
                sub_selected_idx_ = sc > ROW_CENTER ? ROW_CENTER : sc - 1;
                build_sub_view();
            }
            break;
        }
        case KEY_ESC:
            if (go_back_home) go_back_home();
            break;
        default:
            break;
        }
    }

    void handle_sub_key(uint32_t key)
    {
        MenuItem &item = menu_items_[selected_idx_];
        int sub_count = (int)item.sub_items.size();

        switch (key) {
        case KEY_UP:
            if (sub_selected_idx_ > 0) { --sub_selected_idx_; build_sub_view(); }
            break;
        case KEY_DOWN:
            if (sub_selected_idx_ < sub_count - 1) { ++sub_selected_idx_; build_sub_view(); }
            break;
        case KEY_ENTER:
        case KEY_RIGHT: {
            if (sub_selected_idx_ < sub_count) {
                SubItem &sub = item.sub_items[sub_selected_idx_];
                if (sub.is_toggle) {
                    sub.toggle_state = !sub.toggle_state;
                    if (sub.action) sub.action();
                    else build_sub_view();
                } else if (sub.action) {
                    sub.action();
                }
            }
            break;
        }
        case KEY_ESC:
        case KEY_LEFT:
            view_state_ = ViewState::MAIN;
            build_main_view();
            break;
        default:
            if (item.custom_key_handler) item.custom_key_handler(key);
            break;
        }
    }

    void handle_value_key(uint32_t key)
    {
        int val_count = (int)val_options_.size();
        switch (key) {
        case KEY_UP:
            if (val_sel_idx_ > 0) { --val_sel_idx_; build_value_view(); }
            break;
        case KEY_DOWN:
            if (val_sel_idx_ < val_count - 1) { ++val_sel_idx_; build_value_view(); }
            break;
        case KEY_ENTER:
        case KEY_RIGHT:
            apply_value_selection();
            view_state_ = ViewState::SUB;
            build_sub_view();
            break;
        case KEY_ESC:
        case KEY_LEFT:
            view_state_ = ViewState::SUB;
            build_sub_view();
            break;
        default:
            break;
        }
    }
};

#else // HAL_PLATFORM_SDL — stub for emulator builds
#include "../ui_app_page.hpp"

class UISetupPage : public app_base
{
public:
    UISetupPage() : app_base() { set_page_title("SETTING"); }
    ~UISetupPage() {}
};

#endif // !HAL_PLATFORM_SDL
