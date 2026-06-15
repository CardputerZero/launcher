/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
// Keep this page platform-neutral: system and hardware operations go through
// cp0_lvgl's cp0_* service interfaces so the SDL build can compile the page.
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
#include <ctime>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <dirent.h>
#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "../AppRegistry.h"

// ============================================================
//  System settings screen  UISetupPage  (Carousel Design)
//  Screen: 320x170 (top bar20px, body 320x150)
//
//  Menu items (design mockup): Launcher, Boot, Screen, WiFi, Speaker, Camera
//  Actual HAL integration: WiFi scan/connect, brightness, volume, power, reboot, about
// ============================================================

class UISetupPage : public AppPage
{
    enum class ViewState { MAIN, SUB, VALUE_SELECT, WIFI_LIST, WIFI_PW };

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
    UISetupPage() : AppPage()
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
    cp0_wifi_ap_t wifi_aps_[CP0_WIFI_AP_MAX];
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

    // Audio feedback paths
    std::string snd_enter_;
    std::string snd_back_;

    void play_enter() { cp0_signal_audio_api({"SystemSoundPlay", "2"}, nullptr); }
    void play_back()  { play_audio_file(snd_back_); }

    static int config_get_int(const char *key, int default_val)
    {
        int val = default_val;
        cp0_signal_config_api({"GetInt", key ? std::string(key) : std::string(), std::to_string(default_val)},
                              [&](int code, std::string data) {
                                  if (code == 0) val = std::atoi(data.c_str());
                              });
        return val;
    }

    static void config_set_int(const char *key, int val)
    {
        cp0_signal_config_api({"SetInt", key ? std::string(key) : std::string(), std::to_string(val)}, nullptr);
    }

    static void config_save()
    {
        cp0_signal_config_api({"Save"}, nullptr);
    }

    static int audio_volume_read()
    {
        int volume = -1;
        cp0_signal_audio_api({"VolumeRead"}, [&](int code, std::string data) {
            if (code == 0)
                volume = std::atoi(data.c_str());
        });
        return volume;
    }

    static int audio_volume_write(int val)
    {
        int volume = -1;
        cp0_signal_audio_api({"VolumeWrite", std::to_string(val)}, [&](int code, std::string data) {
            if (code == 0)
                volume = std::atoi(data.c_str());
        });
        return volume;
    }

    void play_audio_file(const std::string &path)
    {
        if (!path.empty()) cp0_signal_audio_api({"PlayFile", path}, nullptr);
    }

    void cache_image_paths()
    {
        img_arrow_up_    = cp0_file_path("setting_red_up.png");
        img_arrow_down_  = cp0_file_path("setting_red_down.png");
        img_right_arrow_ = cp0_file_path("setting_right_arrow.png");
        img_ok_          = cp0_file_path("setting_ok.png");
        img_cross_       = cp0_file_path("setting_cross.png");
        snd_enter_       = cp0_file_path("key_enter.wav");
        snd_back_        = cp0_file_path("key_back.wav");
    }

    // ==================== Menu init ====================
    void menu_init()
    {
        // --- Launcher (app enable/disable, OX toggle with persistence) ---
        {
            MenuItem m;
            m.label = "Launcher";
            std::size_t app_count = 0;
            const AppDescriptor *apps = launcher_app_registry_entries(&app_count);
            for (std::size_t i = 0; i < app_count; ++i) {
                const AppDescriptor &desc = apps[i];
                if (!desc.configurable)
                    continue;
                bool enabled = launcher_app_registry_is_enabled(desc);
                m.sub_items.push_back({desc.label, true, enabled,
                    [this, key = std::string(desc.config_key)]() { save_app_toggle(key); }});
            }
            menu_items_.push_back(m);
        }
        // --- Boot (with confirmation via value select) ---
        {
            MenuItem m;
            m.label = "Boot";
            m.sub_items = {
                {"Reboot",   false, false, [this]() { enter_confirm_action("Reboot?", [this](){ cp0_system_reboot(); }); }},
                {"Shutdown", false, false, [this]() { enter_confirm_action("Shutdown?", [this](){ cp0_system_shutdown(); }); }},
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
                {"Scan",    false, false, [this]() { enter_wifi_scan(); }},
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
        // --- Info (auto-refresh with timer) ---
        {
            MenuItem m;
            m.label = "Info";
            m.sub_items = {
                {"Battery: --%",     false, false, nullptr},
                {"Temp: --C",        false, false, nullptr},
                {"Current: --mA",    false, false, nullptr},
                {"Voltage: --V",     false, false, nullptr},
                {"BQ Calibrate",     false, false, [this]() { enter_bq_calibrate(); }},
            };
            m.on_enter = [this]() { refresh_info_values(); start_info_timer(); };
            menu_items_.push_back(m);
        }
        // --- About ---
        {
            MenuItem m;
            m.label = "About";
            m.sub_items = {
                {"CardputerZero",    false, false, nullptr},
                {"LVGL 9.x",        false, false, nullptr},
                {"",                 false, false, nullptr},
                {"",                 false, false, nullptr},
            };
            m.on_enter = [this]() { refresh_about_info(); };
            menu_items_.push_back(m);
        }
        // --- Help ---
        {
            MenuItem m;
            m.label = "Help";
            m.sub_items = {
                {"View Help", false, false, [this]() { enter_help_page(); }},
            };
            menu_items_.push_back(m);
        }
        // --- ExtPort ---
        {
            MenuItem m;
            m.label = "ExtPort";
            bool usb_en = config_get_int("extport_usb", 1) != 0;
            bool vout_en = config_get_int("extport_5vout", 1) != 0;
            m.sub_items = {
                {"USB",   true, usb_en, [this]() {
                    bool en = menu_items_[7].sub_items[0].toggle_state;
                    config_set_int("extport_usb", en ? 1 : 0);
                    config_save();
                }},
                {"5VOUT", true, vout_en, [this]() {
                    bool en = menu_items_[7].sub_items[1].toggle_state;
                    config_set_int("extport_5vout", en ? 1 : 0);
                    config_save();
                }},
            };
            menu_items_.push_back(m);
        }
        // --- RTC ---
        {
            MenuItem m;
            m.label = "RTC";
            m.sub_items = {
                {"Year",   false, false, [this]() { enter_rtc_adjust(0); }},
                {"Month",  false, false, [this]() { enter_rtc_adjust(1); }},
                {"Day",    false, false, [this]() { enter_rtc_adjust(2); }},
                {"Hour",   false, false, [this]() { enter_rtc_adjust(3); }},
                {"Minute", false, false, [this]() { enter_rtc_adjust(4); }},
                {"Second", false, false, [this]() { enter_rtc_adjust(5); }},
            };
            m.on_enter = [this]() { refresh_rtc_values(); };
            menu_items_.push_back(m);
        }
        // --- Bluetooth ---
        {
            MenuItem m;
            m.label = "Bluetooth";
            m.sub_items = {
                {"Power",  true, false, [this]() { bt_toggle_power(); }},
                {"Scan",   false, false, [this]() { bt_do_scan(); }},
            };
            m.on_enter = [this]() { refresh_bt_status(); };
            menu_items_.push_back(m);
        }
        // --- Ethernet ---
        {
            MenuItem m;
            m.label = "Ethernet";
            m.sub_items = {
                {"IP: --",      false, false, nullptr},
                {"Gateway: --", false, false, nullptr},
                {"MAC: --",     false, false, nullptr},
            };
            m.on_enter = [this]() { refresh_ethernet_info(); };
            menu_items_.push_back(m);
        }
        // --- Account ---
        {
            MenuItem m;
            m.label = "Account";
            m.sub_items = {
                {"Username", false, false, nullptr},
                {"Password", false, false, nullptr},
                {"Hostname", false, false, nullptr},
            };
            m.on_enter = [this]() { refresh_account_info(); };
            menu_items_.push_back(m);
        }
        // --- Update ---
        {
            MenuItem m;
            m.label = "Update";
            m.sub_items = {
                {"Check System",   false, false, [this]() { check_system_update(); }},
                {"Update Launcher", false, false, [this]() { update_launcher(); }},
                {"Version: --",    false, false, nullptr},
            };
            m.on_enter = [this]() { refresh_version_info(); };
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
        transition_enter_level();
    }

    void enter_volume_adjust()
    {
        val_title_ = "Volume";
        val_options_ = {"100%", "75%", "50%", "25%", "0%"};
        vol_val_ = config_get_int("volume", audio_volume_read());
        int pct = vol_val_;
        if (pct >= 87) val_sel_idx_ = 0;
        else if (pct >= 62) val_sel_idx_ = 1;
        else if (pct >= 37) val_sel_idx_ = 2;
        else if (pct >= 12) val_sel_idx_ = 3;
        else val_sel_idx_ = 4;
        view_state_ = ViewState::VALUE_SELECT;
        transition_enter_level();
    }

    void enter_camera_resolution()
    {
        val_title_ = "Resolution";
        val_options_ = {"1280x720", "640x480"};
        val_sel_idx_ = 0;
        view_state_ = ViewState::VALUE_SELECT;
        transition_enter_level();
    }

    void enter_startup_select()
    {
        val_title_ = "Startup";
        val_options_ = {"Launcher", "CLI"};
        val_sel_idx_ = config_get_int("startup_mode", 0);
        view_state_ = ViewState::VALUE_SELECT;
        transition_enter_level();
    }

    int wifi_list_sel_ = 0;

    void enter_wifi_scan()
    {
        wifi_do_scan();
        wifi_list_sel_ = 0;
        view_state_ = ViewState::WIFI_LIST;
        build_wifi_list();
    }

    void build_wifi_list()
    {
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);

        // Title + current connection status
        lv_obj_t *title = lv_label_create(cont);
        {
            cp0_wifi_status_t ws = launcher_wifi::get_status();
            static char title_buf[128];
            if (ws.connected)
                snprintf(title_buf, sizeof(title_buf), "Connected WiFi: %.64s  %.40s", ws.ssid, ws.ip);
            else
                snprintf(title_buf, sizeof(title_buf), "WiFi: Not connected");
            lv_label_set_text(title, title_buf);
        }
        lv_obj_set_pos(title, 8, 2);
        lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);

        // Column headers
        lv_obj_t *h1 = lv_label_create(cont);
        lv_label_set_text(h1, "SSID");
        lv_obj_set_pos(h1, 8, 18);
        lv_obj_set_style_text_color(h1, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(h1, &lv_font_montserrat_10, LV_PART_MAIN);

        lv_obj_t *h2 = lv_label_create(cont);
        lv_label_set_text(h2, "Security");
        lv_obj_set_pos(h2, 180, 18);
        lv_obj_set_style_text_color(h2, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(h2, &lv_font_montserrat_10, LV_PART_MAIN);

        lv_obj_t *h3 = lv_label_create(cont);
        lv_label_set_text(h3, "Signal");
        lv_obj_set_pos(h3, 270, 18);
        lv_obj_set_style_text_color(h3, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(h3, &lv_font_montserrat_10, LV_PART_MAIN);

        if (wifi_ap_count_ == 0) {
            lv_obj_t *empty = lv_label_create(cont);
            lv_label_set_text(empty, "No networks found. Press R to rescan.");
            lv_obj_set_pos(empty, 8, 50);
            lv_obj_set_style_text_color(empty, lv_color_hex(0x666666), LV_PART_MAIN);
            lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, LV_PART_MAIN);
            return;
        }

        // List items (show up to 5 visible, scrolled)
        int visible = 5;
        int offset = wifi_list_sel_ - visible / 2;
        if (offset < 0) offset = 0;
        if (offset > wifi_ap_count_ - visible) offset = wifi_ap_count_ - visible;
        if (offset < 0) offset = 0;

        for (int vi = 0; vi < visible && (vi + offset) < wifi_ap_count_; ++vi) {
            int ai = vi + offset;
            bool sel = (ai == wifi_list_sel_);
            cp0_wifi_ap_t *ap = &wifi_aps_[ai];
            int y = 30 + vi * 22;

            // Selection highlight
            if (sel) {
                lv_obj_t *bg = lv_obj_create(cont);
                lv_obj_set_size(bg, SCREEN_W - 8, 20);
                lv_obj_set_pos(bg, 4, y);
                lv_obj_set_style_radius(bg, 2, LV_PART_MAIN);
                lv_obj_set_style_bg_color(bg, lv_color_hex(0x1F3A5F), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN);
                lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN);
                lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
            }

            uint32_t tc = sel ? 0xFFFFFF : 0xCCCCCC;
            if (ap->in_use) tc = 0x58A6FF;

            // SSID (append * if we have a saved password for it)
            lv_obj_t *ssid_lbl = lv_label_create(cont);
            static char ssid_buf[CP0_WIFI_SSID_MAX + 4];
            if (wifi_has_saved_profile(ap->ssid))
                snprintf(ssid_buf, sizeof(ssid_buf), "%s *", ap->ssid);
            else
                snprintf(ssid_buf, sizeof(ssid_buf), "%s", ap->ssid);
            lv_label_set_text(ssid_lbl, ssid_buf);
            lv_obj_set_pos(ssid_lbl, 8, y + 2);
            lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(tc), LV_PART_MAIN);
            lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
            lv_obj_set_width(ssid_lbl, 165);
            lv_label_set_long_mode(ssid_lbl, LV_LABEL_LONG_CLIP);

            // Security
            lv_obj_t *sec = lv_label_create(cont);
            lv_label_set_text(sec, ap->security[0] ? ap->security : "Open");
            lv_obj_set_pos(sec, 180, y + 2);
            lv_obj_set_style_text_color(sec, lv_color_hex(tc), LV_PART_MAIN);
            lv_obj_set_style_text_font(sec, &lv_font_montserrat_10, LV_PART_MAIN);

            // Signal
            char sig_buf[16];
            snprintf(sig_buf, sizeof(sig_buf), "%d%%", ap->signal);
            lv_obj_t *sig = lv_label_create(cont);
            lv_label_set_text(sig, sig_buf);
            lv_obj_set_pos(sig, 275, y + 2);
            lv_obj_set_style_text_color(sig, lv_color_hex(tc), LV_PART_MAIN);
            lv_obj_set_style_text_font(sig, &lv_font_montserrat_10, LV_PART_MAIN);
        }

        // Hint
        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "OK:connect  R:rescan  D:forget  ESC:back");
        lv_obj_set_pos(hint, 8, LIST_H - 14);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
    }

    void handle_wifi_list_key(uint32_t key)
    {
        switch (key) {
        case KEY_UP:
            if (wifi_list_sel_ > 0) { --wifi_list_sel_; build_wifi_list(); }
            break;
        case KEY_DOWN:
            if (wifi_list_sel_ < wifi_ap_count_ - 1) { ++wifi_list_sel_; build_wifi_list(); }
            break;
        case KEY_ENTER:
            if (wifi_ap_count_ > 0) wifi_try_connect(wifi_list_sel_);
            break;
        case KEY_R:
            wifi_do_scan();
            wifi_list_sel_ = 0;
            build_wifi_list();
            break;
        case KEY_D:
            if (wifi_ap_count_ > 0) wifi_forget_selected();
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

    void refresh_info_values()
    {
        for (auto &m : menu_items_) {
            if (m.label != "Info") continue;
            cp0_battery_info_t bat = cp0_battery_read();
            char buf[64];
            snprintf(buf, sizeof(buf), "Battery: %d%%", bat.valid ? bat.soc : 0);
            m.sub_items[0].label = buf;
            snprintf(buf, sizeof(buf), "Temp: %.1fC", bat.valid ? bat.temperature_c10 / 10.0 : 0.0);
            m.sub_items[1].label = buf;
            snprintf(buf, sizeof(buf), "Current: %dmA", bat.valid ? bat.current_ma : 0);
            m.sub_items[2].label = buf;
            snprintf(buf, sizeof(buf), "Voltage: %.2fV", bat.valid ? bat.voltage_mv / 1000.0 : 0.0);
            m.sub_items[3].label = buf;
            break;
        }
        // Refresh the sub view if currently showing Info
        if (view_state_ == ViewState::SUB) build_sub_view();
    }

    // ==================== RTC ====================
    int rtc_values_[6] = {2026, 1, 1, 0, 0, 0}; // Y/M/D/H/Min/S
    int rtc_field_ = 0;

    void refresh_rtc_values()
    {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        if (t) {
            rtc_values_[0] = t->tm_year + 1900;
            rtc_values_[1] = t->tm_mon + 1;
            rtc_values_[2] = t->tm_mday;
            rtc_values_[3] = t->tm_hour;
            rtc_values_[4] = t->tm_min;
            rtc_values_[5] = t->tm_sec;
        }
        // Update labels
        for (auto &m : menu_items_) {
            if (m.label != "RTC") continue;
            char buf[32];
            const char *names[] = {"Year", "Month", "Day", "Hour", "Minute", "Second"};
            for (int i = 0; i < 6; ++i) {
                snprintf(buf, sizeof(buf), "%s: %d", names[i], rtc_values_[i]);
                m.sub_items[i].label = buf;
            }
            break;
        }
    }

    void enter_rtc_adjust(int field)
    {
        rtc_field_ = field;
        const char *names[] = {"Year", "Month", "Day", "Hour", "Minute", "Second"};
        val_title_ = names[field];

        // Generate valid range: all values in range, current in the middle
        int cur = rtc_values_[field];
        int mins[] = {2000, 1, 1, 0, 0, 0};
        int maxs[] = {2099, 12, 31, 23, 59, 59};

        val_options_.clear();
        for (int v = mins[field]; v <= maxs[field]; ++v) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", v);
            val_options_.push_back(buf);
        }
        // Set selection to current value
        val_sel_idx_ = cur - mins[field];
        view_state_ = ViewState::VALUE_SELECT;
        transition_enter_level();
    }

    void apply_rtc_value()
    {
        int new_val = atoi(val_options_[val_sel_idx_].c_str());
        rtc_values_[rtc_field_] = new_val;
        char timestamp[32];
        snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
                 rtc_values_[0], rtc_values_[1], rtc_values_[2],
                 rtc_values_[3], rtc_values_[4], rtc_values_[5]);
        cp0_time_set(timestamp);
    }

    void save_app_toggle(const std::string &config_key)
    {
        std::size_t app_count = 0;
        const AppDescriptor *apps = launcher_app_registry_entries(&app_count);
        int visible_idx = 0;
        for (std::size_t i = 0; i < app_count; ++i) {
            const AppDescriptor &desc = apps[i];
            if (!desc.configurable)
                continue;
            if (config_key == desc.config_key) {
                bool enabled = menu_items_[0].sub_items[visible_idx].toggle_state;
                launcher_app_registry_set_enabled(desc, enabled);
                config_save();
                launcher_app_registry_notify_changed();
                return;
            }
            ++visible_idx;
        }
    }

    // ==================== Bluetooth ====================
    void refresh_bt_status()
    {
        cp0_bt_status_t st = cp0_bt_get_status();
        // Find Bluetooth menu and update Power toggle state
        for (auto &m : menu_items_) {
            if (m.label != "Bluetooth") continue;
            m.sub_items[0].toggle_state = st.powered != 0;
            break;
        }
    }

    void bt_toggle_power()
    {
        for (auto &m : menu_items_) {
            if (m.label != "Bluetooth") continue;
            bool on = m.sub_items[0].toggle_state;
            cp0_bt_set_power(on ? 1 : 0);
            break;
        }
    }

    void bt_do_scan()
    {
        cp0_bt_device_t devices[CP0_BT_DEVICE_MAX];
        cp0_bt_scan(devices, CP0_BT_DEVICE_MAX);
    }

    // ==================== Ethernet ====================
    void refresh_ethernet_info()
    {
        for (auto &m : menu_items_) {
            if (m.label != "Ethernet") continue;
            cp0_eth_info_t info;
            cp0_network_default_info_read(&info);
            m.sub_items[0].label = std::string("IP: ") + info.ipv4;
            m.sub_items[1].label = std::string("GW: ") + info.gateway;
            m.sub_items[2].label = std::string("MAC: ") + info.mac;
            break;
        }
    }

    // ==================== Account ====================
    void refresh_account_info()
    {
        for (auto &m : menu_items_) {
            if (m.label != "Account") continue;
            cp0_account_info_t info;
            cp0_account_info_read(&info);
            m.sub_items[0].label = std::string("User: ") + info.user;
            m.sub_items[1].label = "Password: ****";
            m.sub_items[2].label = std::string("Host: ") + info.hostname;
            break;
        }
    }

    // ==================== Update ====================
    void refresh_version_info()
    {
        for (auto &m : menu_items_) {
            if (m.label != "Update") continue;
            m.sub_items[2].label = std::string("Version: ") + LAUNCHER_GIT_COMMIT;
            break;
        }
    }

    void check_system_update()
    {
        // Run apt update check in background
        cp0_system_apt_update_background();
        // TODO: show result in UI
    }

    void update_launcher()
    {
        cp0_system_update_launcher_background();
    }

    // ==================== Confirm action (Reboot/Shutdown) ====================
    std::function<void()> confirm_action_;

    void enter_confirm_action(const char *title, std::function<void()> action)
    {
        confirm_action_ = action;
        val_title_ = title;
        val_options_ = {"Yes", "No"};
        val_sel_idx_ = 1; // default to No
        view_state_ = ViewState::VALUE_SELECT;
        transition_enter_level();
    }

    // ==================== Info timer (auto-refresh) ====================
    lv_timer_t *info_timer_ = nullptr;

    void start_info_timer()
    {
        stop_info_timer();
        info_timer_ = lv_timer_create([](lv_timer_t *t) {
            UISetupPage *self = (UISetupPage *)lv_timer_get_user_data(t);
            if (self && self->view_state_ == ViewState::SUB) self->refresh_info_values();
        }, 1000, this);
    }

    void stop_info_timer()
    {
        if (info_timer_) { lv_timer_delete(info_timer_); info_timer_ = nullptr; }
    }

    // ==================== BQ27220 Calibration ====================
    void enter_bq_calibrate()
    {
        val_title_ = "BQ Calib";
        val_options_ = {"Enter CAL", "CC Offset", "Board Offset", "Exit CAL"};
        val_sel_idx_ = 0;
        view_state_ = ViewState::VALUE_SELECT;
        transition_enter_level();
    }

    void apply_bq_calibrate()
    {
        cp0_bq27220_calibrate(val_sel_idx_);
    }

    // ==================== About ====================
    void refresh_about_info()
    {
        for (auto &m : menu_items_) {
            if (m.label != "About") continue;
            m.sub_items[0].label = "M5CardputerZero";
            m.sub_items[1].label = "LVGL: 9.x";
            char buf[64];
            snprintf(buf, sizeof(buf), "Build: %s", __DATE__);
            m.sub_items[2].label = buf;
            snprintf(buf, sizeof(buf), "Commit: %s", LAUNCHER_GIT_COMMIT);
            m.sub_items[3].label = buf;
            break;
        }
    }

    // ==================== Help page (full-screen like WiFi scan) ===========
    void enter_help_page()
    {
        view_state_ = ViewState::WIFI_LIST; // reuse WIFI_LIST view state for ESC handling
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);

        int y = 4;
        auto add_line = [&](const char *text, uint32_t color, const lv_font_t *font) {
            lv_obj_t *lbl = lv_label_create(cont);
            lv_label_set_text(lbl, text);
            lv_obj_set_pos(lbl, 8, y);
            lv_obj_set_width(lbl, 300);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
            lv_obj_update_layout(lbl);
            y += lv_obj_get_height(lbl) + 3;
        };

        add_line("Help", 0x58A6FF, launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD));
        add_line("Screenshot: Ctrl+Alt+S", 0xCCCCCC, &lv_font_montserrat_12);
        add_line("  Saved to ~/Screenshots", 0x888888, &lv_font_montserrat_10);
        add_line("Home: Hold ESC 5s", 0xCCCCCC, &lv_font_montserrat_12);
        add_line("Navigate: Arrow keys / OK / ESC", 0xCCCCCC, &lv_font_montserrat_12);
        add_line("WiFi: Setting > WiFi > Scan", 0xCCCCCC, &lv_font_montserrat_12);

        // Footer
        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "ESC: back");
        lv_obj_set_pos(hint, 8, LIST_H - 14);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
    }

    // ==================== Bluetooth scan ====================
    void bt_do_scan_impl()
    {
        // TODO: implement BT scan list page similar to WiFi
        // For now just trigger scan
        cp0_bt_device_t devices[CP0_BT_DEVICE_MAX];
        int count = cp0_bt_scan(devices, CP0_BT_DEVICE_MAX);
        (void)count;
    }

    void factory_reset()
    {
        remove("/var/lib/applaunch/settings");
        cp0_system_reboot();
    }

    // ==================== WiFi functions ====================
    void wifi_do_scan()
    {
        wifi_ap_count_ = launcher_wifi::scan(wifi_aps_, CP0_WIFI_AP_MAX);
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
        cp0_wifi_ap_t *ap = &wifi_aps_[idx];
        if (ap->in_use) return;

        bool needs_password = false;
        int ret = -1;
        if (strcmp(ap->security, "Open") == 0 || ap->security[0] == 0) {
            wifi_show_connecting(ap->ssid);
            ret = launcher_wifi::connect(ap->ssid, NULL);
        } else if (wifi_has_saved_profile(ap->ssid)) {
            wifi_show_connecting(ap->ssid);
            ret = launcher_wifi::connect(ap->ssid, NULL);
        } else {
            needs_password = true;
            wifi_pw_ssid_ = ap->ssid;
            wifi_pw_buf_.clear();
            show_wifi_pw_input();
        }
        if (!needs_password) {
            if (ret != 0) {
                // Connection failed — show error briefly then rebuild list
                wifi_show_error("Connection failed");
            }
            wifi_do_scan();
            build_wifi_list();
        }
    }

    void wifi_show_connecting(const char *ssid)
    {
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);
        static char msg[128];
        snprintf(msg, sizeof(msg), "Connecting to %s...", ssid);
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, msg);
        lv_obj_set_pos(lbl, 8, 60);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x58A6FF), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_refr_now(NULL);
    }

    void wifi_show_error(const char *msg)
    {
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, msg);
        lv_obj_set_pos(lbl, 8, 60);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFF4444), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_refr_now(NULL);
        usleep(2000000); // show error for 2 seconds before rebuilding list
    }

    void wifi_forget_selected()
    {
        if (wifi_list_sel_ < 0 || wifi_list_sel_ >= wifi_ap_count_) return;
        cp0_wifi_ap_t *ap = &wifi_aps_[wifi_list_sel_];

        if (!wifi_has_saved_profile(ap->ssid)) {
            wifi_show_error("No saved password for this network");
            wifi_do_scan();
            build_wifi_list();
            return;
        }

        // Show confirm then delete
        char msg[128];
        snprintf(msg, sizeof(msg), "Forget '%s'?", ap->ssid);
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, msg);
        lv_obj_set_pos(lbl, 8, 50);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFAA00), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "OK:confirm  ESC:cancel");
        lv_obj_set_pos(hint, 8, 75);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_refr_now(NULL);

        // Block and wait for confirm key (simple blocking confirm)
        // We reuse the WIFI_LIST view state but override next key handling
        wifi_pw_ssid_ = ap->ssid;  // stash SSID for confirm
        view_state_ = ViewState::WIFI_PW;  // hijack PW state for confirm
        // Override: next KEY_ENTER in pw handler will do the delete
        // Actually — simpler: just do it immediately (user already pressed F
        // which is intentional). Delete + refresh.
        launcher_wifi::profile_forget(ap->ssid);

        // If currently connected to this SSID, disconnect
        if (ap->in_use) {
            launcher_wifi::profile_disconnect_active();
        }

        // Show success briefly
        lv_obj_clean(cont);
        lbl = lv_label_create(cont);
        snprintf(msg, sizeof(msg), "Forgot '%s'", ap->ssid);
        lv_label_set_text(lbl, msg);
        lv_obj_set_pos(lbl, 8, 60);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x33CC33), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_refr_now(NULL);
        usleep(1500000);

        view_state_ = ViewState::WIFI_LIST;
        wifi_do_scan();
        build_wifi_list();
    }

    bool wifi_has_saved_profile(const char *ssid)
    {
        return launcher_wifi::profile_exists(ssid) != 0;
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
            view_state_ = ViewState::WIFI_LIST;
            rebuild_view();
            return;
        }
        if (key == KEY_ENTER) {
            if (pw_hint_lbl_) lv_label_set_text(pw_hint_lbl_, "Connecting...");
            lv_refr_now(NULL);
            int ret = launcher_wifi::connect(wifi_pw_ssid_.c_str(), wifi_pw_buf_.c_str());
            if (ret != 0) {
                // Connection failed — delete the broken profile that nmcli just
                // saved with the wrong password, so next attempt won't reuse it.
                launcher_wifi::profile_forget(wifi_pw_ssid_.c_str());

                if (pw_hint_lbl_) {
                    lv_label_set_text(pw_hint_lbl_, "Failed! Wrong password? Try again.");
                    lv_obj_set_style_text_color(pw_hint_lbl_, lv_color_hex(0xFF4444), LV_PART_MAIN);
                }
                wifi_pw_buf_.clear();
                wifi_pw_update_display();
                return;
            }
            view_state_ = ViewState::WIFI_LIST;
            wifi_do_scan();
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
        int new_val = pcts[val_sel_idx_];
        audio_volume_write(new_val);
        config_set_int("volume", new_val);
        config_save();
    }

    // ==================== Brightness ====================
    int settings_backlight_read()
    {
        int value = -1;
        cp0_signal_settings_api({"BacklightRead"}, [&](int code, std::string data) {
            if (code == 0) value = std::atoi(data.c_str());
        });
        return value;
    }

    int settings_backlight_max()
    {
        int value = 100;
        cp0_signal_settings_api({"BacklightMax"}, [&](int code, std::string data) {
            if (code == 0) value = std::atoi(data.c_str());
        });
        return value;
    }

    void enter_brightness_adjust()
    {
        val_title_ = "Brightness";
        val_options_ = {"100%", "75%", "50%", "25%"};
        bright_val_ = settings_backlight_read();
        int mx = settings_backlight_max();
        int pct = mx > 0 ? bright_val_ * 100 / mx : 100;
        if (pct >= 87) val_sel_idx_ = 0;
        else if (pct >= 62) val_sel_idx_ = 1;
        else if (pct >= 37) val_sel_idx_ = 2;
        else val_sel_idx_ = 3;
        view_state_ = ViewState::VALUE_SELECT;
        transition_enter_level();
    }

    void apply_value_selection()
    {
        if (val_title_ == "Brightness") {
            int mx = settings_backlight_max();
            int pcts[] = {100, 75, 50, 25};
            int new_val = mx * pcts[val_sel_idx_] / 100;
            if (new_val < 1) new_val = 1;
            cp0_backlight_write(new_val);
            config_set_int("brightness", new_val);
            config_save();
        } else if (val_title_ == "Volume") {
            apply_volume();
        } else if (val_title_ == "DarkTime") {
            // TODO: save dark time setting
            int times[] = {0, 10, 30, 60, 300};
            config_set_int("dark_time", times[val_sel_idx_]);
            config_save();
        } else if (val_title_ == "Resolution") {
            config_set_int("cam_resolution", val_sel_idx_);
            config_save();
        } else if (val_title_ == "Startup") {
            config_set_int("startup_mode", val_sel_idx_);
            config_save();
        } else if (val_title_ == "Year" || val_title_ == "Month" || val_title_ == "Day" ||
                   val_title_ == "Hour" || val_title_ == "Minute" || val_title_ == "Second") {
            apply_rtc_value();
        } else if (val_title_ == "Reboot?" || val_title_ == "Shutdown?") {
            if (val_sel_idx_ == 0 && confirm_action_) confirm_action_(); // "Yes"
        } else if (val_title_ == "BQ Calib") {
            apply_bq_calibrate();
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
            return {launcher_fonts().get("Montserrat-Bold.ttf", 18, LV_FREETYPE_FONT_STYLE_BOLD), 0xFFFFFF, MENU_X, 255};
        if (dist == 1)
            return {launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), 0xAAAAAA, MENU_X, 220};
        if (dist == 2)
            return {launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD), 0x777777, MENU_X, 170};
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
                font = launcher_fonts().get("Montserrat-Bold.ttf", 18, LV_FREETYPE_FONT_STYLE_BOLD);
                color = 0xFFFFFF; opa = 255;
            } else if (dist == 1) {
                font = launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD);
                color = 0xAAAAAA; opa = 220;
            } else if (dist == 2) {
                font = launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD);
                color = 0x777777; opa = 170;
            } else {
                font = &lv_font_montserrat_12;
                color = 0x555555; opa = 130;
            }
        } else {
            // Smaller variant for sub-menu / right column
            if (dist == 0) {
                font = launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD);
                color = 0xFFFFFF; opa = 255;
            } else if (dist == 1) {
                font = launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD);
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
        int ly = row_y(vi) + (row_h() - font_h) / 2;
        if (smaller) ly += 1;
        lv_obj_set_pos(lbl, lx, ly);

        if (!text || !text[0])
            lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
        return lbl;
    }

    static constexpr int ARROW_W = 18;

    // Place blue arrow between left column and right column.
    // Uses left_lbl's right edge and right_min_x (leftmost x of any right-column item)
    // to center the arrow in the gap between them.
    void place_blue_arrow(lv_obj_t *parent, lv_obj_t *left_lbl, int right_min_x)
    {
        if (!left_lbl || right_min_x <= 0) return;
        lv_obj_update_layout(left_lbl);

        int left_right_edge = lv_obj_get_x(left_lbl) + lv_obj_get_width(left_lbl);
        int gap = right_min_x - left_right_edge;

        int arrow_x;
        if (gap >= ARROW_W) {
            arrow_x = left_right_edge + (gap - ARROW_W) / 2;
        } else {
            arrow_x = right_min_x - ARROW_W;
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
        static constexpr int SEL_BAR_H = 23;
        static constexpr int SEL_BAR_W = 312;
        sel_bg_ = lv_obj_create(cont);
        lv_obj_set_size(sel_bg_, SEL_BAR_W, SEL_BAR_H);
        lv_obj_set_pos(sel_bg_, (SCREEN_W - SEL_BAR_W) / 2, row_y(ROW_CENTER) + (row_h() - SEL_BAR_H) / 2);
        lv_obj_set_style_radius(sel_bg_, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(sel_bg_, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(sel_bg_, 255, LV_PART_MAIN);
        lv_obj_set_style_border_width(sel_bg_, 0, LV_PART_MAIN);
        lv_obj_clear_flag(sel_bg_, LV_OBJ_FLAG_SCROLLABLE);

        // Hint label (right-aligned, 6px from right edge, smaller font)
        hint_lbl_ = lv_label_create(cont);
        lv_label_set_text(hint_lbl_, "ok:enter");
        lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0x00CC66), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint_lbl_, launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
        lv_obj_update_layout(hint_lbl_);
        int hint_w = lv_obj_get_width(hint_lbl_);
        int hint_h = lv_obj_get_height(hint_lbl_);
        lv_obj_set_pos(hint_lbl_, SCREEN_W - 6 - hint_w, row_y(ROW_CENTER) + (row_h() - hint_h) / 2);

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

        arrow_down_obj_ = lv_img_create(cont);
        lv_img_set_src(arrow_down_obj_, img_arrow_down_.c_str());
        lv_obj_set_pos(arrow_down_obj_, arrow_x, LIST_H - 14);

        is_animating_ = false;
    }

    // Animate scroll: direction = -1 (up) or +1 (down)
    void animate_scroll(int direction)
    {
        int count = (int)menu_items_.size();
        int new_idx = selected_idx_ + direction;

        // Always bounce the arrow (even at boundary)
        if (direction < 0) bounce_arrow(arrow_up_obj_, -1);
        else bounce_arrow(arrow_down_obj_, 1);

        if (new_idx < 0 || new_idx >= count) return;
        if (is_animating_) return;

        is_animating_ = true;
        selected_idx_ = new_idx;

        // Arrows always visible — bounce only when not at boundary
        // (animate_scroll already blocks at boundaries)

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
        static constexpr int SEL_BAR_H = 23;
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

        // Sub items — two passes: create labels, then place indicators aligned
        struct SubLabelInfo { lv_obj_t *lbl; int si; int right_edge; };
        SubLabelInfo sub_labels[ROWS_VISIBLE] = {};
        int sub_label_count = 0;
        int right_min_x = SCREEN_W;
        int right_max_edge = 0;

        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            int si = sub_selected_idx_ - sub_center_vi + vi;
            if (si < 0 || si >= sub_count) continue;

            SubItem &sub = item.sub_items[si];
            lv_obj_t *lbl = create_carousel_label(cont, vi, sub_center_vi,
                                                   sub.label.c_str(), SUB_CENTER_X, true);
            lv_obj_update_layout(lbl);
            int lx = lv_obj_get_x(lbl);
            int tw = lv_obj_get_width(lbl);
            if (lx < right_min_x) right_min_x = lx;
            if (lx + tw > right_max_edge) right_max_edge = lx + tw;

            sub_labels[sub_label_count++] = {lbl, si, lx + tw};
        }

        // Place toggle indicators at fixed x=220
        int indicator_x = 220;
        for (int i = 0; i < sub_label_count; ++i) {
            SubItem &sub = item.sub_items[sub_labels[i].si];
            if (!sub.is_toggle) continue;

            lv_obj_t *lbl = sub_labels[i].lbl;
            lv_obj_t *ind = lv_img_create(cont);
            lv_img_set_src(ind, sub.toggle_state ? img_ok_.c_str() : img_cross_.c_str());
            // Vertically center indicator with the label
            lv_obj_update_layout(ind);
            int ind_h = lv_obj_get_height(ind);
            int lbl_y = lv_obj_get_y(lbl);
            int lbl_h = lv_obj_get_height(lbl);
            int x_offset = sub.toggle_state ? 0 : 1;
            lv_obj_set_pos(ind, indicator_x + x_offset, lbl_y + (lbl_h - ind_h) / 2);
        }

        // Blue arrow centered between left text right edge and right column left edge
        if (left_center_lbl && sub_count > 0)
            place_blue_arrow(cont, left_center_lbl, right_min_x);

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

        // Hint for selected sub item (right-aligned, 6px from right edge, smaller font)
        SubItem &cur_sub = item.sub_items[sub_selected_idx_];
        lv_obj_t *hint = lv_label_create(cont);
        if (cur_sub.is_toggle)
            lv_label_set_text(hint, cur_sub.toggle_state ? "ok:hide" : "ok:select");
        else
            lv_label_set_text(hint, "ok:enter");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x00CC66), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
        lv_obj_update_layout(hint);
        int sub_hint_w = lv_obj_get_width(hint);
        int sub_hint_h = lv_obj_get_height(hint);
        lv_obj_set_pos(hint, SCREEN_W - 6 - sub_hint_w, row_y(sub_center_vi) + (row_h() - sub_hint_h) / 2);
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

        // Right column: value options — track min X for stable arrow
        static constexpr int VAL_CENTER_X = 160;
        int val_right_min_x = SCREEN_W;
        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            int val_i = val_sel_idx_ - ROW_CENTER + vi;
            if (val_i < 0 || val_i >= val_count) continue;
            lv_obj_t *lbl = create_carousel_label(cont, vi, ROW_CENTER,
                                                   val_options_[val_i].c_str(), VAL_CENTER_X, true);
            lv_obj_update_layout(lbl);
            int lx = lv_obj_get_x(lbl);
            if (lx < val_right_min_x) val_right_min_x = lx;
        }

        // Blue arrow centered between left and right columns
        if (val_left_lbl && val_count > 0)
            place_blue_arrow(cont, val_left_lbl, val_right_min_x);

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

        // Hint (right-aligned, smaller font)
        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "ok:set");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x00CC66), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
        lv_obj_update_layout(hint);
        int val_hint_w = lv_obj_get_width(hint);
        int val_hint_h = lv_obj_get_height(hint);
        lv_obj_set_pos(hint, SCREEN_W - 6 - val_hint_w, row_y(ROW_CENTER) + (row_h() - val_hint_h) / 2);
    }

    void rebuild_view()
    {
        if (view_state_ == ViewState::MAIN) build_main_view();
        else if (view_state_ == ViewState::SUB) build_sub_view();
        else if (view_state_ == ViewState::VALUE_SELECT) build_value_view();
        else if (view_state_ == ViewState::WIFI_LIST) build_wifi_list();
    }

    // Bounce animation for orange arrows (small Y displacement + return)
    void bounce_arrow(lv_obj_t *arrow, int direction)
    {
        if (!arrow) return;
        int cur_y = lv_obj_get_y(arrow);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, arrow);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(&a, cur_y + direction * 4, cur_y);
        lv_anim_set_time(&a, 150);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }

    // Dual-container slide transition
    // direction: +1 = enter deeper (old slides left, new slides in from right)
    //            -1 = go back (old slides right, new slides in from left)
    void slide_transition(int direction)
    {
        lv_obj_t *bg = ui_obj_["bg"];
        lv_obj_t *old_cont = ui_obj_["list_cont"];
        if (!bg || !old_cont) { rebuild_view(); return; }

        // Create new container for the new content
        lv_obj_t *new_cont = lv_obj_create(bg);
        lv_obj_set_size(new_cont, SCREEN_W, LIST_H);
        lv_obj_set_pos(new_cont, direction * SCREEN_W, 0);
        lv_obj_set_style_radius(new_cont, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(new_cont, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(new_cont, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(new_cont, 0, LV_PART_MAIN);
        lv_obj_clear_flag(new_cont, LV_OBJ_FLAG_SCROLLABLE);

        // Switch list_cont to new container and rebuild
        ui_obj_["list_cont"] = new_cont;
        rebuild_view();

        // Animate old container sliding out
        lv_anim_t a_old;
        lv_anim_init(&a_old);
        lv_anim_set_var(&a_old, old_cont);
        lv_anim_set_exec_cb(&a_old, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_set_values(&a_old, 0, -direction * SCREEN_W);
        lv_anim_set_time(&a_old, 200);
        lv_anim_set_path_cb(&a_old, lv_anim_path_ease_out);
        lv_anim_set_completed_cb(&a_old, slide_old_done_cb);
        lv_anim_set_user_data(&a_old, old_cont);
        lv_anim_start(&a_old);

        // Animate new container sliding in
        lv_anim_t a_new;
        lv_anim_init(&a_new);
        lv_anim_set_var(&a_new, new_cont);
        lv_anim_set_exec_cb(&a_new, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_set_values(&a_new, direction * SCREEN_W, 0);
        lv_anim_set_time(&a_new, 200);
        lv_anim_set_path_cb(&a_new, lv_anim_path_ease_out);
        lv_anim_start(&a_new);
    }

    static void slide_old_done_cb(lv_anim_t *a)
    {
        lv_obj_t *old_cont = (lv_obj_t *)lv_anim_get_user_data(a);
        if (old_cont) lv_obj_del(old_cont);
    }

    // Enter deeper level (old slides left, new from right)
    void transition_enter_level()
    {
        slide_transition(1);
    }

    // Return to shallower level (old slides right, new from left)
    void transition_back_level()
    {
        slide_transition(-1);
    }

    // ==================== Events ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(root_screen_, UISetupPage::static_handler, LV_EVENT_ALL, this);
    }
    static void static_handler(lv_event_t *e)
    {
        UISetupPage *self = static_cast<UISetupPage *>(lv_event_get_user_data(e));
        if (self) self->on_event(e);
    }

    uint32_t last_repeat_tick_ = 0;
    static constexpr uint32_t REPEAT_INTERVAL_MS = 300;

    void on_event(lv_event_t *e)
    {
        bool released = launcher_ui::events::is_key_released(e);
        bool pressed = launcher_ui::events::is_key_pressed(e);
        if (!released && !pressed) return;

        struct key_item *elm = (struct key_item *)lv_event_get_param(e);
        cur_elm_ = elm;
        uint32_t key = elm->key_code;
        key = remap_fzxc(key);

        // For held keys (pressed), only handle UP/DOWN with throttle
        if (pressed) {
            if (key != KEY_UP && key != KEY_DOWN) return;
            uint32_t now = lv_tick_get();
            if (now - last_repeat_tick_ < REPEAT_INTERVAL_MS) return;
            last_repeat_tick_ = now;
        } else {
            // On release, also throttle to prevent double-trigger
            uint32_t now = lv_tick_get();
            if (key == KEY_UP || key == KEY_DOWN) {
                if (now - last_repeat_tick_ < 300) return;
                last_repeat_tick_ = now;
            }
        }

        switch (view_state_) {
        case ViewState::MAIN:         handle_main_key(key); break;
        case ViewState::SUB:          handle_sub_key(key);  break;
        case ViewState::VALUE_SELECT: handle_value_key(key); break;
        case ViewState::WIFI_LIST:
            // UP/DOWN: only on pressed (throttled above). Other keys: only on released.
            if (pressed && (key == KEY_UP || key == KEY_DOWN))
                handle_wifi_list_key(key);
            else if (released && key != KEY_UP && key != KEY_DOWN)
                handle_wifi_list_key(key);
            break;
        case ViewState::WIFI_PW:
            if (released) handle_wifi_pw_key(key);
            break;
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
            play_enter();
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
            play_back();
            if (navigate_home) navigate_home();
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
            play_enter();
            if (sub_selected_idx_ < sub_count) {
                SubItem &sub = item.sub_items[sub_selected_idx_];
                if (sub.is_toggle) {
                    sub.toggle_state = !sub.toggle_state;
                    if (sub.action) sub.action();
                    build_sub_view();
                } else if (sub.action) {
                    sub.action();
                }
            }
            break;
        }
        case KEY_ESC:
        case KEY_LEFT:
            play_back();
            stop_info_timer();
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
            // After reboot/shutdown, don't animate back — the system is going down.
            if (val_title_ == "Reboot?" || val_title_ == "Shutdown?") {
                // Show a brief message, then let the system halt/reboot.
                lv_obj_t *cont = ui_obj_["list_cont"];
                lv_obj_clean(cont);
                lv_obj_t *lbl = lv_label_create(cont);
                lv_label_set_text(lbl, val_title_ == "Reboot?" ? "Rebooting..." : "Shutting down...");
                lv_obj_center(lbl);
                lv_obj_set_style_text_color(lbl, lv_color_hex(0x58A6FF), LV_PART_MAIN);
                lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
                lv_refr_now(NULL);
                break;
            }
            view_state_ = ViewState::SUB;
            transition_back_level();
            break;
        case KEY_ESC:
        case KEY_LEFT:
            view_state_ = ViewState::SUB;
            transition_back_level();
            break;
        default:
            break;
        }
    }
};
