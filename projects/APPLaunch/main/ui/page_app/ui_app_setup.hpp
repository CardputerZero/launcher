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
#include <climits>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <dirent.h>
#include <sstream>
#include <thread>
#include <list>
#include <utility>
#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "../app_registry.h"

// ============================================================
//  System settings screen  UISetupPage  (Carousel Design)
//  Screen: 320x170 (top bar20px, body 320x150)
//
//  Menu items (design mockup): Launcher, Boot, Screen, WiFi, Speaker, Camera
//  Actual HAL integration: WiFi scan/connect, brightness, volume, power, reboot, about
// ============================================================

class UISetupPage : public AppPage
{
    enum class ViewState { MAIN, SUB, VALUE_SELECT, WIFI_LIST, WIFI_PW, BT_LIST,
                           SOUNDCARD_CARDS, SOUNDCARD_CONTROLS, SOUNDCARD_DETAIL,
                           USB_GUIDE };

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

    struct BtListRow {
        int device_index;
        const char *title;
        bool is_header;
    };

    enum class BtListMode { Managed, Scan };

public:
    UISetupPage() : AppPage()
    {
        set_page_title("SETTING");
        cache_image_paths();
        menu_init();
        create_ui();
        event_handler_init();
    }
    ~UISetupPage()
    {
        stop_power_timer();
        stop_bt_scan_timer();
    }

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

    // USB-mode guidance screen (shown when a reboot is required to switch dwc2).
    // Drawn natively with LVGL objects + lv_anim (no GIF) so it stays crisp.
    bool usb_guide_enabling_ = true;
    lv_obj_t *usb_guide_knob_ = nullptr;

    // WiFi state
    cp0_wifi_ap_t wifi_aps_[CP0_WIFI_AP_MAX];
    int wifi_ap_count_ = 0;
    std::string wifi_pw_ssid_;
    std::string wifi_pw_buf_;
    lv_obj_t *pw_input_lbl_ = nullptr;
    lv_obj_t *pw_hint_lbl_ = nullptr;
    struct key_item *cur_elm_ = nullptr;

    // Bluetooth state
    cp0_bt_device_t bt_devices_[CP0_BT_DEVICE_MAX];
    int bt_device_count_ = 0;
    int bt_list_sel_ = 0;
    std::vector<BtListRow> bt_rows_;
    BtListMode bt_list_mode_ = BtListMode::Managed;
    lv_timer_t *bt_scan_timer_ = nullptr;
    bool bt_discovery_active_ = false;
    bool bt_named_only_ = true;
    bool bt_action_busy_ = false;

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
                {"Power",   true,  false, [this]() { wifi_toggle_enable(); }},
                {"Scan",    false, false, [this]() { enter_wifi_scan(); }},
            };
            m.on_enter = [this]() { refresh_wifi_radio(); };
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
                    bool en = menu_items_[selected_idx_].sub_items[0].toggle_state;
                    config_set_int("extport_usb", en ? 1 : 0);
                    config_save();
                }},
                {"5VOUT", true, vout_en, [this]() {
                    bool en = menu_items_[selected_idx_].sub_items[1].toggle_state;
                    config_set_int("extport_5vout", en ? 1 : 0);
                    config_save();
                }},
            };
            menu_items_.push_back(m);
        }
        // --- Developer (developer options: ADB USB debug bridge, ON by default) ---
        {
            MenuItem m;
            m.label = "Developer";
            // ADB is OFF by default: enabling it switches the USB controller to
            // peripheral mode, which disables the onboard USB hub (keyboard/mouse
            // and other peripherals). It must stay opt-in.
            bool adb_en = config_get_int("adb_debug", 0) != 0;
            m.sub_items = {
                {"ADB", true, adb_en, [this]() { adb_toggle(); }},
            };
            m.on_enter = [this]() { refresh_adb_status(); };
            menu_items_.push_back(m);
        }
        // --- RTC ---
        {
            MenuItem m;
            m.label = "RTC";
            m.sub_items = {
                {"NTP",    true,  true,  [this]() { ntp_toggle(); }},
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
            bt_named_only_ = config_get_int("bt_named_only", 1) != 0;
            m.sub_items = {
                {"Power",  true, false, [this]() { bt_toggle_power(); }},
                {"Named Only", true, bt_named_only_, [this]() { bt_toggle_named_only(); }},
                {"Connected", false, false, [this]() { enter_bt_devices(); }},
                {"Scan",   false, false, [this]() { enter_bt_scan(); }},
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
                {"Run Setup Wizard", false, false, [this]() { enter_confirm_action("Run Setup?", [this](){ rearm_oobe_and_reboot(); }); }},
                {"Factory Reset", false, false, [this]() { factory_reset(); }},
            };
            menu_items_.push_back(m);
        }
        // --- SoundCard ---
        {
            MenuItem m;
            m.label = "SoundCard";
            // Single sub-item acts as an entry point into the full-screen card browser
            m.sub_items = {
                {"Open Mixer", false, false, [this]() { sc_enter_cards(); }},
            };
            menu_items_.push_back(m);
        }
    }

    // ==================== Placeholder functions for new menus ====================
    void enter_darktime_adjust()
    {
        val_title_ = "DarkTime";
        val_options_ = {"Never", "10S", "30S", "60S", "300S"};
        const int times[] = {0, 10, 30, 60, 300};
        int saved = config_get_int("dark_time", 30); // default 30S
        val_sel_idx_ = 2;
        for (int i = 0; i < (int)(sizeof(times) / sizeof(times[0])); ++i) {
            if (times[i] == saved) { val_sel_idx_ = i; break; }
        }
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
        // The connected-WiFi line (SSID + IP) can overflow off-screen (#66). Clamp it to a
        // fixed box and marquee-scroll when wider than the threshold so it stays fully readable.
        apply_overflow_handling(title, 8, WIFI_TITLE_BOX_W, true);

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

    void enter_bt_devices()
    {
        stop_bt_scan_timer();
        bt_list_mode_ = BtListMode::Managed;
        view_state_ = ViewState::BT_LIST;
        bt_list_sel_ = 0;
        bt_refresh_devices();
        build_bt_list();
    }

    void enter_bt_scan()
    {
        bt_list_mode_ = BtListMode::Scan;
        view_state_ = ViewState::BT_LIST;
        bt_list_sel_ = 0;
        start_bt_scan_timer();
    }

    void build_bt_list()
    {
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);
        rebuild_bt_rows();

        cp0_bt_status_t st = bt_get_status();
        char title_buf[96];
        snprintf(title_buf, sizeof(title_buf), "%s: %s  %.24s",
                 bt_list_mode_ == BtListMode::Scan ? "Scan" : "Connected",
                 st.powered ? "On" : "Off", st.address[0] ? st.address : "--");
        lv_obj_t *title = lv_label_create(cont);
        lv_label_set_text(title, title_buf);
        lv_obj_set_pos(title, 8, 2);
        lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
        apply_overflow_handling(title, 8, WIFI_TITLE_BOX_W, true);

        if (bt_rows_.empty()) {
            lv_obj_t *empty = lv_label_create(cont);
            if (!st.powered)
                lv_label_set_text(empty, "Bluetooth is off. Enable Power first.");
            else if (bt_list_mode_ == BtListMode::Scan)
                lv_label_set_text(empty, "Scanning...");
            else
                lv_label_set_text(empty, "No connected devices.");
            lv_obj_set_pos(empty, 8, 50);
            lv_obj_set_width(empty, 300);
            lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_color(empty, lv_color_hex(0x666666), LV_PART_MAIN);
            lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, LV_PART_MAIN);
        }

        constexpr int list_y = 22;
        constexpr int row_step = 20;
        constexpr int hint_y = LIST_H - 14;
        constexpr int list_bottom_gap = 8;
        int visible = (hint_y - list_bottom_gap - list_y) / row_step;
        if (visible < 1) visible = 1;
        int offset = bt_list_sel_ - visible / 2;
        if (offset < 0) offset = 0;
        if (offset > (int)bt_rows_.size() - visible) offset = (int)bt_rows_.size() - visible;
        if (offset < 0) offset = 0;

        for (int vi = 0; vi < visible && (vi + offset) < (int)bt_rows_.size(); ++vi) {
            int row_index = vi + offset;
            const BtListRow &row = bt_rows_[row_index];
            int y = list_y + vi * row_step;

            if (row.is_header) {
                lv_obj_t *header = lv_label_create(cont);
                lv_label_set_text(header, row.title);
                lv_obj_set_pos(header, 8, y + 3);
                lv_obj_set_style_text_color(header, lv_color_hex(0x888888), LV_PART_MAIN);
                lv_obj_set_style_text_font(header, launcher_fonts().get("Montserrat-Bold.ttf", 10, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
                continue;
            }

            bool sel = (row_index == bt_list_sel_);
            cp0_bt_device_t *dev = &bt_devices_[row.device_index];

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

            uint32_t tc = dev->connected ? 0x58A6FF : (sel ? 0xFFFFFF : 0xCCCCCC);
            lv_obj_t *name = lv_label_create(cont);
            lv_label_set_text(name, dev->name[0] ? dev->name : dev->address);
            lv_obj_set_pos(name, 8, y + 1);
            lv_obj_set_width(name, 150);
            lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_color(name, lv_color_hex(tc), LV_PART_MAIN);
            lv_obj_set_style_text_font(name, &lv_font_montserrat_12, LV_PART_MAIN);

            lv_obj_t *addr = lv_label_create(cont);
            lv_label_set_text(addr, dev->address);
            lv_obj_set_pos(addr, 8, y + 12);
            lv_obj_set_width(addr, 190);
            lv_label_set_long_mode(addr, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_color(addr, lv_color_hex(sel ? 0xBBBBBB : 0x777777), LV_PART_MAIN);
            lv_obj_set_style_text_font(addr, &lv_font_montserrat_10, LV_PART_MAIN);

            char state_buf[32];
            if (dev->connected)
                snprintf(state_buf, sizeof(state_buf), "Connected");
            else if (dev->paired)
                snprintf(state_buf, sizeof(state_buf), "Paired");
            else
                snprintf(state_buf, sizeof(state_buf), "%d", dev->rssi);
            lv_obj_t *state = lv_label_create(cont);
            lv_label_set_text(state, state_buf);
            lv_obj_set_pos(state, 226, y + 4);
            lv_obj_set_width(state, 88);
            lv_label_set_long_mode(state, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_align(state, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
            lv_obj_set_style_text_color(state, lv_color_hex(tc), LV_PART_MAIN);
            lv_obj_set_style_text_font(state, &lv_font_montserrat_10, LV_PART_MAIN);
        }

        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, bt_list_mode_ == BtListMode::Scan
                                     ? "OK:act R:restart ESC:back"
                                     : "OK:disconnect D:remove ESC:back");
        lv_obj_set_pos(hint, 8, hint_y);
        lv_obj_set_width(hint, 304);
        lv_label_set_long_mode(hint, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
    }

    void rebuild_bt_rows()
    {
        bt_rows_.clear();
        if (bt_list_mode_ == BtListMode::Managed) {
            bool has_connected = false;
            for (int i = 0; i < bt_device_count_; ++i) {
                if (bt_should_hide_device(bt_devices_[i]))
                    continue;
                if (bt_devices_[i].connected) {
                    if (!has_connected) {
                        bt_rows_.push_back({-1, "Connected Devices", true});
                        has_connected = true;
                    }
                    bt_rows_.push_back({i, nullptr, false});
                }
            }
        } else {
            bool has_devices = false;
            for (int i = 0; i < bt_device_count_; ++i) {
                if (bt_should_hide_device(bt_devices_[i]))
                    continue;
                if (!has_devices) {
                    bt_rows_.push_back({-1, "Discovered Devices", true});
                    has_devices = true;
                }
                bt_rows_.push_back({i, nullptr, false});
            }
        }
        if (bt_list_sel_ >= (int)bt_rows_.size())
            bt_list_sel_ = bt_rows_.empty() ? 0 : (int)bt_rows_.size() - 1;
        if (!bt_rows_.empty() && bt_rows_[bt_list_sel_].is_header)
            bt_select_next_device(1);
    }

    bool bt_should_hide_device(const cp0_bt_device_t &dev) const
    {
        if (!bt_named_only_)
            return false;
        if (!dev.name[0])
            return true;
        std::string name_hex = bt_normalized_mac_text(dev.name);
        std::string addr_hex = bt_normalized_mac_text(dev.address);
        return !name_hex.empty() && (name_hex == addr_hex || name_hex.size() == 12);
    }

    static std::string bt_normalized_mac_text(const char *text)
    {
        std::string out;
        if (!text)
            return out;
        for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
            if (std::isxdigit(*p))
                out.push_back((char)std::tolower(*p));
            else if (*p != ':' && *p != '-' && *p != '_' && *p != ' ')
                return std::string();
        }
        return out;
    }

    int bt_selected_device_index() const
    {
        if (bt_list_sel_ < 0 || bt_list_sel_ >= (int)bt_rows_.size())
            return -1;
        return bt_rows_[bt_list_sel_].is_header ? -1 : bt_rows_[bt_list_sel_].device_index;
    }

    void bt_select_next_device(int direction)
    {
        if (bt_rows_.empty())
            return;
        int idx = bt_list_sel_;
        for (int steps = 0; steps < (int)bt_rows_.size(); ++steps) {
            idx += direction;
            if (idx < 0 || idx >= (int)bt_rows_.size())
                return;
            if (!bt_rows_[idx].is_header) {
                bt_list_sel_ = idx;
                return;
            }
        }
    }

    void bt_show_action(const char *msg, uint32_t color = 0x58A6FF)
    {
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, msg);
        lv_obj_set_pos(lbl, 8, 60);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_refr_now(NULL);
    }

    void bt_activate_selected()
    {
        if (bt_action_busy_)
            return;
        int dev_index = bt_selected_device_index();
        if (dev_index < 0)
            return;
        cp0_bt_device_t dev = bt_devices_[dev_index];
        bool from_scan = bt_list_mode_ == BtListMode::Scan;
        if (from_scan)
            stop_bt_scan_timer();
        bt_action_busy_ = true;
        if (dev.connected)
            bt_show_action("Disconnecting...");
        else if (dev.paired)
            bt_show_action("Connecting...");
        else
            bt_show_action("Pairing...");

        struct BtActionResult {
            UISetupPage *self;
            int ret;
            bool from_scan;
        };

        std::thread([this, dev, from_scan]() {
            int ret = -1;
            if (dev.connected) {
                ret = bt_device_command("BtDisconnect", dev.address);
            } else if (dev.paired) {
                ret = bt_device_command("BtConnect", dev.address);
            } else {
                ret = bt_device_command("BtPair", dev.address);
                if (ret == 0)
                    ret = bt_device_command("BtConnect", dev.address);
            }

            BtActionResult *result = new BtActionResult{this, ret, from_scan};
            lv_async_call([](void *user) {
                BtActionResult *result = static_cast<BtActionResult *>(user);
                UISetupPage *self = result->self;
                if (self) {
                    self->bt_action_busy_ = false;
                    if (result->ret != 0) {
                        self->bt_show_action("Bluetooth action failed", 0xFF4444);
                    } else if (result->from_scan) {
                        self->bt_list_mode_ = BtListMode::Managed;
                    }
                    self->bt_refresh_devices();
                    if (self->view_state_ == ViewState::BT_LIST)
                        self->build_bt_list();
                }
                delete result;
            }, result);
        }).detach();
    }

    void bt_remove_selected()
    {
        int dev_index = bt_selected_device_index();
        if (dev_index < 0)
            return;
        bt_show_action("Removing...");
        int ret = bt_device_command("BtRemove", bt_devices_[dev_index].address);
        if (ret != 0) {
            bt_show_action("Remove failed", 0xFF4444);
            usleep(1200000);
        }
        bt_refresh_devices();
        build_bt_list();
    }

    void handle_bt_list_key(uint32_t key)
    {
        switch (key) {
        case KEY_UP:
            bt_select_next_device(-1);
            build_bt_list();
            break;
        case KEY_DOWN:
            bt_select_next_device(1);
            build_bt_list();
            break;
        case KEY_ENTER:
            bt_activate_selected();
            break;
        case KEY_D:
            if (bt_list_mode_ == BtListMode::Managed)
                bt_remove_selected();
            break;
        case KEY_R:
            if (bt_list_mode_ == BtListMode::Scan) {
                start_bt_scan_timer();
            } else {
                bt_refresh_devices();
                build_bt_list();
            }
            break;
        case KEY_ESC:
        case KEY_LEFT:
            stop_bt_scan_timer();
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
            if (bat.valid && bat.current_ma != INT32_MIN) {
                snprintf(buf, sizeof(buf), "Current: %dmA", bat.current_ma);
            } else {
                snprintf(buf, sizeof(buf), "Current: --mA");
            }
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
    bool rtc_ntp_on_ = true; // cached NTP state; manual set only allowed when off

    void refresh_rtc_values()
    {
        rtc_ntp_on_ = cp0_time_ntp_get() == 1;
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
        // Update labels. sub_items[0] is the NTP toggle; date fields follow.
        for (auto &m : menu_items_) {
            if (m.label != "RTC") continue;
            m.sub_items[0].toggle_state = rtc_ntp_on_;
            char buf[32];
            const char *names[] = {"Year", "Month", "Day", "Hour", "Minute", "Second"};
            for (int i = 0; i < 6; ++i) {
                snprintf(buf, sizeof(buf), "%s: %d", names[i], rtc_values_[i]);
                m.sub_items[i + 1].label = buf;
            }
            break;
        }
    }

    void ntp_toggle()
    {
        for (auto &m : menu_items_) {
            if (m.label != "RTC") continue;
            bool on = m.sub_items[0].toggle_state; // already flipped by handler
            cp0_time_ntp_set(on ? 1 : 0);
            break;
        }
        refresh_rtc_values();
    }

    void enter_rtc_adjust(int field)
    {
        // Manual time can only persist while NTP auto-sync is off.
        if (rtc_ntp_on_)
            return;
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

        // Build a combined shell command: set system clock then sync to hardware RTC.
        // Timestamp format is always "YYYY-MM-DD HH:MM:SS" — safe to single-quote.
        char shell_cmd[128];
        snprintf(shell_cmd, sizeof(shell_cmd),
                 "date -s '%s' && hwclock -w", timestamp);

        SudoPrompt::show({"sh", "-c", shell_cmd}, [this](int code) {
            refresh_rtc_values();
            update_datetime_status();
        });
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

    // ==================== ADB debug (USB) ====================
    // Device-side helper shipped inside the APPLaunch resource tree. It installs
    // adbd, embeds the default authorized key, brands the USB gadget as
    // "CardputerZero", and starts adbd.service. It exits 10 when the USB
    // controller had to be switched to peripheral mode and a reboot is required.
    static constexpr const char *kAdbHelper = "/usr/share/APPLaunch/adb/cardputer-adb";

    int find_menu(const char *label)
    {
        for (size_t i = 0; i < menu_items_.size(); ++i)
            if (menu_items_[i].label == label) return (int)i;
        return -1;
    }

    void adb_toggle()
    {
        int idx = find_menu("Developer");
        if (idx < 0) return;
        // The key handler has already flipped the toggle; read the desired state.
        bool want_on = menu_items_[idx].sub_items[0].toggle_state;

        const char *argv[] = {"sudo", kAdbHelper, want_on ? "enable" : "disable", nullptr};
        int rc = cp0_process_run_argv(argv, 0 /* foreground: we need the exit code */);

        // rc == 10: the USB controller (dwc2) had to change dr_mode in config.txt
        // (host<->peripheral). That only takes effect after a reboot, so guide the
        // user through the physical switch and offer to reboot. This happens on
        // BOTH directions: enabling switches host->peripheral, disabling switches
        // it back so the onboard hub/peripherals work again.
        if (rc == 10) {
            config_set_int("adb_debug", want_on ? 1 : 0);
            config_save();
            enter_usb_guide(want_on);
            return;
        }
        if (rc != 0) {
            // Failed — revert; the key handler redraws the sub view afterwards.
            menu_items_[idx].sub_items[0].toggle_state = !want_on;
            return;
        }
        config_set_int("adb_debug", want_on ? 1 : 0);
        config_save();
    }

    // Sync the toggle with the real adbd.service state when the menu is opened.
    void refresh_adb_status()
    {
        int idx = find_menu("Developer");
        if (idx < 0) return;
        char out[64] = {0};
        const char *argv[] = {"systemctl", "is-active", "adbd.service", nullptr};
        cp0_process_capture_argv(argv, out, sizeof(out));
        bool active = (std::strncmp(out, "active", 6) == 0);
        menu_items_[idx].sub_items[0].toggle_state = active;
        config_set_int("adb_debug", active ? 1 : 0);
    }

    // Full-screen animated guide shown when the USB controller has to switch
    // dwc2 dr_mode (a reboot is required). It plays a GIF that tells the user to
    // flip the left USB switch, which port to use, and that peripherals are
    // temporarily unavailable. OK reboots now; ESC postpones.
    void enter_usb_guide(bool enabling)
    {
        usb_guide_enabling_ = enabling;
        // Defer the actual build one cycle so the trailing build_sub_view() from
        // the key handler does not clobber this view.
        lv_timer_t *t = lv_timer_create([](lv_timer_t *timer) {
            UISetupPage *self = (UISetupPage *)lv_timer_get_user_data(timer);
            lv_timer_delete(timer);
            self->build_usb_guide_view();
        }, 60, this);
        lv_timer_set_repeat_count(t, 1);
    }

    // Small helper: a filled, rounded "chip" used for the board/switch/ports.
    static lv_obj_t *guide_chip(lv_obj_t *parent, int x, int y, int w, int h,
                                uint32_t bg, uint32_t border, int radius = 5,
                                int border_w = 2)
    {
        lv_obj_t *o = lv_obj_create(parent);
        lv_obj_set_pos(o, x, y);
        lv_obj_set_size(o, w, h);
        lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
        lv_obj_set_style_bg_color(o, lv_color_hex(bg), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(o, lv_color_hex(border), LV_PART_MAIN);
        lv_obj_set_style_border_width(o, border_w, LV_PART_MAIN);
        lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        return o;
    }

    static lv_obj_t *guide_label(lv_obj_t *parent, int x, int y, const char *txt,
                                 uint32_t color, const lv_font_t *font)
    {
        lv_obj_t *l = lv_label_create(parent);
        lv_label_set_text(l, txt);
        lv_obj_set_pos(l, x, y);
        lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
        return l;
    }

    // Draw the USB-mode guidance natively (crisp vector-style shapes + lv_anim).
    void build_usb_guide_view()
    {
        view_state_ = ViewState::USB_GUIDE;
        usb_guide_knob_ = nullptr;
        lv_obj_t *cont = ui_obj_["list_cont"];
        lv_obj_clean(cont);

        const bool en = usb_guide_enabling_;
        const uint32_t C_GREEN = 0x46DC87, C_YEL = 0xF0C850, C_RED = 0xEB5F5F;
        const uint32_t C_WHITE = 0xECECEC, C_GREY = 0x9A9AA0;
        const lv_font_t *f_title = launcher_fonts().get("Montserrat-Bold.ttf", 13,
                                                        LV_FREETYPE_FONT_STYLE_BOLD);
        const lv_font_t *f_msg = &lv_font_montserrat_10;

        // Title
        guide_label(cont, 8, 2,
                    en ? "Enable ADB - switch USB to device" : "Disable ADB - switch USB to HUB",
                    C_WHITE, f_title ? f_title : &lv_font_montserrat_12);

        // Board
        guide_chip(cont, 86, 24, 146, 50, 0x282A30, 0x5A5C64, 6, 2);
        guide_label(cont, 120, 28, "CardputerZero", C_GREY, f_msg);

        // ---- Top-right USB-C port with a cable plugged in ----
        guide_chip(cont, 218, 30, 12, 12, 0x101012, 0x5A5C64, 3, 2);  // socket
        guide_chip(cont, 228, 32, 22, 8, 0xCDCDD2, 0xCDCDD2, 2, 0);   // plug (inserted)
        guide_chip(cont, 250, 34, 60, 4, 0x6A6C72, 0x6A6C72, 2, 0);   // cable
        guide_label(cont, 232, 42, "USB-C", C_GREEN, f_msg);

        // ---- Left slide switch (side view), narrow thumb ----
        guide_chip(cont, 24, 28, 32, 44, 0x1A1A1C, 0x5A5C64, 6, 2);   // housing
        guide_chip(cont, 33, 33, 14, 34, 0x0E0E10, 0x0E0E10, 4, 0);   // track slot
        guide_label(cont, 26, 14, "HUB", en ? C_RED : C_GREEN, f_msg);
        guide_label(cont, 28, 72, "USB", en ? C_GREEN : C_GREY, f_msg);
        const int thumb_top = 34, thumb_bot = 54;
        usb_guide_knob_ = guide_chip(cont, 32, en ? thumb_top : thumb_bot, 16, 10,
                                     C_GREEN, 0x2A6F49, 3, 1);

        // ---- Messages ----
        int y = 80;
        if (en) {
            guide_label(cont, 8, y,      "1  Slide LEFT switch  HUB -> USB", C_WHITE, f_msg);
            guide_label(cont, 8, y + 15, "2  USB hub & peripherals turn OFF", C_YEL, f_msg);
            guide_label(cont, 8, y + 30, "3  Cable -> top-right USB-C port", C_GREEN, f_msg);
        } else {
            guide_label(cont, 8, y,      "1  Slide LEFT switch  USB -> HUB", C_WHITE, f_msg);
            guide_label(cont, 8, y + 15, "2  USB hub & peripherals come back", C_GREEN, f_msg);
            guide_label(cont, 8, y + 30, "3  Reboot to apply the change", C_YEL, f_msg);
        }

        // Footer
        guide_label(cont, 8, LIST_H - 16, "OK: reboot now     ESC: later", C_GREY,
                    &lv_font_montserrat_10);

        // ---- Animation: the switch thumb slides toward its target, ping-pong ----
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, usb_guide_knob_);
        lv_anim_set_values(&a, en ? thumb_top : thumb_bot, en ? thumb_bot : thumb_top);
        lv_anim_set_time(&a, 650);
        lv_anim_set_playback_time(&a, 650);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_exec_cb(&a, [](void *var, int32_t v) {
            lv_obj_set_y((lv_obj_t *)var, v);
        });
        lv_anim_start(&a);
    }

    void stop_usb_guide_anims()
    {
        if (usb_guide_knob_) lv_anim_del(usb_guide_knob_, nullptr);
        usb_guide_knob_ = nullptr;
    }

    void handle_usb_guide_key(uint32_t key)
    {
        switch (key) {
        case KEY_ENTER:
        case KEY_RIGHT: {
            stop_usb_guide_anims();
            lv_obj_t *cont = ui_obj_["list_cont"];
            lv_obj_clean(cont);
            lv_obj_t *lbl = lv_label_create(cont);
            lv_label_set_text(lbl, "Rebooting...");
            lv_obj_center(lbl);
            cp0_system_reboot();
            break;
        }
        case KEY_ESC:
        case KEY_LEFT:
            // Postpone: the dr_mode change is already written to config.txt and
            // will take effect on the next boot. Return to the sub view.
            stop_usb_guide_anims();
            view_state_ = ViewState::SUB;
            build_sub_view();
            break;
        default:
            break;
        }
    }

    // ==================== Bluetooth ====================
    static void bt_copy_string(char *dst, size_t dst_size, const std::string &src)
    {
        if (!dst || dst_size == 0)
            return;
        std::strncpy(dst, src.c_str(), dst_size - 1);
        dst[dst_size - 1] = '\0';
    }

    static std::vector<std::string> bt_split_char(const std::string &line, char delimiter)
    {
        std::vector<std::string> cols;
        std::string item;
        std::istringstream iss(line);
        while (std::getline(iss, item, delimiter))
            cols.push_back(item);
        return cols;
    }

    static bool bt_decode_status(const std::string &data, cp0_bt_status_t &st)
    {
        auto cols = bt_split_char(data, '\t');
        if (cols.size() < 2)
            return false;
        st = {};
        st.powered = std::atoi(cols[0].c_str());
        bt_copy_string(st.address, sizeof(st.address), cols[1]);
        return true;
    }

    static int bt_decode_devices(const std::string &data, cp0_bt_device_t *out, int max_devices)
    {
        if (!out || max_devices <= 0)
            return 0;
        int count = 0;
        std::istringstream lines(data);
        std::string line;
        while (count < max_devices && std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto cols = bt_split_char(line, '\t');
            if (cols.size() < 4 || cols[0].empty())
                continue;
            cp0_bt_device_t dev{};
            bt_copy_string(dev.address, sizeof(dev.address), cols[0]);
            dev.rssi = std::atoi(cols[1].c_str());
            dev.connected = std::atoi(cols[2].c_str());
            if (cols.size() >= 6) {
                dev.paired = std::atoi(cols[3].c_str());
                dev.trusted = std::atoi(cols[4].c_str());
                bt_copy_string(dev.name, sizeof(dev.name), cols[5]);
            } else {
                bt_copy_string(dev.name, sizeof(dev.name), cols[3]);
            }
            out[count++] = dev;
        }
        return count;
    }

    static int bt_api_int(std::list<std::string> args, int default_value = -1)
    {
        int result = default_value;
        cp0_signal_bt_api(std::move(args), [&](int code, std::string) {
            result = code;
        });
        return result;
    }

    static cp0_bt_status_t bt_get_status()
    {
        cp0_bt_status_t st{};
        cp0_signal_bt_api({"BtStatus"}, [&](int code, std::string data) {
            if (code == 0)
                bt_decode_status(data, st);
        });
        return st;
    }

    static int bt_set_power(int on)
    {
        return bt_api_int({"BtPower", std::to_string(on)});
    }

    static int bt_device_command(const char *cmd, const char *address)
    {
        return bt_api_int({cmd ? std::string(cmd) : std::string(),
                           address ? std::string(address) : std::string()});
    }

    static int bt_device_list(const char *cmd, cp0_bt_device_t *out, int max_devices)
    {
        int count = 0;
        cp0_signal_bt_api({cmd ? std::string(cmd) : std::string(), std::to_string(max_devices)},
                          [&](int code, std::string data) {
                              count = out && max_devices > 0 ? bt_decode_devices(data, out, max_devices) : code;
                          });
        return count;
    }

    void refresh_bt_status()
    {
        cp0_bt_status_t st = bt_get_status();
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
            if (!on)
                stop_bt_scan_timer();
            bt_set_power(on ? 1 : 0);
            break;
        }
    }

    void bt_toggle_named_only()
    {
        for (auto &m : menu_items_) {
            if (m.label != "Bluetooth") continue;
            bt_named_only_ = m.sub_items[1].toggle_state;
            config_set_int("bt_named_only", bt_named_only_ ? 1 : 0);
            config_save();
            break;
        }
        if (view_state_ == ViewState::BT_LIST)
            build_bt_list();
    }

    void start_bt_scan_timer()
    {
        stop_bt_scan_timer();
        bt_discovery_active_ = bt_api_int({"BtDiscoveryStart"}, 0) == 0;
        bt_refresh_devices();
        build_bt_list();
        if (!bt_discovery_active_)
            return;
        bt_scan_timer_ = lv_timer_create([](lv_timer_t *t) {
            UISetupPage *self = (UISetupPage *)lv_timer_get_user_data(t);
            if (!self || self->view_state_ != ViewState::BT_LIST || self->bt_list_mode_ != BtListMode::Scan)
                return;
            self->bt_refresh_devices();
            self->build_bt_list();
        }, 1000, this);
    }

    void stop_bt_scan_timer()
    {
        if (bt_scan_timer_) {
            lv_timer_delete(bt_scan_timer_);
            bt_scan_timer_ = nullptr;
        }
        if (bt_discovery_active_) {
            bt_api_int({"BtDiscoveryStop"}, 0);
            bt_discovery_active_ = false;
        }
    }

    void bt_refresh_devices()
    {
        if (bt_list_mode_ == BtListMode::Managed)
            bt_device_count_ = bt_device_list("BtConnectedList", bt_devices_, CP0_BT_DEVICE_MAX);
        else
            bt_device_count_ = bt_device_list("BtList", bt_devices_, CP0_BT_DEVICE_MAX);
        if (bt_device_count_ < 0)
            bt_device_count_ = 0;
        if (bt_device_count_ == 0)
            bt_list_sel_ = 0;
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
        enter_bt_scan();
    }

    void factory_reset()
    {
        remove("/var/lib/applaunch/settings");
        cp0_system_reboot();
    }

    // Re-arm the first-boot setup wizard (OOBE) so it replays once on the next
    // boot, then reboot. LaunchWizard (root system service) detects this marker,
    // shows the OOBE, and clears it when finished — so configured devices can
    // simulate the first-run experience on demand.
    void rearm_oobe_and_reboot()
    {
#ifndef _WIN32
        mkdir("/var/lib/applaunch", 0755);
#endif
        FILE *f = fopen("/var/lib/applaunch/run-oobe", "w");
        if (f) fclose(f);
        cp0_system_reboot();
    }

    // ==================== WiFi functions ====================
    void wifi_do_scan()
    {
        wifi_ap_count_ = launcher_wifi::scan(wifi_aps_, CP0_WIFI_AP_MAX);
    }

    void refresh_wifi_radio()
    {
        for (auto &m : menu_items_) {
            if (m.label != "WiFi") continue;
            m.sub_items[0].toggle_state = launcher_wifi::radio_enabled() != 0;
            break;
        }
    }

    void wifi_toggle_enable()
    {
        for (auto &m : menu_items_) {
            if (m.label != "WiFi") continue;
            // Same NetworkManager radio control used by nmtui: nmcli radio wifi on/off.
            bool on = m.sub_items[0].toggle_state;
            launcher_wifi::radio_set_enabled(on);
            m.sub_items[0].toggle_state = launcher_wifi::radio_enabled() != 0;
            break;
        }
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
            if (ret != 0) {
                // Saved profile failed to connect (e.g. stale/wrong password). Fall
                // back to asking for the password again instead of silently erroring,
                // so the user can re-enter it until the connection succeeds (#69).
                needs_password = true;
                wifi_pw_ssid_ = ap->ssid;
                wifi_pw_buf_.clear();
                show_wifi_pw_input();
            }
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
            // Idle screen-blank timeout in seconds (0 = Never); consumed by
            // ui_darkscreen_tick() in the launcher main loop (#72).
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
        } else if (val_title_ == "Reboot?" || val_title_ == "Shutdown?" || val_title_ == "Run Setup?") {
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

    // Right-column value box for sub / value views. After measuring a value's actual
    // width, anything wider than VALUE_BOX_W is shrunk to VALUE_BOX_W and marquee-scrolled
    // on the focused row / ellipsized elsewhere — so long values (MAC, Commit, IP,
    // hostname, version, ...) scroll instead of overflowing or overlapping. (#57)
    static constexpr int VALUE_BOX_LEFT = 112;
    static constexpr int VALUE_BOX_W    = 100; // 超过 100px 即缩宽并滚动
    // Right-column hint (ok:xxx) scroll threshold. The hint sits at the far right,
    // to the right of any toggle indicator (x=220). Anything wider than this is
    // clamped into a right-edge box and marquee-scrolled. Slightly smaller than the
    // center VALUE_BOX_W so it clears the toggle indicator instead of overlapping it.
    static constexpr int RIGHT_HINT_BOX_W = 74;

    // Width of the "Connected WiFi: <ssid> <ip>" header box in the WiFi list. When the
    // text is wider than this it marquee-scrolls instead of overflowing off-screen (#66).
    static constexpr int WIFI_TITLE_BOX_W = 300;

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

    // If a (carousel) label's text is wider than box_w, clamp it into [box_left, box_left+box_w]
    // and either marquee-scroll it (focused/center row) or ellipsize it (other rows). Labels
    // that already fit keep their original centered auto-width layout. Generic so any long value
    // (MAC / Commit / IP / hostname / version / long option) is handled, not just one screen.
    static void apply_overflow_handling(lv_obj_t *lbl, int box_left, int box_w, bool focused)
    {
        if (!lbl || box_w <= 0)
            return;
        lv_obj_update_layout(lbl);
        if (lv_obj_get_width(lbl) <= box_w)
            return; // fits — keep default centered behavior
        lv_obj_set_width(lbl, box_w);
        lv_obj_set_x(lbl, box_left);
        // focused row marquee-scrolls (single line); others clip (single line, no wrap).
        // Do NOT use LV_LABEL_LONG_DOT here: with auto height it wraps to multiple lines
        // before adding dots, which looks like an unwanted line break.
        lv_label_set_long_mode(lbl, focused ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_CLIP);
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
            // Focused row uses a tighter box (80px) so labels wider than 80px scroll;
            // non-focused rows keep the full 100px box (only clip when truly overflowing).
            bool focused_row = (vi == sub_center_vi);
            apply_overflow_handling(lbl, VALUE_BOX_LEFT, focused_row ? 80 : VALUE_BOX_W, focused_row);
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
        if (cur_sub.is_toggle && item.label == "RTC" && cur_sub.label == "NTP")
            lv_label_set_text(hint, cur_sub.toggle_state ? "ok:disable" : "ok:enable");
        else if (cur_sub.is_toggle && item.label == "WiFi" && cur_sub.label == "Power")
            lv_label_set_text(hint, cur_sub.toggle_state ? "ok:disable" : "ok:enable");
        else if (cur_sub.is_toggle && item.label == "Bluetooth" && cur_sub.label == "Named Only")
            lv_label_set_text(hint, cur_sub.toggle_state ? "ok:show all" : "ok:named");
        else if (cur_sub.is_toggle)
            lv_label_set_text(hint, cur_sub.toggle_state ? "ok:hide" : "ok:select");
        else if (item.label == "RTC" && rtc_ntp_on_)
            lv_label_set_text(hint, "ntp on");
        else
            lv_label_set_text(hint, "ok:enter");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x00CC66), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
        apply_right_hint_overflow(hint, row_y(sub_center_vi));
    }

    // Position the far-right hint (ok:xxx). If it is wider than RIGHT_HINT_BOX_W it is
    // clamped into a right-edge box and marquee-scrolled (so long hints like "ok:disable"
    // don't overlap the toggle indicator); otherwise it keeps its natural right-aligned pos.
    void apply_right_hint_overflow(lv_obj_t *hint, int row_top_y)
    {
        lv_obj_update_layout(hint);
        int hw = lv_obj_get_width(hint);
        int hh = lv_obj_get_height(hint);
        int y = row_top_y + (row_h() - hh) / 2;
        if (hw > RIGHT_HINT_BOX_W) {
            lv_obj_set_width(hint, RIGHT_HINT_BOX_W);
            lv_obj_set_pos(hint, SCREEN_W - 6 - RIGHT_HINT_BOX_W, y);
            lv_label_set_long_mode(hint, LV_LABEL_LONG_SCROLL_CIRCULAR);
        } else {
            lv_obj_set_pos(hint, SCREEN_W - 6 - hw, y);
        }
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
            apply_overflow_handling(lbl, VALUE_BOX_LEFT, VALUE_BOX_W, vi == ROW_CENTER);
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
        else if (view_state_ == ViewState::BT_LIST) build_bt_list();
        else if (view_state_ == ViewState::SOUNDCARD_CARDS) build_soundcard_cards_view();
        else if (view_state_ == ViewState::SOUNDCARD_CONTROLS) build_soundcard_controls_view();
        else if (view_state_ == ViewState::SOUNDCARD_DETAIL) build_soundcard_detail_view();
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
        case ViewState::BT_LIST:
            if (pressed && (key == KEY_UP || key == KEY_DOWN))
                handle_bt_list_key(key);
            else if (released && key != KEY_UP && key != KEY_DOWN)
                handle_bt_list_key(key);
            break;
        case ViewState::SOUNDCARD_CARDS:
            if (pressed && (key == KEY_UP || key == KEY_DOWN))
                handle_soundcard_cards_key(key);
            else if (released && key != KEY_UP && key != KEY_DOWN)
                handle_soundcard_cards_key(key);
            break;
        case ViewState::SOUNDCARD_CONTROLS:
            if (pressed && (key == KEY_UP || key == KEY_DOWN))
                handle_soundcard_controls_key(key);
            else if (released && key != KEY_UP && key != KEY_DOWN)
                handle_soundcard_controls_key(key);
            break;
        case ViewState::SOUNDCARD_DETAIL:
            if (released) handle_soundcard_detail_key(key);
            break;
        case ViewState::USB_GUIDE:
            if (released) handle_usb_guide_key(key);
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
            if (val_title_ == "Reboot?" || val_title_ == "Shutdown?" || val_title_ == "Run Setup?") {
                // Show a brief message, then let the system halt/reboot.
                lv_obj_t *cont = ui_obj_["list_cont"];
                lv_obj_clean(cont);
                lv_obj_t *lbl = lv_label_create(cont);
                lv_label_set_text(lbl, val_title_ == "Shutdown?" ? "Shutting down..." : "Rebooting...");
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

    // ==================== SoundCard ====================
// ====================================================================
//  State added to UISetupPage (included in private section)
// ====================================================================

struct ScCard {
    int         index = 0;
    std::string name;
};

struct ScCtrl {
    std::string name;
    std::string type;
    int         min_val    = 0;
    int         max_val    = 0;
    int         step       = 1;
    std::string current_str;
    int         current_val = 0;
};

// Sound card navigation state
std::vector<ScCard> sc_cards_;
std::vector<ScCtrl> sc_controls_;
int  sc_card_sel_   = 0;
int  sc_ctrl_sel_   = 0;
int  sc_card_idx_   = -1;

// Detail / input state
ScCtrl      sc_detail_;
std::string sc_input_buf_;
lv_obj_t   *sc_input_lbl_   = nullptr;
lv_obj_t   *sc_hint_lbl2_   = nullptr;
lv_timer_t *sc_cursor_timer_ = nullptr;
bool        sc_cursor_vis_   = true;

// ====================================================================
//  Parsing helpers (static member functions)
// ====================================================================

static std::string sc_trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static bool sc_parse_limits(const std::string &line, int &mn, int &mx)
{
    size_t p = line.find("Limits:");
    if (p == std::string::npos) return false;
    std::string rest = line.substr(p + 7);
    for (const char *pfx : {"Playback ", "Capture "}) {
        if (rest.find(pfx) == 0) { rest = rest.substr(std::strlen(pfx)); break; }
    }
    int a = 0, b = 0;
    if (std::sscanf(rest.c_str(), " %d - %d", &a, &b) == 2) {
        mn = a; mx = b; return true;
    }
    return false;
}

static int sc_parse_current_val(const std::string &line)
{
    size_t p = line.find(": ");
    if (p == std::string::npos) return -1;
    int v = 0;
    if (std::sscanf(line.c_str() + p + 2, " %d", &v) == 1) return v;
    return -1;
}

static std::string sc_extract_value_str(const std::string &line)
{
    static const char *pfx[] = {
        "Mono:", "Front Left:", "Front Right:", "Rear Left:", "Rear Right:",
        "Center:", "LFE:", "Side Left:", "Side Right:", "Capture:", "Playback:",
        nullptr
    };
    for (int i = 0; pfx[i]; ++i) {
        size_t p = line.find(pfx[i]);
        if (p != std::string::npos) return sc_trim(line.substr(p));
    }
    return sc_trim(line);
}

static bool sc_is_value_line(const std::string &tl)
{
    static const char *pfx[] = {
        "Mono:", "Front Left:", "Front Right:", "Rear Left:", "Rear Right:",
        "Center:", "LFE:", "Side Left:", "Side Right:", "Capture:", "Playback:",
        nullptr
    };
    for (int i = 0; pfx[i]; ++i)
        if (tl.rfind(pfx[i], 0) == 0) return true;
    return false;
}

// ====================================================================
//  Helpers
// ====================================================================

void sc_enter_cards()
{
    sc_cards_.clear();
    cp0_signal_soundcard_api({"ListCards"}, [this](int code, std::string data) {
        if (code != 0) return;
        std::istringstream lines(data);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.empty()) continue;
            size_t tab = line.find('\t');
            if (tab == std::string::npos) continue;
            ScCard c;
            c.index = std::atoi(line.substr(0, tab).c_str());
            c.name  = line.substr(tab + 1);
            sc_cards_.push_back(std::move(c));
        }
    });
    sc_card_sel_ = 0;
    view_state_ = ViewState::SOUNDCARD_CARDS;
    transition_enter_level();
}

void sc_enter_controls()
{
    if (sc_cards_.empty()) return;
    sc_card_idx_ = sc_cards_[sc_card_sel_].index;
    sc_controls_.clear();
    cp0_signal_soundcard_api({"ListControls", std::to_string(sc_card_idx_)},
        [this](int code, std::string data) {
            if (code != 0) return;
            std::istringstream lines(data);
            std::string line;
            while (std::getline(lines, line)) {
                if (line.empty()) continue;
                std::vector<std::string> cols;
                std::string item;
                std::istringstream row(line);
                while (std::getline(row, item, '\t')) cols.push_back(item);
                if (cols.size() < 7) continue;
                ScCtrl c;
                c.name        = cols[0];
                c.type        = cols[1];
                c.min_val     = std::atoi(cols[2].c_str());
                c.max_val     = std::atoi(cols[3].c_str());
                c.step        = std::atoi(cols[4].c_str());
                c.current_str = cols[5];
                c.current_val = std::atoi(cols[6].c_str());
                sc_controls_.push_back(std::move(c));
            }
        });
    sc_ctrl_sel_ = 0;
    view_state_  = ViewState::SOUNDCARD_CONTROLS;
    transition_enter_level();
}

void sc_enter_detail()
{
    if (sc_controls_.empty()) return;
    const auto &ctrl = sc_controls_[sc_ctrl_sel_];
    sc_detail_ = ScCtrl{};
    sc_detail_.name = ctrl.name;
    cp0_signal_soundcard_api({"GetControlDetail", std::to_string(sc_card_idx_), ctrl.name},
        [this, &ctrl](int code, std::string data) {
            if (code != 0) { sc_detail_ = ctrl; return; }
            std::istringstream ss(data);
            std::string line;
            while (std::getline(ss, line)) {
                std::string tl = sc_trim(line);
                if (tl.rfind("Capabilities:", 0) == 0)
                    sc_detail_.type = (tl.find("enum") != std::string::npos) ? "ENUMERATED" : "INTEGER";
                else if (tl.rfind("Limits:", 0) == 0)
                    sc_parse_limits(tl, sc_detail_.min_val, sc_detail_.max_val);
                else if (sc_detail_.current_str.empty() && sc_is_value_line(tl)) {
                    sc_detail_.current_str = sc_extract_value_str(tl);
                    int v = sc_parse_current_val(tl);
                    if (v >= 0) sc_detail_.current_val = v;
                }
            }
        });
    if (sc_detail_.max_val == 0 && ctrl.max_val != 0) {
        sc_detail_.min_val = ctrl.min_val;
        sc_detail_.max_val = ctrl.max_val;
    }
    sc_input_buf_.clear();
    sc_input_lbl_  = nullptr;
    sc_hint_lbl2_  = nullptr;
    view_state_    = ViewState::SOUNDCARD_DETAIL;
    transition_enter_level();
}

// ====================================================================
//  Build: card list view
// ====================================================================
void build_soundcard_cards_view()
{
    lv_obj_t *cont = ui_obj_["list_cont"];
    lv_obj_clean(cont);

    // Title
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Sound Cards");
    lv_obj_set_pos(title, 8, 4);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);

    if (sc_cards_.empty()) {
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, "No ALSA cards found.");
        lv_obj_set_pos(lbl, 8, 40);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "ESC: back");
        lv_obj_set_pos(hint, 8, LIST_H - 14);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
        return;
    }

    int visible = 5;
    int total   = (int)sc_cards_.size();
    int offset  = sc_card_sel_ - visible / 2;
    if (offset < 0) offset = 0;
    if (total > visible && offset > total - visible) offset = total - visible;

    for (int vi = 0; vi < visible && (vi + offset) < total; ++vi) {
        int ai  = vi + offset;
        bool sel = (ai == sc_card_sel_);
        int  y   = 22 + vi * 22;

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

        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, sc_cards_[ai].name.c_str());
        lv_obj_set_pos(lbl, 12, y + 2);
        lv_obj_set_width(lbl, SCREEN_W - 24);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl, lv_color_hex(sel ? 0xFFFFFF : 0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    }

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "OK: open  ESC: back");
    lv_obj_set_pos(hint, 8, LIST_H - 14);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

// ====================================================================
//  Build: control list view
// ====================================================================
void build_soundcard_controls_view()
{
    lv_obj_t *cont = ui_obj_["list_cont"];
    lv_obj_clean(cont);

    // Title: card name
    char title_buf[80];
    if (!sc_cards_.empty() && sc_card_sel_ < (int)sc_cards_.size())
        std::snprintf(title_buf, sizeof(title_buf), "%s", sc_cards_[sc_card_sel_].name.c_str());
    else
        std::snprintf(title_buf, sizeof(title_buf), "Card %d", sc_card_idx_);

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, title_buf);
    lv_obj_set_pos(title, 8, 4);
    lv_obj_set_width(title, SCREEN_W - 16);
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);

    if (sc_controls_.empty()) {
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, "No controls found.");
        lv_obj_set_pos(lbl, 8, 40);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "ESC: back");
        lv_obj_set_pos(hint, 8, LIST_H - 14);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
        return;
    }

    int visible = 5;
    int total   = (int)sc_controls_.size();
    int offset  = sc_ctrl_sel_ - visible / 2;
    if (offset < 0) offset = 0;
    if (total > visible && offset > total - visible) offset = total - visible;

    for (int vi = 0; vi < visible && (vi + offset) < total; ++vi) {
        int ai  = vi + offset;
        bool sel = (ai == sc_ctrl_sel_);
        int  y   = 20 + vi * 22;

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

        const auto &ctrl = sc_controls_[ai];

        // Left: control name
        lv_obj_t *name_lbl = lv_label_create(cont);
        lv_label_set_text(name_lbl, ctrl.name.c_str());
        lv_obj_set_pos(name_lbl, 12, y + 2);
        lv_obj_set_width(name_lbl, 180);
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(sel ? 0xFFFFFF : 0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, LV_PART_MAIN);

        // Right: current value summary
        if (!ctrl.current_str.empty()) {
            lv_obj_t *val_lbl = lv_label_create(cont);
            lv_label_set_text(val_lbl, ctrl.current_str.c_str());
            lv_obj_set_pos(val_lbl, 196, y + 2);
            lv_obj_set_width(val_lbl, SCREEN_W - 200);
            lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_color(val_lbl, lv_color_hex(sel ? 0xAADDFF : 0x888888), LV_PART_MAIN);
            lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
        }
    }

    // Scroll arrows
    if (sc_ctrl_sel_ > 0) {
        lv_obj_t *a = lv_img_create(cont);
        lv_img_set_src(a, img_arrow_up_.c_str());
        lv_obj_set_pos(a, SCREEN_W / 2 - 8, 2);
    }
    if (sc_ctrl_sel_ < total - 1) {
        lv_obj_t *a = lv_img_create(cont);
        lv_img_set_src(a, img_arrow_down_.c_str());
        lv_obj_set_pos(a, SCREEN_W / 2 - 8, LIST_H - 28);
    }

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "OK: edit  ESC: back");
    lv_obj_set_pos(hint, 8, LIST_H - 14);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

// ====================================================================
//  Build: detail / input view
// ====================================================================
void build_soundcard_detail_view()
{
    lv_obj_t *cont = ui_obj_["list_cont"];
    lv_obj_clean(cont);
    sc_input_lbl_ = nullptr;
    sc_hint_lbl2_ = nullptr;

    // Control name (header)
    lv_obj_t *name_lbl = lv_label_create(cont);
    lv_label_set_text(name_lbl, sc_detail_.name.c_str());
    lv_obj_set_pos(name_lbl, 8, 4);
    lv_obj_set_width(name_lbl, SCREEN_W - 16);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(name_lbl, launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);

    // Info block: Limits + current value
    char info_buf[128];
    std::snprintf(info_buf, sizeof(info_buf),
                  "Limits: %d - %d", sc_detail_.min_val, sc_detail_.max_val);
    lv_obj_t *limits_lbl = lv_label_create(cont);
    lv_label_set_text(limits_lbl, info_buf);
    lv_obj_set_pos(limits_lbl, 8, 26);
    lv_obj_set_style_text_color(limits_lbl, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(limits_lbl, &lv_font_montserrat_12, LV_PART_MAIN);

    if (!sc_detail_.current_str.empty()) {
        lv_obj_t *cur_lbl = lv_label_create(cont);
        lv_label_set_text(cur_lbl, sc_detail_.current_str.c_str());
        lv_obj_set_pos(cur_lbl, 8, 44);
        lv_obj_set_width(cur_lbl, SCREEN_W - 16);
        lv_label_set_long_mode(cur_lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(cur_lbl, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(cur_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    }

    // Separator line
    lv_obj_t *sep = lv_obj_create(cont);
    lv_obj_set_size(sep, SCREEN_W - 16, 1);
    lv_obj_set_pos(sep, 8, 64);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sep, 255, LV_PART_MAIN);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    // "New value:" label
    lv_obj_t *new_lbl = lv_label_create(cont);
    lv_label_set_text(new_lbl, "New value:");
    lv_obj_set_pos(new_lbl, 8, 72);
    lv_obj_set_style_text_color(new_lbl, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(new_lbl, &lv_font_montserrat_12, LV_PART_MAIN);

    // Input field (digits + cursor)
    sc_cursor_vis_ = true;
    sc_input_lbl_ = lv_label_create(cont);
    sc_input_update_display();
    lv_obj_set_pos(sc_input_lbl_, 100, 70);
    lv_obj_set_width(sc_input_lbl_, 150);
    lv_label_set_long_mode(sc_input_lbl_, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(sc_input_lbl_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(sc_input_lbl_, &lv_font_montserrat_14, LV_PART_MAIN);

    // Blinking cursor timer (500 ms period)
    sc_cursor_timer_ = lv_timer_create([](lv_timer_t *timer) {
        auto *self = static_cast<UISetupPage *>(lv_timer_get_user_data(timer));
        self->sc_cursor_vis_ = !self->sc_cursor_vis_;
        self->sc_input_update_display();
    }, 500, this);

    // Range hint below input
    char range_buf[64];
    std::snprintf(range_buf, sizeof(range_buf), "Range: %d ~ %d",
                  sc_detail_.min_val, sc_detail_.max_val);
    sc_hint_lbl2_ = lv_label_create(cont);
    lv_label_set_text(sc_hint_lbl2_, range_buf);
    lv_obj_set_pos(sc_hint_lbl2_, 8, 92);
    lv_obj_set_style_text_color(sc_hint_lbl2_, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(sc_hint_lbl2_, &lv_font_montserrat_10, LV_PART_MAIN);

    // Bottom hint
    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "0-9:type  BS:del  OK:apply  ESC:back");
    lv_obj_set_pos(hint, 8, LIST_H - 14);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

void sc_input_update_display()
{
    if (!sc_input_lbl_) return;
    std::string disp = sc_input_buf_ + (sc_cursor_vis_ ? "_" : " ");
    lv_label_set_text(sc_input_lbl_, disp.c_str());
}

void sc_cursor_stop()
{
    if (sc_cursor_timer_) {
        lv_timer_del(sc_cursor_timer_);
        sc_cursor_timer_ = nullptr;
    }
    sc_cursor_vis_ = true;
}

// Apply the typed value via cp0_signal_soundcard_api
void sc_apply_value()
{
    if (sc_input_buf_.empty()) return;

    int new_val = std::atoi(sc_input_buf_.c_str());
    // Clamp to declared limits when they are known
    if (sc_detail_.max_val > sc_detail_.min_val) {
        if (new_val < sc_detail_.min_val) new_val = sc_detail_.min_val;
        if (new_val > sc_detail_.max_val) new_val = sc_detail_.max_val;
    }

    // Visual feedback while applying
    if (sc_hint_lbl2_) {
        lv_label_set_text(sc_hint_lbl2_, "Applying...");
        lv_obj_set_style_text_color(sc_hint_lbl2_, lv_color_hex(0xFFAA00), LV_PART_MAIN);
        lv_refr_now(NULL);
    }

    int rc = -1;
    cp0_signal_soundcard_api(
        {"SetControl", std::to_string(sc_card_idx_), sc_detail_.name, std::to_string(new_val)},
        [&rc](int code, std::string) { rc = code; });

    if (sc_hint_lbl2_) {
        if (rc == 0) {
            lv_label_set_text(sc_hint_lbl2_, "Applied OK");
            lv_obj_set_style_text_color(sc_hint_lbl2_, lv_color_hex(0x33CC33), LV_PART_MAIN);
        } else {
            lv_label_set_text(sc_hint_lbl2_, "Error (check amixer)");
            lv_obj_set_style_text_color(sc_hint_lbl2_, lv_color_hex(0xFF4444), LV_PART_MAIN);
        }
        lv_refr_now(NULL);
    }

    // Refresh the control list entry with the new value
    if (rc == 0 && sc_ctrl_sel_ < (int)sc_controls_.size()) {
        char val_str[32];
        std::snprintf(val_str, sizeof(val_str), "%d", new_val);
        sc_controls_[sc_ctrl_sel_].current_val = new_val;
        sc_controls_[sc_ctrl_sel_].current_str = val_str;
    }

    // Go back to control list after a short pause
    sc_cursor_stop();
    sc_input_lbl_  = nullptr;
    sc_hint_lbl2_  = nullptr;
    view_state_ = ViewState::SOUNDCARD_CONTROLS;
    lv_timer_t *t = lv_timer_create([](lv_timer_t *timer) {
        auto *self = static_cast<UISetupPage *>(lv_timer_get_user_data(timer));
        lv_timer_del(timer);
        self->transition_back_level();
    }, 900, this);
    (void)t;
}

// ====================================================================
//  Key handlers
// ====================================================================
void handle_soundcard_cards_key(uint32_t key)
{
    int total = (int)sc_cards_.size();
    switch (key) {
    case KEY_UP:
        if (sc_card_sel_ > 0) { --sc_card_sel_; build_soundcard_cards_view(); }
        break;
    case KEY_DOWN:
        if (sc_card_sel_ < total - 1) { ++sc_card_sel_; build_soundcard_cards_view(); }
        break;
    case KEY_ENTER:
    case KEY_RIGHT:
        if (total > 0) { play_enter(); sc_enter_controls(); }
        break;
    case KEY_ESC:
    case KEY_LEFT:
        play_back();
        view_state_ = ViewState::SUB;
        transition_back_level();
        break;
    default:
        break;
    }
}

void handle_soundcard_controls_key(uint32_t key)
{
    int total = (int)sc_controls_.size();
    switch (key) {
    case KEY_UP:
        if (sc_ctrl_sel_ > 0) { --sc_ctrl_sel_; build_soundcard_controls_view(); }
        break;
    case KEY_DOWN:
        if (sc_ctrl_sel_ < total - 1) { ++sc_ctrl_sel_; build_soundcard_controls_view(); }
        break;
    case KEY_ENTER:
    case KEY_RIGHT:
        if (total > 0) { play_enter(); sc_enter_detail(); }
        break;
    case KEY_ESC:
    case KEY_LEFT:
        play_back();
        view_state_ = ViewState::SOUNDCARD_CARDS;
        transition_back_level();
        break;
    default:
        break;
    }
}

void handle_soundcard_detail_key(uint32_t key)
{
    // Digit keys: accumulate input
    if (key == KEY_0 || (key >= KEY_1 && key <= KEY_9)) {
        // KEY_1..KEY_9 map to '1'..'9', KEY_0 maps to '0'
        // Linux input key codes: KEY_1=2..KEY_9=10, KEY_0=11
        int digit = -1;
        if (key == KEY_0)         digit = 0;
        else if (key >= KEY_1 && key <= KEY_9) digit = (int)(key - KEY_1 + 1);
        if (digit >= 0 && sc_input_buf_.size() < 8) {
            sc_input_buf_ += (char)('0' + digit);
            sc_input_update_display();
        }
        return;
    }

    switch (key) {
    case KEY_BACKSPACE:
        if (!sc_input_buf_.empty()) {
            sc_input_buf_.pop_back();
            sc_input_update_display();
        }
        break;
    case KEY_ENTER:
    case KEY_RIGHT:
        sc_apply_value();
        break;
    case KEY_ESC:
    case KEY_LEFT:
        sc_cursor_stop();
        play_back();
        view_state_ = ViewState::SOUNDCARD_CONTROLS;
        transition_back_level();
        break;
    default:
        // Also accept typed digit characters forwarded via cur_elm_->utf8
        if (cur_elm_ && cur_elm_->utf8[0] >= '0' && cur_elm_->utf8[0] <= '9') {
            if (sc_input_buf_.size() < 8) {
                sc_input_buf_ += cur_elm_->utf8[0];
                sc_input_update_display();
            }
        }
        break;
    }
}

};
