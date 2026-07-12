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
#include <memory>
#include <utility>
#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "../app_registry.h"


class UISetupPage;

namespace setting {

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

class Launcher {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    static void save_app_toggle(UISetupPage &page, const std::string &config_key);
};

class Boot {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    static void factory_reset();
    static void rearm_oobe_and_reboot();
};

class Screen {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    void enter_brightness_adjust(UISetupPage &page);
    void enter_darktime_adjust(UISetupPage &page);
    void apply_value(UISetupPage &page);
    static int backlight_read();
    static int backlight_max();
private:
    int bright_val_ = 75;
};

class WiFi {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    void enter_scan(UISetupPage &page);
    void build_list(UISetupPage &page);
    void handle_list_key(UISetupPage &page, uint32_t key);
    void refresh_radio(UISetupPage &page);
    void toggle_enable(UISetupPage &page);
    void try_connect(UISetupPage &page, int idx);
    void show_connecting(UISetupPage &page, const char *ssid);
    void show_error(UISetupPage &page, const char *msg);
    void forget_selected(UISetupPage &page);
    bool has_saved_profile(const char *ssid);
    void show_pw_input(UISetupPage &page);
    void handle_pw_key(UISetupPage &page, uint32_t key);
    void pw_update_display();
    void do_scan();
private:
    cp0_wifi_ap_t aps_[CP0_WIFI_AP_MAX];
    int ap_count_ = 0;
    int list_sel_ = 0;
    std::string pw_ssid_;
    std::string pw_buf_;
    lv_obj_t *pw_input_lbl_ = nullptr;
    lv_obj_t *pw_hint_lbl_ = nullptr;
};

class Speaker {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    void enter_volume_adjust(UISetupPage &page);
    void apply_value(UISetupPage &page);
private:
    int vol_val_ = 39;
};

class Camera {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    void enter_resolution(UISetupPage &page);
    void apply_value(UISetupPage &page);
};

class Info {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    void refresh_values(UISetupPage &page);
    void start_timer(UISetupPage &page);
    void stop_timer();
    void enter_bq_calibrate(UISetupPage &page);
    void apply_bq_calibrate(UISetupPage &page);
    void reset_visible_labels();
    void track_visible_label(int index, lv_obj_t *label, bool focused, const std::string &text);
    void refresh_visible_labels(UISetupPage &page);
private:
    lv_timer_t *timer_ = nullptr;
    lv_obj_t *sub_labels_[4] = {};
    bool sub_label_focused_[4] = {};
    std::string visible_text_[4];
};

class Developer {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    void toggle_adb(UISetupPage &page);
    void refresh_adb_status(UISetupPage &page);
    void enter_usb_guide(UISetupPage &page, bool enabling);
    void build_usb_guide_view(UISetupPage &page);
    void stop_usb_guide_anims();
    void handle_usb_guide_key(UISetupPage &page, uint32_t key);
private:
    static constexpr const char *kAdbHelper = "/usr/share/APPLaunch/adb/cardputer-adb";
    static lv_obj_t *guide_chip(lv_obj_t *parent, int x, int y, int w, int h,
                                uint32_t bg, uint32_t border, int radius, int border_w);
    static lv_obj_t *guide_label(lv_obj_t *parent, int x, int y, const char *txt,
                                 uint32_t color, const lv_font_t *font);
    bool usb_guide_enabling_ = true;
    lv_obj_t *usb_guide_knob_ = nullptr;
};

class RTC {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    void refresh_values(UISetupPage &page);
    void toggle_ntp(UISetupPage &page);
    void enter_adjust(UISetupPage &page, int field);
    void apply_value(UISetupPage &page);
    void commit_to_hardware(UISetupPage &page);
    void show_write_confirm(UISetupPage &page);
    void close_write_confirm();
    void handle_write_confirm_key(UISetupPage &page, uint32_t key);
    bool is_dirty() const { return dirty_; }
    bool ntp_on() const { return ntp_on_; }
    bool write_confirm_active() const { return confirm_overlay_ != nullptr; }
    void clear_dirty() { dirty_ = false; }
private:
    void update_labels(UISetupPage &page);
    void update_write_confirm_buttons();
    int values_[6] = {2026, 1, 1, 0, 0, 0};
    int field_ = 0;
    bool ntp_on_ = true;
    bool dirty_ = false;
    int confirm_sel_ = 1;
    lv_obj_t *confirm_overlay_ = nullptr;
    lv_obj_t *confirm_yes_lbl_ = nullptr;
    lv_obj_t *confirm_no_lbl_ = nullptr;
};

class About {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    static void refresh_info(UISetupPage &page);
};

class Help {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    static void enter_page(UISetupPage &page);
};

class ExtPort {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
};

class Ethernet {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    static void refresh_info(UISetupPage &page);
};

class Account {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    static void refresh_info(UISetupPage &page);
};

class Update {
public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    static void refresh_version_info(UISetupPage &page);
    static void check_system_update();
    static void update_launcher();
};

class Bluetooth {
    struct ListRow {
        int device_index;
        const char *title;
        bool is_header;
    };
    enum class ListMode { Managed, Scan };

public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    void enter_devices(UISetupPage &page);
    void enter_alias(UISetupPage &page);
    void build_alias_view(UISetupPage &page);
    void handle_alias_key(UISetupPage &page, uint32_t key);
    void enter_scan(UISetupPage &page);
    void build_list(UISetupPage &page);
    void handle_list_key(UISetupPage &page, uint32_t key);
    void refresh_status(UISetupPage &page);
    void toggle_power(UISetupPage &page);
    void toggle_named_only(UISetupPage &page);
    void toggle_discoverable(UISetupPage &page);
    void start_scan_timer(UISetupPage &page);
    void stop_scan_timer();
    void refresh_devices();
    void do_scan(UISetupPage &page);

private:
    static bool alias_char_allowed(unsigned char ch);
    std::string alias_sanitized() const;
    void alias_update_display();
    void rebuild_rows();
    bool should_hide_device(const cp0_bt_device_t &dev) const;
    static std::string normalized_mac_text(const char *text);
    int selected_device_index() const;
    void select_next_device(int direction);
    void show_action(UISetupPage &page, const char *msg, uint32_t color = 0x58A6FF);
    void activate_selected(UISetupPage &page);
    void remove_selected(UISetupPage &page);
    static void copy_string(char *dst, size_t dst_size, const std::string &src);
    static std::vector<std::string> split_char(const std::string &line, char delimiter);
    static bool decode_status(const std::string &data, cp0_bt_status_t &st);
    static int decode_devices(const std::string &data, cp0_bt_device_t *out, int max_devices);
    static int api_int(std::list<std::string> args, int default_value = -1);
    static cp0_bt_status_t get_status();
    static int set_power(int on);
    static int set_alias(const std::string &alias);
    static int set_discoverable(int on);
    static int device_command(const char *cmd, const char *address);
    static int device_list(const char *cmd, cp0_bt_device_t *out, int max_devices);

    cp0_bt_device_t devices_[CP0_BT_DEVICE_MAX];
    int device_count_ = 0;
    int list_sel_ = 0;
    std::vector<ListRow> rows_;
    ListMode list_mode_ = ListMode::Managed;
    lv_timer_t *scan_timer_ = nullptr;
    bool discovery_active_ = false;
    bool named_only_ = true;
    bool action_busy_ = false;
    std::string alias_ = "CardputerZero";
    bool discoverable_ = false;
    std::string alias_input_;
    lv_obj_t *alias_input_lbl_ = nullptr;
    lv_obj_t *alias_hint_lbl_ = nullptr;
};

class SoundCard {
    struct Card {
        int index = 0;
        std::string name;
    };

    struct Control {
        std::string name;
        std::string type;
        int min_val = 0;
        int max_val = 0;
        int step = 1;
        std::string current_str;
        int current_val = 0;
    };

public:
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    void enter_cards(UISetupPage &page);
    void enter_controls(UISetupPage &page);
    void enter_detail(UISetupPage &page);
    void build_cards_view(UISetupPage &page);
    void build_controls_view(UISetupPage &page);
    void build_detail_view(UISetupPage &page);
    void handle_cards_key(UISetupPage &page, uint32_t key);
    void handle_controls_key(UISetupPage &page, uint32_t key);
    void handle_detail_key(UISetupPage &page, uint32_t key);

private:
    static std::string trim(const std::string &s);
    static bool parse_limits(const std::string &line, int &mn, int &mx);
    static int parse_current_val(const std::string &line);
    static std::string extract_value_str(const std::string &line);
    static bool is_value_line(const std::string &tl);
    void input_update_display();
    void cursor_stop();
    void apply_value(UISetupPage &page);

    std::vector<Card> cards_;
    std::vector<Control> controls_;
    int card_sel_ = 0;
    int ctrl_sel_ = 0;
    int card_idx_ = -1;
    Control detail_;
    std::string input_buf_;
    lv_obj_t *input_lbl_ = nullptr;
    lv_obj_t *hint_lbl_ = nullptr;
    lv_timer_t *cursor_timer_ = nullptr;
    bool cursor_vis_ = true;
};

void build_menu(UISetupPage &page);

} // namespace setting

// ============================================================
//  System settings screen  UISetupPage  (Carousel Design)
//  Screen: 320x170 (top bar20px, body 320x150)
//
//  Menu items (design mockup): Launcher, Boot, Screen, WiFi, Speaker, Camera
//  Actual HAL integration: WiFi scan/connect, brightness, volume, power, reboot, about
// ============================================================

class UISetupPage : public AppPage
{
    enum class ViewState { MAIN, SUB, VALUE_SELECT, WIFI_LIST, WIFI_PW, BT_LIST, BT_ALIAS,
                           SOUNDCARD_CARDS, SOUNDCARD_CONTROLS, SOUNDCARD_DETAIL,
                           USB_GUIDE };

    using SubItem = setting::SubItem;
    using MenuItem = setting::MenuItem;

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
        info_.stop_timer();
        bluetooth_.stop_scan_timer();
        rtc_.close_write_confirm();
    }

private:
    std::vector<MenuItem> menu_items_;
    friend class setting::Launcher;
    friend class setting::Boot;
    friend class setting::Screen;
    friend class setting::WiFi;
    friend class setting::Speaker;
    friend class setting::Camera;
    friend class setting::Info;
    friend class setting::Developer;
    friend class setting::RTC;
    friend class setting::About;
    friend class setting::Help;
    friend class setting::ExtPort;
    friend class setting::Ethernet;
    friend class setting::Account;
    friend class setting::Update;
    friend class setting::Bluetooth;
    friend class setting::SoundCard;
    friend void setting::build_menu(UISetupPage &page);

    setting::Screen screen_;
    setting::WiFi wifi_;
    setting::Speaker speaker_;
    setting::Camera camera_;
    setting::Info info_;
    setting::Developer developer_;
    setting::RTC rtc_;
    setting::Bluetooth bluetooth_;
    setting::SoundCard soundcard_;

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

    struct key_item *cur_elm_ = nullptr;

    // Value select (3rd level)
    int val_sel_idx_ = 0;
    std::vector<std::string> val_options_;
    std::string val_title_;

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

    static void gpio_set(const char *name, int val)
    {
        cp0_signal_settings_api({"GpioSet", name ? std::string(name) : std::string(), std::to_string(val)}, nullptr);
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
        setting::build_menu(*this);
    }

    int find_menu(const char *label)
    {
        for (size_t i = 0; i < menu_items_.size(); ++i)
            if (menu_items_[i].label == label) return (int)i;
        return -1;
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

    void apply_value_selection()
    {
        if (val_title_ == "Brightness") {
            screen_.apply_value(*this);
        } else if (val_title_ == "DarkTime") {
            screen_.apply_value(*this);
        } else if (val_title_ == "Volume") {
            speaker_.apply_value(*this);
        } else if (val_title_ == "Resolution") {
            camera_.apply_value(*this);
        } else if (val_title_ == "BQ Calib") {
            info_.apply_bq_calibrate(*this);
        } else if (val_title_ == "Reboot?" || val_title_ == "Shutdown?" || val_title_ == "Run Setup?") {
            if (val_sel_idx_ == 0 && confirm_action_) confirm_action_();
            confirm_action_ = nullptr;
        } else if (val_title_ == "Year" || val_title_ == "Month" || val_title_ == "Day" ||
                   val_title_ == "Hour" || val_title_ == "Minute" || val_title_ == "Second") {
            rtc_.apply_value(*this);
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
    static constexpr int VALUE_BOX_LEFT = 124;
    static constexpr int VALUE_BOX_W    = 88; // 超过 88px 即缩宽并滚动
    // Right-column hint (ok:xxx) scroll threshold. The hint sits at the far right,
    // to the right of any toggle indicator (x=220). Anything wider than this is
    // clamped into a right-edge box and marquee-scrolled. Slightly smaller than the
    // center VALUE_BOX_W so it clears the toggle indicator instead of overlapping it.
    static constexpr int RIGHT_HINT_BOX_W = 74;

    // Width of the "Connected WiFi: <ssid> <ip>" header box in the WiFi list. When the
    // text is wider than this it marquee-scrolls instead of overflowing off-screen (#66).
    static constexpr int WIFI_TITLE_BOX_W = 300;

    // SoundCard uses fixed slots so long ALSA names/values cannot push neighboring
    // text or hints out of place.
    static constexpr int SC_MARGIN_X      = 8;
    static constexpr int SC_ROW_X         = 12;
    static constexpr int SC_CARD_NAME_W   = SCREEN_W - 24;
    static constexpr int SC_CTRL_NAME_X   = 12;
    static constexpr int SC_CTRL_NAME_W   = 172;
    static constexpr int SC_CTRL_VALUE_X  = 190;
    static constexpr int SC_CTRL_VALUE_W  = SCREEN_W - SC_CTRL_VALUE_X - 8;
    static constexpr int SC_DETAIL_TEXT_W = SCREEN_W - 16;
    static constexpr int SC_INPUT_X       = 100;
    static constexpr int SC_INPUT_W       = SCREEN_W - SC_INPUT_X - 12;
    static constexpr int SC_BOTTOM_HINT_W = SCREEN_W - 16;

    static constexpr int SUB_LEFT_BOX_X   = 4;
    static constexpr int SUB_LEFT_BOX_W   = 90;
    static constexpr int SUB_ARROW_X      = 100;
    static constexpr int SUB_CENTER_X     = 160;

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

    static constexpr int SUB_RIGHT_ARROW_X = SUB_ARROW_X;
    static constexpr int ARROW_SRC = 19;    // setting_right_arrow.png is 19x19
    static constexpr int ARROW_SCALE = 224; // 256 = 100%; shrink the blue arrow a touch

    // Place the blue "drill-in" arrow between the left column and the right
    // column. It sits just left of the right text with a guaranteed gap, is
    // scaled down slightly, and is drawn *behind* the row text (but above the
    // highlight bar) so a wide left label can never be occluded by it. (plan A)
    void place_blue_arrow(lv_obj_t *parent, lv_obj_t *left_lbl, int right_min_x)
    {
        if (!left_lbl || right_min_x <= 0) return;
        lv_obj_update_layout(left_lbl);

        const int vis = ARROW_SRC * ARROW_SCALE / 256; // scaled width/height (square)
        int left_right_edge = lv_obj_get_x(left_lbl) + lv_obj_get_width(left_lbl);

        static constexpr int SAFE_GAP = 4;
        int arrow_x = right_min_x - SAFE_GAP - vis;
        if (arrow_x < left_right_edge + 1) arrow_x = left_right_edge + 1;

        // Vertically center the (scaled) arrow within the focused row.
        int arrow_y = row_y(ROW_CENTER) + (row_h() - vis) / 2;

        lv_obj_t *arrow = lv_img_create(parent);
        lv_img_set_src(arrow, img_right_arrow_.c_str());
        lv_image_set_pivot(arrow, 0, 0);
        lv_image_set_scale(arrow, ARROW_SCALE);
        lv_obj_set_pos(arrow, arrow_x, arrow_y);
        // Behind the row text, above the highlight bar (child index 0).
        lv_obj_move_to_index(arrow, 1);
    }

    void place_fixed_sub_arrow(lv_obj_t *parent)
    {
        static constexpr int ARROW_H = 19;
        lv_obj_t *arrow = lv_img_create(parent);
        lv_img_set_src(arrow, img_right_arrow_.c_str());
        lv_obj_set_pos(arrow, SUB_RIGHT_ARROW_X, row_y(ROW_CENTER) + (row_h() - ARROW_H) / 2);
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

    static void apply_fixed_label_box(lv_obj_t *lbl, int x, int y, int w, bool scroll)
    {
        if (!lbl || w <= 0)
            return;
        lv_obj_set_pos(lbl, x, y);
        lv_obj_set_width(lbl, w);
        lv_label_set_long_mode(lbl, scroll ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_CLIP);
    }

    static void clamp_label_box(lv_obj_t *lbl, int x, int w, bool scroll)
    {
        if (!lbl || w <= 0)
            return;
        lv_obj_set_x(lbl, x);
        lv_obj_set_width(lbl, w);
        lv_label_set_long_mode(lbl, scroll ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_CLIP);
    }

    // Same as apply_overflow_handling but keeps the clamped box centered on
    // center_x, so an overflowing (marquee) value stays visually centered
    // instead of drifting to a fixed left edge.
    static void apply_overflow_centered(lv_obj_t *lbl, int center_x, int box_w, bool focused)
    {
        apply_overflow_handling(lbl, center_x - box_w / 2, box_w, focused);
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
        info_.reset_visible_labels();
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
            clamp_label_box(lbl, SUB_LEFT_BOX_X, SUB_LEFT_BOX_W, vi == ROW_CENTER);
            if (vi == ROW_CENTER) left_center_lbl = lbl;
        }

        // Right column: sub items (same carousel style, centered at x=160)

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

        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            int si = sub_selected_idx_ - sub_center_vi + vi;
            if (si < 0 || si >= sub_count) continue;

            SubItem &sub = item.sub_items[si];
            lv_obj_t *lbl = create_carousel_label(cont, vi, sub_center_vi,
                                                   sub.label.c_str(), SUB_CENTER_X, true);
            bool focused_row = (vi == sub_center_vi);
            apply_overflow_centered(lbl, SUB_CENTER_X, focused_row ? 80 : VALUE_BOX_W, focused_row);
            if (item.label == "Info" && si >= 0 && si < 4) {
                info_.track_visible_label(si, lbl, focused_row, sub.label);
            }
            lv_obj_update_layout(lbl);
            int lx = lv_obj_get_x(lbl);
            int tw = lv_obj_get_width(lbl);
            if (lx < right_min_x) right_min_x = lx;

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

        // Keep the sub-view pointer in a fixed slot. Dynamic placement based on text
        // widths breaks when long menu names like "SoundCard" overlap the value column.
        if (sub_count > 0)
            place_fixed_sub_arrow(cont);

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
            lv_label_set_text(hint, cur_sub.toggle_state ? "ok:hide" : "ok:show");
        else if (item.label == "RTC" && rtc_.ntp_on())
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

        // Left column: sub-item name (e.g. "Resolution") as carousel at MENU_X.
        // Long names are clamped + marquee-scrolled so they can't run into the
        // arrow / value column (kept centered on MENU_X).
        static constexpr int VAL_LEFT_BOX_W = 84;
        lv_obj_t *val_left_lbl = nullptr;
        for (int vi = 0; vi < ROWS_VISIBLE; ++vi) {
            int si = sub_selected_idx_ - ROW_CENTER + vi;
            if (si < 0 || si >= count) continue;
            const char *text = menu_items_[selected_idx_].sub_items[si].label.c_str();
            lv_obj_t *lbl = create_carousel_label(cont, vi, ROW_CENTER, text, MENU_X);
            apply_overflow_centered(lbl, MENU_X, VAL_LEFT_BOX_W, vi == ROW_CENTER);
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
            apply_overflow_centered(lbl, VAL_CENTER_X, VALUE_BOX_W, vi == ROW_CENTER);
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
        else if (view_state_ == ViewState::WIFI_LIST) wifi_.build_list(*this);
        else if (view_state_ == ViewState::BT_LIST) bluetooth_.build_list(*this);
        else if (view_state_ == ViewState::BT_ALIAS) bluetooth_.build_alias_view(*this);
        else if (view_state_ == ViewState::SOUNDCARD_CARDS) soundcard_.build_cards_view(*this);
        else if (view_state_ == ViewState::SOUNDCARD_CONTROLS) soundcard_.build_controls_view(*this);
        else if (view_state_ == ViewState::SOUNDCARD_DETAIL) soundcard_.build_detail_view(*this);
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

        if (rtc_.write_confirm_active()) {
            if (released)
                rtc_.handle_write_confirm_key(*this, key);
            return;
        }

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
                wifi_.handle_list_key(*this, key);
            else if (released && key != KEY_UP && key != KEY_DOWN)
                wifi_.handle_list_key(*this, key);
            break;
        case ViewState::WIFI_PW:
            if (released) wifi_.handle_pw_key(*this, key);
            break;
        case ViewState::BT_LIST:
            if (pressed && (key == KEY_UP || key == KEY_DOWN))
                bluetooth_.handle_list_key(*this, key);
            else if (released && key != KEY_UP && key != KEY_DOWN)
                bluetooth_.handle_list_key(*this, key);
            break;
        case ViewState::BT_ALIAS:
            if (released) bluetooth_.handle_alias_key(*this, key);
            break;
        case ViewState::SOUNDCARD_CARDS:
            if (pressed && (key == KEY_UP || key == KEY_DOWN))
                soundcard_.handle_cards_key(*this, key);
            else if (released && key != KEY_UP && key != KEY_DOWN)
                soundcard_.handle_cards_key(*this, key);
            break;
        case ViewState::SOUNDCARD_CONTROLS:
            if (pressed && (key == KEY_UP || key == KEY_DOWN))
                soundcard_.handle_controls_key(*this, key);
            else if (released && key != KEY_UP && key != KEY_DOWN)
                soundcard_.handle_controls_key(*this, key);
            break;
        case ViewState::SOUNDCARD_DETAIL:
            if (released) soundcard_.handle_detail_key(*this, key);
            break;
        case ViewState::USB_GUIDE:
            if (released) developer_.handle_usb_guide_key(*this, key);
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
            info_.stop_timer();
            if (item.label == "RTC" && rtc_.is_dirty()) {
                rtc_.show_write_confirm(*this);
                break;
            }
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
        case KEY_RIGHT: {
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
        }
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


namespace setting {

void Launcher::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
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
            [page, key = std::string(desc.config_key)]() { Launcher::save_app_toggle(*page, key); }});
    }
    menu.push_back(m);
}

void Launcher::save_app_toggle(UISetupPage &page, const std::string &config_key)
{
    int launcher_idx = page.find_menu("Launcher");
    if (launcher_idx < 0)
        return;
    MenuItem &launcher_menu = page.menu_items_[launcher_idx];

    std::size_t app_count = 0;
    const AppDescriptor *apps = launcher_app_registry_entries(&app_count);
    int visible_idx = 0;
    for (std::size_t i = 0; i < app_count; ++i) {
        const AppDescriptor &desc = apps[i];
        if (!desc.configurable)
            continue;
        if (config_key == desc.config_key) {
            if (visible_idx >= (int)launcher_menu.sub_items.size())
                return;
            bool enabled = launcher_menu.sub_items[visible_idx].toggle_state;
            launcher_app_registry_set_enabled(desc, enabled);
            UISetupPage::config_save();
            launcher_app_registry_notify_changed();
            return;
        }
        ++visible_idx;
    }
}

void Boot::factory_reset()
{
    remove("/var/lib/applaunch/settings");
    cp0_system_reboot();
}

void Boot::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Boot";
    m.sub_items = {
        {"Reboot", false, false, [page]() {
            page->enter_confirm_action("Reboot?", [page](){ cp0_system_reboot(); });
        }},
        {"Shutdown", false, false, [page]() {
            page->enter_confirm_action("Shutdown?", [page](){ cp0_system_shutdown(); });
        }},
    };
    menu.push_back(m);
}

void Boot::rearm_oobe_and_reboot()
{
#ifndef _WIN32
    mkdir("/var/lib/applaunch", 0755);
#endif
    FILE *f = fopen("/var/lib/applaunch/run-oobe", "w");
    if (f) fclose(f);
    cp0_system_reboot();
}

void Screen::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Screen";
    m.sub_items = {
        {"Brightness", false, false, [page]() { page->screen_.enter_brightness_adjust(*page); }},
        {"DarkTime", false, false, [page]() { page->screen_.enter_darktime_adjust(*page); }},
    };
    menu.push_back(m);
}

int Screen::backlight_read()
{
    int value = -1;
    cp0_signal_settings_api({"BacklightRead"}, [&](int code, std::string data) {
        if (code == 0) value = std::atoi(data.c_str());
    });
    return value;
}

int Screen::backlight_max()
{
    int value = 100;
    cp0_signal_settings_api({"BacklightMax"}, [&](int code, std::string data) {
        if (code == 0) value = std::atoi(data.c_str());
    });
    return value;
}

void Screen::enter_brightness_adjust(UISetupPage &page)
{
    page.val_title_ = "Brightness";
    page.val_options_ = {"100%", "75%", "50%", "25%"};
    bright_val_ = backlight_read();
    int mx = backlight_max();
    int pct = mx > 0 ? bright_val_ * 100 / mx : 100;
    if (pct >= 87) page.val_sel_idx_ = 0;
    else if (pct >= 62) page.val_sel_idx_ = 1;
    else if (pct >= 37) page.val_sel_idx_ = 2;
    else page.val_sel_idx_ = 3;
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void Screen::apply_value(UISetupPage &page)
{
    if (page.val_title_ == "DarkTime") {
        static const int times[] = {0, 10, 30, 60, 300};
        UISetupPage::config_set_int("dark_time", times[page.val_sel_idx_]);
        UISetupPage::config_save();
        return;
    }

    int mx = backlight_max();
    int pcts[] = {100, 75, 50, 25};
    int new_val = mx * pcts[page.val_sel_idx_] / 100;
    if (new_val < 1) new_val = 1;
    cp0_backlight_write(new_val);
    UISetupPage::config_set_int("brightness", new_val);
    UISetupPage::config_save();
}

void Screen::enter_darktime_adjust(UISetupPage &page)
{
    static const int times[] = {0, 10, 30, 60, 300};
    page.val_title_ = "DarkTime";
    page.val_options_ = {"Never", "10S", "30S", "60S", "300S"};
    const int saved = UISetupPage::config_get_int("dark_time", 30);
    page.val_sel_idx_ = 2;
    for (size_t i = 0; i < sizeof(times) / sizeof(times[0]); ++i) {
        if (times[i] == saved) {
            page.val_sel_idx_ = static_cast<int>(i);
            break;
        }
    }
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void Speaker::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Speaker";
    m.sub_items = {{"Volume", false, false, [page]() { page->speaker_.enter_volume_adjust(*page); }}};
    menu.push_back(m);
}

void Speaker::enter_volume_adjust(UISetupPage &page)
{
    page.val_title_ = "Volume";
    page.val_options_ = {"100%", "75%", "50%", "25%", "0%"};
    vol_val_ = UISetupPage::config_get_int("volume", UISetupPage::audio_volume_read());
    int pct = vol_val_;
    if (pct >= 87) page.val_sel_idx_ = 0;
    else if (pct >= 62) page.val_sel_idx_ = 1;
    else if (pct >= 37) page.val_sel_idx_ = 2;
    else if (pct >= 12) page.val_sel_idx_ = 3;
    else page.val_sel_idx_ = 4;
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void Speaker::apply_value(UISetupPage &page)
{
    int pcts[] = {100, 75, 50, 25, 0};
    int new_val = pcts[page.val_sel_idx_];
    UISetupPage::audio_volume_write(new_val);
    UISetupPage::config_set_int("volume", new_val);
    UISetupPage::config_save();
}

void Camera::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Camera";
    m.sub_items = {{"Resolution", false, false, [page]() { page->camera_.enter_resolution(*page); }}};
    menu.push_back(m);
}

void Camera::enter_resolution(UISetupPage &page)
{
    page.val_title_ = "Resolution";
    page.val_options_ = {"1280x720", "640x480"};
    page.val_sel_idx_ = (UISetupPage::config_get_int("camera.resolution.width", 1280) == 640) ? 1 : 0;
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void Camera::apply_value(UISetupPage &page)
{
    int width = 1280, height = 720;
    if (page.val_sel_idx_ == 1) { width = 640; height = 480; }
    UISetupPage::config_set_int("camera.resolution.width", width);
    UISetupPage::config_set_int("camera.resolution.height", height);
    UISetupPage::config_save();
}

void WiFi::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "WiFi";
    m.sub_items = {
        {"Power", true, false, [page]() { page->wifi_.toggle_enable(*page); }},
        {"Scan", false, false, [page]() { page->wifi_.enter_scan(*page); }},
    };
    m.on_enter = [page]() { page->wifi_.refresh_radio(*page); };
    menu.push_back(m);
}

void WiFi::do_scan()
{
    ap_count_ = launcher_wifi::scan(aps_, CP0_WIFI_AP_MAX);
}

void WiFi::enter_scan(UISetupPage &page)
{
    do_scan();
    list_sel_ = 0;
    page.view_state_ = UISetupPage::ViewState::WIFI_LIST;
    build_list(page);
}

void WiFi::refresh_radio(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "WiFi") continue;
        m.sub_items[0].toggle_state = launcher_wifi::radio_enabled() != 0;
        break;
    }
}

void WiFi::toggle_enable(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "WiFi") continue;
        bool on = m.sub_items[0].toggle_state;
        launcher_wifi::radio_set_enabled(on);
        m.sub_items[0].toggle_state = launcher_wifi::radio_enabled() != 0;
        break;
    }
}

void WiFi::build_list(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);

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
    UISetupPage::apply_overflow_handling(title, 8, UISetupPage::WIFI_TITLE_BOX_W, true);

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

    if (ap_count_ == 0) {
        lv_obj_t *empty = lv_label_create(cont);
        lv_label_set_text(empty, "No networks found. Press R to rescan.");
        lv_obj_set_pos(empty, 8, 50);
        lv_obj_set_style_text_color(empty, lv_color_hex(0x666666), LV_PART_MAIN);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, LV_PART_MAIN);
        return;
    }

    int visible = 5;
    int offset = list_sel_ - visible / 2;
    if (offset < 0) offset = 0;
    if (offset > ap_count_ - visible) offset = ap_count_ - visible;
    if (offset < 0) offset = 0;

    for (int vi = 0; vi < visible && (vi + offset) < ap_count_; ++vi) {
        int ai = vi + offset;
        bool sel = (ai == list_sel_);
        cp0_wifi_ap_t *ap = &aps_[ai];
        int y = 30 + vi * 22;

        if (sel) {
            lv_obj_t *bg = lv_obj_create(cont);
            lv_obj_set_size(bg, UISetupPage::SCREEN_W - 8, 20);
            lv_obj_set_pos(bg, 4, y);
            lv_obj_set_style_radius(bg, 2, LV_PART_MAIN);
            lv_obj_set_style_bg_color(bg, lv_color_hex(0x1F3A5F), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN);
            lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN);
            lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        }

        uint32_t tc = sel ? 0xFFFFFF : 0xCCCCCC;
        if (ap->in_use) tc = 0x58A6FF;

        lv_obj_t *ssid_lbl = lv_label_create(cont);
        static char ssid_buf[CP0_WIFI_SSID_MAX + 4];
        if (has_saved_profile(ap->ssid))
            snprintf(ssid_buf, sizeof(ssid_buf), "%s *", ap->ssid);
        else
            snprintf(ssid_buf, sizeof(ssid_buf), "%s", ap->ssid);
        lv_label_set_text(ssid_lbl, ssid_buf);
        lv_obj_set_pos(ssid_lbl, 8, y + 2);
        lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(tc), LV_PART_MAIN);
        lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_width(ssid_lbl, 165);
        lv_label_set_long_mode(ssid_lbl, LV_LABEL_LONG_CLIP);

        lv_obj_t *sec = lv_label_create(cont);
        lv_label_set_text(sec, ap->security[0] ? ap->security : "Open");
        lv_obj_set_pos(sec, 180, y + 2);
        lv_obj_set_style_text_color(sec, lv_color_hex(tc), LV_PART_MAIN);
        lv_obj_set_style_text_font(sec, &lv_font_montserrat_10, LV_PART_MAIN);

        char sig_buf[16];
        snprintf(sig_buf, sizeof(sig_buf), "%d%%", ap->signal);
        lv_obj_t *sig = lv_label_create(cont);
        lv_label_set_text(sig, sig_buf);
        lv_obj_set_pos(sig, 275, y + 2);
        lv_obj_set_style_text_color(sig, lv_color_hex(tc), LV_PART_MAIN);
        lv_obj_set_style_text_font(sig, &lv_font_montserrat_10, LV_PART_MAIN);
    }

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "OK:connect  R:rescan  D:forget  ESC:back");
    lv_obj_set_pos(hint, 8, UISetupPage::LIST_H - 14);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

void WiFi::handle_list_key(UISetupPage &page, uint32_t key)
{
    switch (key) {
    case KEY_UP:
        if (list_sel_ > 0) { --list_sel_; build_list(page); }
        break;
    case KEY_DOWN:
        if (list_sel_ < ap_count_ - 1) { ++list_sel_; build_list(page); }
        break;
    case KEY_ENTER:
        if (ap_count_ > 0) try_connect(page, list_sel_);
        break;
    case KEY_R:
        do_scan();
        list_sel_ = 0;
        build_list(page);
        break;
    case KEY_D:
        if (ap_count_ > 0) forget_selected(page);
        break;
    case KEY_ESC:
    case KEY_LEFT:
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.build_sub_view();
        break;
    default:
        break;
    }
}

void WiFi::try_connect(UISetupPage &page, int idx)
{
    if (idx < 0 || idx >= ap_count_) return;
    cp0_wifi_ap_t *ap = &aps_[idx];
    if (ap->in_use) return;

    bool needs_password = false;
    int ret = -1;
    if (strcmp(ap->security, "Open") == 0 || ap->security[0] == 0) {
        show_connecting(page, ap->ssid);
        ret = launcher_wifi::connect(ap->ssid, NULL);
    } else if (has_saved_profile(ap->ssid)) {
        show_connecting(page, ap->ssid);
        ret = launcher_wifi::connect(ap->ssid, NULL);
        if (ret != 0) {
            needs_password = true;
            pw_ssid_ = ap->ssid;
            pw_buf_.clear();
            show_pw_input(page);
        }
    } else {
        needs_password = true;
        pw_ssid_ = ap->ssid;
        pw_buf_.clear();
        show_pw_input(page);
    }
    if (!needs_password) {
        if (ret != 0)
            show_error(page, "Connection failed");
        do_scan();
        build_list(page);
    }
}

void WiFi::show_connecting(UISetupPage &page, const char *ssid)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
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

void WiFi::show_error(UISetupPage &page, const char *msg)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, msg);
    lv_obj_set_pos(lbl, 8, 60);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_refr_now(NULL);
    usleep(2000000);
}

void WiFi::forget_selected(UISetupPage &page)
{
    if (list_sel_ < 0 || list_sel_ >= ap_count_) return;
    cp0_wifi_ap_t *ap = &aps_[list_sel_];

    if (!has_saved_profile(ap->ssid)) {
        show_error(page, "No saved password for this network");
        do_scan();
        build_list(page);
        return;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Forget '%s'?", ap->ssid);
    lv_obj_t *cont = page.ui_obj_["list_cont"];
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

    pw_ssid_ = ap->ssid;
    page.view_state_ = UISetupPage::ViewState::WIFI_PW;
    launcher_wifi::profile_forget(ap->ssid);
    if (ap->in_use)
        launcher_wifi::profile_disconnect_active();

    lv_obj_clean(cont);
    lbl = lv_label_create(cont);
    snprintf(msg, sizeof(msg), "Forgot '%s'", ap->ssid);
    lv_label_set_text(lbl, msg);
    lv_obj_set_pos(lbl, 8, 60);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x33CC33), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_refr_now(NULL);
    usleep(1500000);

    page.view_state_ = UISetupPage::ViewState::WIFI_LIST;
    do_scan();
    build_list(page);
}

bool WiFi::has_saved_profile(const char *ssid)
{
    return launcher_wifi::profile_exists(ssid) != 0;
}

void WiFi::show_pw_input(UISetupPage &page)
{
    page.view_state_ = UISetupPage::ViewState::WIFI_PW;
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);

    lv_obj_t *title = lv_label_create(cont);
    char buf[128];
    snprintf(buf, sizeof(buf), "Connect: %s", pw_ssid_.c_str());
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

void WiFi::handle_pw_key(UISetupPage &page, uint32_t key)
{
    if (key == KEY_ESC) {
        page.view_state_ = UISetupPage::ViewState::WIFI_LIST;
        page.rebuild_view();
        return;
    }
    if (key == KEY_ENTER) {
        if (pw_hint_lbl_) lv_label_set_text(pw_hint_lbl_, "Connecting...");
        lv_refr_now(NULL);
        int ret = launcher_wifi::connect(pw_ssid_.c_str(), pw_buf_.c_str());
        if (ret != 0) {
            launcher_wifi::profile_forget(pw_ssid_.c_str());
            if (pw_hint_lbl_) {
                lv_label_set_text(pw_hint_lbl_, "Failed! Wrong password? Try again.");
                lv_obj_set_style_text_color(pw_hint_lbl_, lv_color_hex(0xFF4444), LV_PART_MAIN);
            }
            pw_buf_.clear();
            pw_update_display();
            return;
        }
        page.view_state_ = UISetupPage::ViewState::WIFI_LIST;
        do_scan();
        page.rebuild_view();
        return;
    }
    if (key == KEY_BACKSPACE) {
        if (!pw_buf_.empty()) pw_buf_.pop_back();
        pw_update_display();
        return;
    }
    if (page.cur_elm_ && page.cur_elm_->utf8[0]) {
        pw_buf_ += page.cur_elm_->utf8;
        pw_update_display();
    }
}

void WiFi::pw_update_display()
{
    if (!pw_input_lbl_) return;
    std::string display = pw_buf_ + "_";
    lv_label_set_text(pw_input_lbl_, display.c_str());
}

void Info::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Info";
    m.sub_items = {
        {"Battery: --%", false, false, nullptr},
        {"Temp: --C", false, false, nullptr},
        {"Current: --mA", false, false, nullptr},
        {"Voltage: --V", false, false, nullptr},
        {"BQ Calibrate", false, false, [page]() { page->info_.enter_bq_calibrate(*page); }},
    };
    m.on_enter = [page]() { page->info_.refresh_values(*page); page->info_.start_timer(*page); };
    menu.push_back(m);
}

void Info::refresh_values(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Info") continue;
        cp0_battery_info_t bat = cp0_battery_read();
        char buf[64];
        snprintf(buf, sizeof(buf), "Battery: %d%%", bat.valid ? bat.soc : 0);
        m.sub_items[0].label = buf;
        snprintf(buf, sizeof(buf), "Temp: %.1fC", bat.valid ? bat.temperature_c10 / 10.0 : 0.0);
        m.sub_items[1].label = buf;
        if (bat.valid && bat.current_ma != INT32_MIN)
            snprintf(buf, sizeof(buf), "Current: %dmA", bat.current_ma);
        else
            snprintf(buf, sizeof(buf), "Current: --mA");
        m.sub_items[2].label = buf;
        snprintf(buf, sizeof(buf), "Voltage: %.2fV", bat.valid ? bat.voltage_mv / 1000.0 : 0.0);
        m.sub_items[3].label = buf;
        break;
    }
    if (page.view_state_ == UISetupPage::ViewState::SUB) refresh_visible_labels(page);
}

void Info::reset_visible_labels()
{
    for (int i = 0; i < 4; ++i) {
        sub_labels_[i] = nullptr;
        sub_label_focused_[i] = false;
        visible_text_[i].clear();
    }
}

void Info::track_visible_label(int index, lv_obj_t *label, bool focused, const std::string &text)
{
    if (index < 0 || index >= 4)
        return;

    sub_labels_[index] = label;
    sub_label_focused_[index] = focused;
    visible_text_[index] = text;
}

void Info::refresh_visible_labels(UISetupPage &page)
{
    if (page.selected_idx_ < 0 || page.selected_idx_ >= (int)page.menu_items_.size())
        return;

    MenuItem &item = page.menu_items_[page.selected_idx_];
    if (item.label != "Info")
        return;

    for (int i = 0; i < 4 && i < (int)item.sub_items.size(); ++i) {
        lv_obj_t *lbl = sub_labels_[i];
        if (!lbl)
            continue;

        const char *new_text = item.sub_items[i].label.c_str();
        if (visible_text_[i] == new_text)
            continue;

        lv_label_set_text(lbl, new_text);
        visible_text_[i] = new_text;
        page.apply_overflow_centered(lbl, UISetupPage::SUB_CENTER_X,
                                     sub_label_focused_[i] ? 80 : UISetupPage::VALUE_BOX_W,
                                     sub_label_focused_[i]);
    }
}

void Info::start_timer(UISetupPage &page)
{
    stop_timer();
    timer_ = lv_timer_create([](lv_timer_t *t) {
        UISetupPage *self = (UISetupPage *)lv_timer_get_user_data(t);
        if (self && self->view_state_ == UISetupPage::ViewState::SUB)
            self->info_.refresh_values(*self);
    }, 1000, &page);
}

void Info::stop_timer()
{
    if (timer_) { lv_timer_delete(timer_); timer_ = nullptr; }
}

void Info::enter_bq_calibrate(UISetupPage &page)
{
    page.val_title_ = "BQ Calib";
    page.val_options_ = {"Enter CAL", "CC Offset", "Board Offset", "Exit CAL"};
    page.val_sel_idx_ = 0;
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void Info::apply_bq_calibrate(UISetupPage &page)
{
    cp0_bq27220_calibrate(page.val_sel_idx_);
}

void RTC::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "RTC";
    m.sub_items = {
        {"NTP", true, true, [page]() { page->rtc_.toggle_ntp(*page); }},
        {"Year", false, false, [page]() { page->rtc_.enter_adjust(*page, 0); }},
        {"Month", false, false, [page]() { page->rtc_.enter_adjust(*page, 1); }},
        {"Day", false, false, [page]() { page->rtc_.enter_adjust(*page, 2); }},
        {"Hour", false, false, [page]() { page->rtc_.enter_adjust(*page, 3); }},
        {"Minute", false, false, [page]() { page->rtc_.enter_adjust(*page, 4); }},
        {"Second", false, false, [page]() { page->rtc_.enter_adjust(*page, 5); }},
    };
    m.on_enter = [page]() { page->rtc_.refresh_values(*page); };
    menu.push_back(m);
}

void RTC::update_labels(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "RTC") continue;
        m.sub_items[0].toggle_state = ntp_on_;
        char buf[32];
        const char *names[] = {"Year", "Month", "Day", "Hour", "Minute", "Second"};
        for (int i = 0; i < 6; ++i) {
            snprintf(buf, sizeof(buf), "%s: %d", names[i], values_[i]);
            m.sub_items[i + 1].label = buf;
        }
        break;
    }
}

void RTC::refresh_values(UISetupPage &page)
{
    ntp_on_ = cp0_time_ntp_get() == 1;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) {
        values_[0] = t->tm_year + 1900;
        values_[1] = t->tm_mon + 1;
        values_[2] = t->tm_mday;
        values_[3] = t->tm_hour;
        values_[4] = t->tm_min;
        values_[5] = t->tm_sec;
    }
    dirty_ = false;
    update_labels(page);
}

void RTC::toggle_ntp(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "RTC") continue;
        bool on = m.sub_items[0].toggle_state;
        cp0_time_ntp_set(on ? 1 : 0);
        break;
    }
    refresh_values(page);
}

void RTC::enter_adjust(UISetupPage &page, int field)
{
    if (ntp_on_)
        return;
    field_ = field;
    const char *names[] = {"Year", "Month", "Day", "Hour", "Minute", "Second"};
    page.val_title_ = names[field];
    int cur = values_[field];
    int mins[] = {2000, 1, 1, 0, 0, 0};
    int maxs[] = {2099, 12, 31, 23, 59, 59};

    page.val_options_.clear();
    for (int v = mins[field]; v <= maxs[field]; ++v) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", v);
        page.val_options_.push_back(buf);
    }
    page.val_sel_idx_ = cur - mins[field];
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void RTC::apply_value(UISetupPage &page)
{
    int new_val = atoi(page.val_options_[page.val_sel_idx_].c_str());
    values_[field_] = new_val;
    dirty_ = true;
    update_labels(page);

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             values_[0], values_[1], values_[2], values_[3], values_[4], values_[5]);
    struct Context {
        RTC *rtc;
        UISetupPage *page;
    };
    auto *context = new (std::nothrow) Context{this, &page};
    if (!context) {
        refresh_values(page);
        return;
    }
    const char *argv[] = {"date", "-s", timestamp, nullptr};
    int rc = cp0_sudo_run_argv_async(
        argv, CP0_SUDO_CALLBACK_LVGL, nullptr,
        [](cp0_sudo_result_t result, int, void *user) {
            std::unique_ptr<Context> context(static_cast<Context *>(user));
            if (result == CP0_SUDO_RESULT_SUCCESS) {
                context->rtc->dirty_ = true;
                context->page->update_datetime_status();
            } else {
                context->rtc->refresh_values(*context->page);
            }
        }, context);
    if (rc != 0) {
        delete context;
        refresh_values(page);
    }
}

void RTC::commit_to_hardware(UISetupPage &page)
{
    struct Context {
        RTC *rtc;
        UISetupPage *page;
    };
    auto *context = new (std::nothrow) Context{this, &page};
    if (!context)
        return;
    const char *argv[] = {"hwclock", "-w", nullptr};
    int rc = cp0_sudo_run_argv_async(
        argv, CP0_SUDO_CALLBACK_LVGL, nullptr,
        [](cp0_sudo_result_t result, int, void *user) {
            std::unique_ptr<Context> context(static_cast<Context *>(user));
            if (result == CP0_SUDO_RESULT_SUCCESS) {
                context->rtc->refresh_values(*context->page);
                context->page->update_datetime_status();
            } else {
                context->rtc->dirty_ = true;
            }
        }, context);
    if (rc != 0) {
        delete context;
        dirty_ = true;
    }
}

void RTC::show_write_confirm(UISetupPage &page)
{
    if (confirm_overlay_)
        return;

    confirm_sel_ = 1;
    lv_obj_t *layer = lv_layer_top();

    confirm_overlay_ = lv_obj_create(layer);
    lv_obj_remove_style_all(confirm_overlay_);
    lv_obj_set_size(confirm_overlay_, UISetupPage::SCREEN_W, UISetupPage::SCREEN_H + 20);
    lv_obj_set_pos(confirm_overlay_, 0, 0);
    lv_obj_set_style_bg_color(confirm_overlay_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(confirm_overlay_, LV_OPA_60, 0);
    lv_obj_clear_flag(confirm_overlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(confirm_overlay_, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *box = lv_obj_create(confirm_overlay_);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 230, 86);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x3A5A8A), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, "Write hardware RTC?");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *hint = lv_label_create(box);
    lv_label_set_text(hint, "OK:confirm  ESC:no");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -7);

    confirm_yes_lbl_ = lv_label_create(box);
    lv_label_set_text(confirm_yes_lbl_, "Yes");
    lv_obj_set_style_text_font(confirm_yes_lbl_, launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), 0);
    lv_obj_align(confirm_yes_lbl_, LV_ALIGN_CENTER, -42, 8);

    confirm_no_lbl_ = lv_label_create(box);
    lv_label_set_text(confirm_no_lbl_, "No");
    lv_obj_set_style_text_font(confirm_no_lbl_, launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), 0);
    lv_obj_align(confirm_no_lbl_, LV_ALIGN_CENTER, 42, 8);

    update_write_confirm_buttons();
    lv_obj_move_foreground(confirm_overlay_);
    lv_refr_now(NULL);
}

void RTC::close_write_confirm()
{
    if (confirm_overlay_) {
        lv_obj_del(confirm_overlay_);
        confirm_overlay_ = nullptr;
    }
    confirm_yes_lbl_ = nullptr;
    confirm_no_lbl_ = nullptr;
}

void RTC::update_write_confirm_buttons()
{
    if (!confirm_yes_lbl_ || !confirm_no_lbl_)
        return;
    lv_obj_set_style_text_color(confirm_yes_lbl_,
                                lv_color_hex(confirm_sel_ == 0 ? 0x00CC66 : 0x888888), 0);
    lv_obj_set_style_text_color(confirm_no_lbl_,
                                lv_color_hex(confirm_sel_ == 1 ? 0x00CC66 : 0x888888), 0);
}

void RTC::handle_write_confirm_key(UISetupPage &page, uint32_t key)
{
    switch (key) {
    case KEY_LEFT:
    case KEY_UP:
        confirm_sel_ = 0;
        update_write_confirm_buttons();
        break;
    case KEY_RIGHT:
    case KEY_DOWN:
        confirm_sel_ = 1;
        update_write_confirm_buttons();
        break;
    case KEY_ENTER:
        page.play_enter();
        close_write_confirm();
        if (confirm_sel_ == 0) {
            commit_to_hardware(page);
        } else {
            refresh_values(page);
        }
        dirty_ = false;
        page.view_state_ = UISetupPage::ViewState::MAIN;
        page.build_main_view();
        break;
    case KEY_ESC:
        page.play_back();
        close_write_confirm();
        refresh_values(page);
        dirty_ = false;
        page.view_state_ = UISetupPage::ViewState::MAIN;
        page.build_main_view();
        break;
    default:
        break;
    }
}

void Developer::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Developer";
    bool adb_en = UISetupPage::config_get_int("adb_debug", 0) != 0;
    m.sub_items = {{"ADB", true, adb_en, [page]() { page->developer_.toggle_adb(*page); }}};
    m.on_enter = [page]() { page->developer_.refresh_adb_status(*page); };
    menu.push_back(m);
}

void Developer::toggle_adb(UISetupPage &page)
{
    int idx = page.find_menu("Developer");
    if (idx < 0) return;
    bool want_on = page.menu_items_[idx].sub_items[0].toggle_state;
    const char *argv[] = {"sudo", kAdbHelper, want_on ? "enable" : "disable", nullptr};
    int rc = cp0_process_run_argv(argv, 0);
    if (rc == 10) {
        UISetupPage::config_set_int("adb_debug", want_on ? 1 : 0);
        UISetupPage::config_save();
        enter_usb_guide(page, want_on);
        return;
    }
    if (rc != 0) {
        page.menu_items_[idx].sub_items[0].toggle_state = !want_on;
        return;
    }
    UISetupPage::config_set_int("adb_debug", want_on ? 1 : 0);
    UISetupPage::config_save();
}

void Developer::refresh_adb_status(UISetupPage &page)
{
    int idx = page.find_menu("Developer");
    if (idx < 0) return;
    char out[64] = {0};
    const char *argv[] = {"systemctl", "is-active", "adbd.service", nullptr};
    cp0_process_capture_argv(argv, out, sizeof(out));
    bool active = (std::strncmp(out, "active", 6) == 0);
    page.menu_items_[idx].sub_items[0].toggle_state = active;
    UISetupPage::config_set_int("adb_debug", active ? 1 : 0);
}

void Developer::enter_usb_guide(UISetupPage &page, bool enabling)
{
    usb_guide_enabling_ = enabling;
    lv_timer_t *t = lv_timer_create([](lv_timer_t *timer) {
        UISetupPage *self = (UISetupPage *)lv_timer_get_user_data(timer);
        lv_timer_delete(timer);
        self->developer_.build_usb_guide_view(*self);
    }, 60, &page);
    lv_timer_set_repeat_count(t, 1);
}

lv_obj_t *Developer::guide_chip(lv_obj_t *parent, int x, int y, int w, int h,
                                uint32_t bg, uint32_t border, int radius, int border_w)
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

lv_obj_t *Developer::guide_label(lv_obj_t *parent, int x, int y, const char *txt,
                                 uint32_t color, const lv_font_t *font)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    return l;
}

void Developer::build_usb_guide_view(UISetupPage &page)
{
    page.view_state_ = UISetupPage::ViewState::USB_GUIDE;
    usb_guide_knob_ = nullptr;
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    const bool en = usb_guide_enabling_;
    const uint32_t C_GREEN = 0x46DC87, C_YEL = 0xF0C850, C_RED = 0xEB5F5F;
    const uint32_t C_WHITE = 0xECECEC, C_GREY = 0x9A9AA0;
    const lv_font_t *f_title = launcher_fonts().get("Montserrat-Bold.ttf", 13, LV_FREETYPE_FONT_STYLE_BOLD);
    const lv_font_t *f_msg = &lv_font_montserrat_10;

    guide_label(cont, 8, 2, en ? "Enable ADB - switch USB to device" : "Disable ADB - switch USB to HUB",
                C_WHITE, f_title ? f_title : &lv_font_montserrat_12);
    guide_chip(cont, 86, 24, 146, 50, 0x282A30, 0x5A5C64, 6, 2);
    guide_label(cont, 120, 28, "CardputerZero", C_GREY, f_msg);
    guide_chip(cont, 218, 30, 12, 12, 0x101012, 0x5A5C64, 3, 2);
    guide_chip(cont, 228, 32, 22, 8, 0xCDCDD2, 0xCDCDD2, 2, 0);
    guide_chip(cont, 250, 34, 60, 4, 0x6A6C72, 0x6A6C72, 2, 0);
    guide_label(cont, 232, 42, "USB-C", C_GREEN, f_msg);
    guide_chip(cont, 24, 28, 32, 44, 0x1A1A1C, 0x5A5C64, 6, 2);
    guide_chip(cont, 33, 33, 14, 34, 0x0E0E10, 0x0E0E10, 4, 0);
    guide_label(cont, 26, 14, "HUB", en ? C_RED : C_GREEN, f_msg);
    guide_label(cont, 28, 72, "USB", en ? C_GREEN : C_GREY, f_msg);
    const int thumb_top = 34, thumb_bot = 54;
    usb_guide_knob_ = guide_chip(cont, 32, en ? thumb_top : thumb_bot, 16, 10, C_GREEN, 0x2A6F49, 3, 1);

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
    guide_label(cont, 8, UISetupPage::LIST_H - 16, "OK: reboot now     ESC: later", C_GREY, &lv_font_montserrat_10);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, usb_guide_knob_);
    lv_anim_set_values(&a, en ? thumb_top : thumb_bot, en ? thumb_bot : thumb_top);
    lv_anim_set_time(&a, 650);
    lv_anim_set_playback_time(&a, 650);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, [](void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); });
    lv_anim_start(&a);
}

void Developer::stop_usb_guide_anims()
{
    if (usb_guide_knob_) lv_anim_del(usb_guide_knob_, nullptr);
    usb_guide_knob_ = nullptr;
}

void Developer::handle_usb_guide_key(UISetupPage &page, uint32_t key)
{
    switch (key) {
    case KEY_ENTER:
    case KEY_RIGHT: {
        stop_usb_guide_anims();
        lv_obj_t *cont = page.ui_obj_["list_cont"];
        lv_obj_clean(cont);
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, "Rebooting...");
        lv_obj_center(lbl);
        cp0_system_reboot();
        break;
    }
    case KEY_ESC:
    case KEY_LEFT:
        stop_usb_guide_anims();
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.build_sub_view();
        break;
    default:
        break;
    }
}

void About::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "About";
    m.sub_items = {
        {"CardputerZero", false, false, nullptr},
        {"LVGL 9.x", false, false, nullptr},
        {"", false, false, nullptr},
        {"", false, false, nullptr},
    };
    m.on_enter = [page]() { About::refresh_info(*page); };
    menu.push_back(m);
}

void About::refresh_info(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
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

void Help::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Help";
    m.sub_items = {{"View Help", false, false, [page]() { Help::enter_page(*page); }}};
    menu.push_back(m);
}

void Help::enter_page(UISetupPage &page)
{
    page.view_state_ = UISetupPage::ViewState::WIFI_LIST;
    lv_obj_t *cont = page.ui_obj_["list_cont"];
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
    add_line("Home: Hold ESC 3s", 0xCCCCCC, &lv_font_montserrat_12);
    add_line("Navigate: Arrow keys / OK / ESC", 0xCCCCCC, &lv_font_montserrat_12);
    add_line("WiFi: Setting > WiFi > Scan", 0xCCCCCC, &lv_font_montserrat_12);

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "ESC: back");
    lv_obj_set_pos(hint, 8, UISetupPage::LIST_H - 14);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

void ExtPort::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "ExtPort";
    bool usb_en = UISetupPage::config_get_int("extport_usb", 1) != 0;
    bool vout_en = UISetupPage::config_get_int("extport_5vout", 1) != 0;
    m.sub_items = {
        {"GROVE5V", true, usb_en, [page]() {
            bool en = page->menu_items_[page->selected_idx_].sub_items[0].toggle_state;
            UISetupPage::config_set_int("extport_usb", en ? 1 : 0);
            UISetupPage::gpio_set("GROVE5V", en ? 1 : 0);
            UISetupPage::config_save();
        }},
        {"EXT5V", true, vout_en, [page]() {
            bool en = page->menu_items_[page->selected_idx_].sub_items[1].toggle_state;
            UISetupPage::config_set_int("extport_5vout", en ? 1 : 0);
            UISetupPage::gpio_set("EXT5V", en ? 1 : 0);
            UISetupPage::config_save();
        }},
    };
    menu.push_back(m);
}

void Ethernet::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Ethernet";
    m.sub_items = {
        {"IP: --", false, false, nullptr},
        {"Gateway: --", false, false, nullptr},
        {"MAC: --", false, false, nullptr},
    };
    m.on_enter = [page]() { Ethernet::refresh_info(*page); };
    menu.push_back(m);
}

void Ethernet::refresh_info(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Ethernet") continue;
        cp0_eth_info_t info;
        cp0_network_default_info_read(&info);
        m.sub_items[0].label = std::string("IP: ") + info.ipv4;
        m.sub_items[1].label = std::string("GW: ") + info.gateway;
        m.sub_items[2].label = std::string("MAC: ") + info.mac;
        break;
    }
}

void Account::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Account";
    m.sub_items = {
        {"Username", false, false, nullptr},
        {"Password", false, false, nullptr},
        {"Hostname", false, false, nullptr},
    };
    m.on_enter = [page]() { Account::refresh_info(*page); };
    menu.push_back(m);
}

void Account::refresh_info(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Account") continue;
        cp0_account_info_t info;
        cp0_account_info_read(&info);
        m.sub_items[0].label = std::string("User: ") + info.user;
        m.sub_items[1].label = "Password: ****";
        m.sub_items[2].label = std::string("Host: ") + info.hostname;
        break;
    }
}

void Update::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Update";
    m.sub_items = {
        {"Check System", false, false, []() { Update::check_system_update(); }},
        {"Update Launcher", false, false, []() { Update::update_launcher(); }},
        {"Version: --", false, false, nullptr},
    };
    m.on_enter = [page]() { Update::refresh_version_info(*page); };
    menu.push_back(m);
}

void Update::refresh_version_info(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Update") continue;
        m.sub_items[2].label = std::string("Version: ") + LAUNCHER_GIT_COMMIT;
        break;
    }
}

void Update::check_system_update()
{
    cp0_system_apt_update_background();
}

void Update::update_launcher()
{
    cp0_system_update_launcher_background();
}


void Bluetooth::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    Bluetooth *bt = &page->bluetooth_;
    MenuItem m;
    m.label = "Bluetooth";
    bt->named_only_ = UISetupPage::config_get_int("bt_named_only", 1) != 0;
    m.sub_items = {
        {"Power", true, false, [bt, page]() { bt->toggle_power(*page); }},
        {"Alias: CardputerZero", false, false, [bt, page]() { bt->enter_alias(*page); }},
        {"Discoverable", true, false, [bt, page]() { bt->toggle_discoverable(*page); }},
        {"Named Only", true, bt->named_only_, [bt, page]() { bt->toggle_named_only(*page); }},
        {"Connected", false, false, [bt, page]() { bt->enter_devices(*page); }},
        {"Scan", false, false, [bt, page]() { bt->enter_scan(*page); }},
    };
    m.on_enter = [bt, page]() { bt->refresh_status(*page); };
    menu.push_back(m);
}



void Bluetooth::enter_devices(UISetupPage &page)
{
    stop_scan_timer();
    list_mode_ = ListMode::Managed;
    page.view_state_ = UISetupPage::ViewState::BT_LIST;
    list_sel_ = 0;
    refresh_devices();
    build_list(page);
}

void Bluetooth::enter_alias(UISetupPage &page)
{
    stop_scan_timer();
    refresh_status(page);
    alias_input_ = alias_.empty() ? "CardputerZero" : alias_;
    page.view_state_ = UISetupPage::ViewState::BT_ALIAS;
    build_alias_view(page);
}

bool Bluetooth::alias_char_allowed(unsigned char ch)
{
    return std::isprint(ch) && ch != '\t' && ch != '\n' && ch != '\r';
}

std::string Bluetooth::alias_sanitized() const
{
    std::string out;
    out.reserve(alias_input_.size());
    for (unsigned char ch : alias_input_) {
        if (alias_char_allowed(ch))
            out.push_back(static_cast<char>(ch));
    }
    if (out.empty())
        out = "CardputerZero";
    return out.substr(0, CP0_BT_NAME_MAX - 1);
}

void Bluetooth::build_alias_view(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    alias_input_lbl_ = nullptr;
    alias_hint_lbl_ = nullptr;

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Bluetooth Name");
    lv_obj_set_pos(title, 10, 10);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, "Name:");
    lv_obj_set_pos(label, 10, 38);
    lv_obj_set_style_text_color(label, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);

    alias_input_lbl_ = lv_label_create(cont);
    lv_obj_set_pos(alias_input_lbl_, 64, 36);
    lv_obj_set_width(alias_input_lbl_, 236);
    lv_label_set_long_mode(alias_input_lbl_, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(alias_input_lbl_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(alias_input_lbl_, &lv_font_montserrat_14, LV_PART_MAIN);
    alias_update_display();

    alias_hint_lbl_ = lv_label_create(cont);
    lv_label_set_text(alias_hint_lbl_, "OK:set  BS:del  ESC:cancel");
    lv_obj_set_pos(alias_hint_lbl_, 10, 70);
    lv_obj_set_style_text_color(alias_hint_lbl_, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(alias_hint_lbl_, &lv_font_montserrat_10, LV_PART_MAIN);
}

void Bluetooth::alias_update_display()
{
    if (!alias_input_lbl_)
        return;
    std::string display = alias_input_ + "_";
    lv_label_set_text(alias_input_lbl_, display.c_str());
}

void Bluetooth::handle_alias_key(UISetupPage &page, uint32_t key)
{
    if (key == KEY_ESC || key == KEY_LEFT) {
        page.play_back();
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.build_sub_view();
        return;
    }
    if (key == KEY_ENTER || key == KEY_RIGHT) {
        std::string alias = alias_sanitized();
        if (alias_hint_lbl_) {
            lv_label_set_text(alias_hint_lbl_, "Setting alias...");
            lv_obj_set_style_text_color(alias_hint_lbl_, lv_color_hex(0xFFAA00), LV_PART_MAIN);
            lv_refr_now(NULL);
        }
        int ret = set_alias(alias);
        if (ret == 0) {
            alias_ = alias;
            refresh_status(page);
            page.view_state_ = UISetupPage::ViewState::SUB;
            page.build_sub_view();
        } else if (alias_hint_lbl_) {
            lv_label_set_text(alias_hint_lbl_, "Set failed");
            lv_obj_set_style_text_color(alias_hint_lbl_, lv_color_hex(0xFF4444), LV_PART_MAIN);
        }
        return;
    }
    if (key == KEY_BACKSPACE) {
        if (!alias_input_.empty())
            alias_input_.pop_back();
        alias_update_display();
        return;
    }
    if (page.cur_elm_ && page.cur_elm_->utf8[0] && alias_input_.size() < CP0_BT_NAME_MAX - 1) {
        const char *text = page.cur_elm_->utf8;
        while (*text && alias_input_.size() < CP0_BT_NAME_MAX - 1) {
            unsigned char ch = static_cast<unsigned char>(*text++);
            if (alias_char_allowed(ch))
                alias_input_ += static_cast<char>(ch);
        }
        alias_update_display();
    }
}

void Bluetooth::enter_scan(UISetupPage &page)
{
    list_mode_ = ListMode::Scan;
    page.view_state_ = UISetupPage::ViewState::BT_LIST;
    list_sel_ = 0;
    start_scan_timer(page);
}

void Bluetooth::build_list(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    rebuild_rows();

    cp0_bt_status_t st = get_status();
    char title_buf[96];
    snprintf(title_buf, sizeof(title_buf), "%s: %s  %.24s",
             list_mode_ == ListMode::Scan ? "Scan" : "Connected",
             st.powered ? "On" : "Off", st.address[0] ? st.address : "--");
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, title_buf);
    lv_obj_set_pos(title, 8, 2);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
    UISetupPage::apply_overflow_handling(title, 8, UISetupPage::WIFI_TITLE_BOX_W, true);

    if (rows_.empty()) {
        lv_obj_t *empty = lv_label_create(cont);
        if (!st.powered)
            lv_label_set_text(empty, "Bluetooth is off. Enable Power first.");
        else if (list_mode_ == ListMode::Scan)
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
    constexpr int hint_y = UISetupPage::LIST_H - 14;
    constexpr int list_bottom_gap = 8;
    int visible = (hint_y - list_bottom_gap - list_y) / row_step;
    if (visible < 1) visible = 1;
    int offset = list_sel_ - visible / 2;
    if (offset < 0) offset = 0;
    if (offset > (int)rows_.size() - visible) offset = (int)rows_.size() - visible;
    if (offset < 0) offset = 0;

    for (int vi = 0; vi < visible && (vi + offset) < (int)rows_.size(); ++vi) {
        int row_index = vi + offset;
        const ListRow &row = rows_[row_index];
        int y = list_y + vi * row_step;

        if (row.is_header) {
            lv_obj_t *header = lv_label_create(cont);
            lv_label_set_text(header, row.title);
            lv_obj_set_pos(header, 8, y + 3);
            lv_obj_set_style_text_color(header, lv_color_hex(0x888888), LV_PART_MAIN);
            lv_obj_set_style_text_font(header, launcher_fonts().get("Montserrat-Bold.ttf", 10, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
            continue;
        }

        bool sel = (row_index == list_sel_);
        cp0_bt_device_t *dev = &devices_[row.device_index];
        if (sel) {
            lv_obj_t *bg = lv_obj_create(cont);
            lv_obj_set_size(bg, UISetupPage::SCREEN_W - 8, 20);
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
    lv_label_set_text(hint, list_mode_ == ListMode::Scan
                                 ? "OK:act R:restart ESC:back"
                                 : "OK:disconnect D:remove ESC:back");
    lv_obj_set_pos(hint, 8, hint_y);
    lv_obj_set_width(hint, 304);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

void Bluetooth::rebuild_rows()
{
    rows_.clear();
    if (list_mode_ == ListMode::Managed) {
        bool has_connected = false;
        for (int i = 0; i < device_count_; ++i) {
            if (should_hide_device(devices_[i]))
                continue;
            if (devices_[i].connected) {
                if (!has_connected) {
                    rows_.push_back({-1, "Connected Devices", true});
                    has_connected = true;
                }
                rows_.push_back({i, nullptr, false});
            }
        }
    } else {
        bool has_devices = false;
        for (int i = 0; i < device_count_; ++i) {
            if (should_hide_device(devices_[i]))
                continue;
            if (!has_devices) {
                rows_.push_back({-1, "Discovered Devices", true});
                has_devices = true;
            }
            rows_.push_back({i, nullptr, false});
        }
    }
    if (list_sel_ >= (int)rows_.size())
        list_sel_ = rows_.empty() ? 0 : (int)rows_.size() - 1;
    if (!rows_.empty() && rows_[list_sel_].is_header)
        select_next_device(1);
}

bool Bluetooth::should_hide_device(const cp0_bt_device_t &dev) const
{
    if (!named_only_)
        return false;
    if (!dev.name[0])
        return true;
    std::string name_hex = normalized_mac_text(dev.name);
    std::string addr_hex = normalized_mac_text(dev.address);
    return !name_hex.empty() && (name_hex == addr_hex || name_hex.size() == 12);
}

std::string Bluetooth::normalized_mac_text(const char *text)
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

int Bluetooth::selected_device_index() const
{
    if (list_sel_ < 0 || list_sel_ >= (int)rows_.size())
        return -1;
    return rows_[list_sel_].is_header ? -1 : rows_[list_sel_].device_index;
}

void Bluetooth::select_next_device(int direction)
{
    if (rows_.empty())
        return;
    int idx = list_sel_;
    for (int steps = 0; steps < (int)rows_.size(); ++steps) {
        idx += direction;
        if (idx < 0 || idx >= (int)rows_.size())
            return;
        if (!rows_[idx].is_header) {
            list_sel_ = idx;
            return;
        }
    }
}

void Bluetooth::show_action(UISetupPage &page, const char *msg, uint32_t color)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, msg);
    lv_obj_set_pos(lbl, 8, 60);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_refr_now(NULL);
}

void Bluetooth::activate_selected(UISetupPage &page)
{
    if (action_busy_)
        return;
    int dev_index = selected_device_index();
    if (dev_index < 0)
        return;
    cp0_bt_device_t dev = devices_[dev_index];
    bool from_scan = list_mode_ == ListMode::Scan;
    if (from_scan)
        stop_scan_timer();
    action_busy_ = true;
    if (dev.connected)
        show_action(page, "Disconnecting...");
    else if (dev.paired)
        show_action(page, "Connecting...");
    else
        show_action(page, "Pairing...");

    struct BtActionResult {
        Bluetooth *bt;
        UISetupPage *page;
        int ret;
        bool from_scan;
    };

    std::thread([this, &page, dev, from_scan]() {
        int ret = -1;
        if (dev.connected) {
            ret = device_command("BtDisconnect", dev.address);
        } else if (dev.paired) {
            ret = device_command("BtConnect", dev.address);
        } else {
            ret = device_command("BtPair", dev.address);
            if (ret == 0)
                ret = device_command("BtConnect", dev.address);
        }

        BtActionResult *result = new BtActionResult{this, &page, ret, from_scan};
        lv_async_call([](void *user) {
            BtActionResult *result = static_cast<BtActionResult *>(user);
            Bluetooth *bt = result->bt;
            UISetupPage *page = result->page;
            if (bt && page) {
                bt->action_busy_ = false;
                if (result->ret != 0) {
                    bt->show_action(*page, "Bluetooth action failed", 0xFF4444);
                } else if (result->from_scan) {
                    bt->list_mode_ = ListMode::Managed;
                }
                bt->refresh_devices();
                if (page->view_state_ == UISetupPage::ViewState::BT_LIST)
                    bt->build_list(*page);
            }
            delete result;
        }, result);
    }).detach();
}

void Bluetooth::remove_selected(UISetupPage &page)
{
    int dev_index = selected_device_index();
    if (dev_index < 0)
        return;
    show_action(page, "Removing...");
    int ret = device_command("BtRemove", devices_[dev_index].address);
    if (ret != 0) {
        show_action(page, "Remove failed", 0xFF4444);
        usleep(1200000);
    }
    refresh_devices();
    build_list(page);
}

void Bluetooth::handle_list_key(UISetupPage &page, uint32_t key)
{
    switch (key) {
    case KEY_UP:
        select_next_device(-1);
        build_list(page);
        break;
    case KEY_DOWN:
        select_next_device(1);
        build_list(page);
        break;
    case KEY_ENTER:
        activate_selected(page);
        break;
    case KEY_D:
        if (list_mode_ == ListMode::Managed)
            remove_selected(page);
        break;
    case KEY_R:
        if (list_mode_ == ListMode::Scan) {
            start_scan_timer(page);
        } else {
            refresh_devices();
            build_list(page);
        }
        break;
    case KEY_ESC:
    case KEY_LEFT:
        stop_scan_timer();
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.build_sub_view();
        break;
    default:
        break;
    }
}

void Bluetooth::copy_string(char *dst, size_t dst_size, const std::string &src)
{
    if (!dst || dst_size == 0)
        return;
    std::snprintf(dst, dst_size, "%s", src.c_str());
}

std::vector<std::string> Bluetooth::split_char(const std::string &line, char delimiter)
{
    std::vector<std::string> cols;
    std::string item;
    std::istringstream row(line);
    while (std::getline(row, item, delimiter))
        cols.push_back(item);
    return cols;
}

bool Bluetooth::decode_status(const std::string &data, cp0_bt_status_t &st)
{
    auto cols = split_char(data, '\t');
    if (cols.size() < 3)
        return false;
    st.powered = std::atoi(cols[0].c_str());
    copy_string(st.address, sizeof(st.address), cols[1]);
    st.discoverable = std::atoi(cols[2].c_str());
    if (cols.size() >= 4)
        copy_string(st.alias, sizeof(st.alias), cols[3]);
    return true;
}

int Bluetooth::decode_devices(const std::string &data, cp0_bt_device_t *out, int max_devices)
{
    if (!out || max_devices <= 0)
        return 0;
    int count = 0;
    std::istringstream lines(data);
    std::string line;
    while (count < max_devices && std::getline(lines, line)) {
        if (line.empty())
            continue;
        auto cols = split_char(line, '\t');
        if (cols.size() < 4)
            continue;
        cp0_bt_device_t dev{};
        copy_string(dev.address, sizeof(dev.address), cols[0]);
        dev.paired = std::atoi(cols[1].c_str());
        dev.connected = std::atoi(cols[2].c_str());
        dev.rssi = std::atoi(cols[3].c_str());
        if (cols.size() >= 6)
            copy_string(dev.name, sizeof(dev.name), cols[5]);
        else if (cols.size() >= 4)
            copy_string(dev.name, sizeof(dev.name), cols[3]);
        out[count++] = dev;
    }
    return count;
}

int Bluetooth::api_int(std::list<std::string> args, int default_value)
{
    int ret = default_value;
    cp0_signal_bt_api(std::move(args), [&](int code, std::string) {
        ret = code;
    });
    return ret;
}

cp0_bt_status_t Bluetooth::get_status()
{
    cp0_bt_status_t st{};
    cp0_signal_bt_api({"BtStatus"}, [&](int code, std::string data) {
        if (code == 0)
            decode_status(data, st);
    });
    return st;
}

int Bluetooth::set_power(int on)
{
    return api_int({"BtPower", std::to_string(on)});
}

int Bluetooth::set_alias(const std::string &alias)
{
    return api_int({"BtAlias", alias});
}

int Bluetooth::set_discoverable(int on)
{
    return api_int({"BtDiscoverable", std::to_string(on)});
}

int Bluetooth::device_command(const char *cmd, const char *address)
{
    return api_int({cmd ? std::string(cmd) : std::string(),
                   address ? std::string(address) : std::string()});
}

int Bluetooth::device_list(const char *cmd, cp0_bt_device_t *out, int max_devices)
{
    int count = 0;
    cp0_signal_bt_api({cmd ? std::string(cmd) : std::string(), std::to_string(max_devices)},
                      [&](int code, std::string data) {
                          count = out && max_devices > 0 ? decode_devices(data, out, max_devices) : code;
                      });
    return count;
}

void Bluetooth::refresh_status(UISetupPage &page)
{
    cp0_bt_status_t st = get_status();
    for (auto &m : page.menu_items_) {
        if (m.label != "Bluetooth") continue;
        m.sub_items[0].toggle_state = st.powered != 0;
        discoverable_ = st.discoverable != 0;
        alias_ = st.alias[0] ? st.alias : "CardputerZero";
        m.sub_items[1].label = "Alias: " + alias_;
        m.sub_items[2].toggle_state = discoverable_;
        break;
    }
}

void Bluetooth::toggle_power(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Bluetooth") continue;
        bool on = m.sub_items[0].toggle_state;
        if (!on)
            stop_scan_timer();
        set_power(on ? 1 : 0);
        refresh_status(page);
        break;
    }
}

void Bluetooth::toggle_named_only(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Bluetooth") continue;
        named_only_ = m.sub_items[3].toggle_state;
        UISetupPage::config_set_int("bt_named_only", named_only_ ? 1 : 0);
        UISetupPage::config_save();
        break;
    }
    if (page.view_state_ == UISetupPage::ViewState::BT_LIST)
        build_list(page);
}

void Bluetooth::toggle_discoverable(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Bluetooth") continue;
        discoverable_ = m.sub_items[2].toggle_state;
        if (set_discoverable(discoverable_ ? 1 : 0) != 0) {
            m.sub_items[2].toggle_state = !discoverable_;
            discoverable_ = m.sub_items[2].toggle_state;
        }
        break;
    }
}

void Bluetooth::start_scan_timer(UISetupPage &page)
{
    stop_scan_timer();
    discovery_active_ = api_int({"BtDiscoveryStart"}, 0) == 0;
    refresh_devices();
    build_list(page);
    if (!discovery_active_)
        return;
    scan_timer_ = lv_timer_create([](lv_timer_t *t) {
        UISetupPage *self = (UISetupPage *)lv_timer_get_user_data(t);
        if (!self || self->view_state_ != UISetupPage::ViewState::BT_LIST ||
            self->bluetooth_.list_mode_ != ListMode::Scan)
            return;
        self->bluetooth_.refresh_devices();
        self->bluetooth_.build_list(*self);
    }, 2500, &page);
}

void Bluetooth::stop_scan_timer()
{
    if (scan_timer_) {
        lv_timer_delete(scan_timer_);
        scan_timer_ = nullptr;
    }
    if (discovery_active_) {
        api_int({"BtDiscoveryStop"}, 0);
        discovery_active_ = false;
    }
}

void Bluetooth::refresh_devices()
{
    if (list_mode_ == ListMode::Managed)
        device_count_ = device_list("BtConnectedList", devices_, CP0_BT_DEVICE_MAX);
    else
        device_count_ = device_list("BtList", devices_, CP0_BT_DEVICE_MAX);
    if (device_count_ < 0)
        device_count_ = 0;
    if (device_count_ == 0)
        list_sel_ = 0;
}

void Bluetooth::do_scan(UISetupPage &page)
{
    enter_scan(page);
}

std::string SoundCard::trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool SoundCard::parse_limits(const std::string &line, int &mn, int &mx)
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

int SoundCard::parse_current_val(const std::string &line)
{
    size_t p = line.find(": ");
    if (p == std::string::npos) return -1;
    int v = 0;
    if (std::sscanf(line.c_str() + p + 2, " %d", &v) == 1) return v;
    return -1;
}

std::string SoundCard::extract_value_str(const std::string &line)
{
    static const char *pfx[] = {
        "Mono:", "Front Left:", "Front Right:", "Rear Left:", "Rear Right:",
        "Center:", "LFE:", "Side Left:", "Side Right:", "Capture:", "Playback:",
        nullptr
    };
    for (int i = 0; pfx[i]; ++i) {
        size_t p = line.find(pfx[i]);
        if (p != std::string::npos) return trim(line.substr(p));
    }
    return trim(line);
}

bool SoundCard::is_value_line(const std::string &tl)
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

void SoundCard::enter_cards(UISetupPage &page)
{
    cards_.clear();
    cp0_signal_soundcard_api({"ListCards"}, [this](int code, std::string data) {
        if (code != 0) return;
        std::istringstream lines(data);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.empty()) continue;
            size_t tab = line.find('\t');
            if (tab == std::string::npos) continue;
            SoundCard::Card c;
            c.index = std::atoi(line.substr(0, tab).c_str());
            c.name  = line.substr(tab + 1);
            cards_.push_back(std::move(c));
        }
    });
    card_sel_ = 0;
    page.view_state_ = UISetupPage::ViewState::SOUNDCARD_CARDS;
    page.transition_enter_level();
}

void SoundCard::enter_controls(UISetupPage &page)
{
    if (cards_.empty()) return;
    card_idx_ = cards_[card_sel_].index;
    controls_.clear();
    cp0_signal_soundcard_api({"ListControls", std::to_string(card_idx_)},
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
                SoundCard::Control c;
                c.name        = cols[0];
                c.type        = cols[1];
                c.min_val     = std::atoi(cols[2].c_str());
                c.max_val     = std::atoi(cols[3].c_str());
                c.step        = std::atoi(cols[4].c_str());
                c.current_str = cols[5];
                c.current_val = std::atoi(cols[6].c_str());
                controls_.push_back(std::move(c));
            }
        });
    ctrl_sel_ = 0;
    page.view_state_  = UISetupPage::ViewState::SOUNDCARD_CONTROLS;
    page.transition_enter_level();
}

void SoundCard::enter_detail(UISetupPage &page)
{
    if (controls_.empty()) return;
    const auto &ctrl = controls_[ctrl_sel_];
    detail_ = SoundCard::Control{};
    detail_.name = ctrl.name;
    cp0_signal_soundcard_api({"GetControlDetail", std::to_string(card_idx_), ctrl.name},
        [this, &ctrl](int code, std::string data) {
            if (code != 0) { detail_ = ctrl; return; }
            std::istringstream ss(data);
            std::string line;
            while (std::getline(ss, line)) {
                std::string tl = trim(line);
                if (tl.rfind("Capabilities:", 0) == 0)
                    detail_.type = (tl.find("enum") != std::string::npos) ? "ENUMERATED" : "INTEGER";
                else if (tl.rfind("Limits:", 0) == 0)
                    parse_limits(tl, detail_.min_val, detail_.max_val);
                else if (detail_.current_str.empty() && is_value_line(tl)) {
                    detail_.current_str = extract_value_str(tl);
                    int v = parse_current_val(tl);
                    if (v >= 0) detail_.current_val = v;
                }
            }
        });
    if (detail_.max_val == 0 && ctrl.max_val != 0) {
        detail_.min_val = ctrl.min_val;
        detail_.max_val = ctrl.max_val;
    }
    input_buf_.clear();
    input_lbl_  = nullptr;
    hint_lbl_  = nullptr;
    page.view_state_    = UISetupPage::ViewState::SOUNDCARD_DETAIL;
    page.transition_enter_level();
}

// ====================================================================
//  Build: card list view
// ====================================================================
void SoundCard::build_cards_view(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);

    // Title
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Sound Cards");
    UISetupPage::apply_fixed_label_box(title, UISetupPage::SC_MARGIN_X, 4, UISetupPage::SC_DETAIL_TEXT_W, false);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);

    if (cards_.empty()) {
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, "No ALSA cards found.");
        UISetupPage::apply_fixed_label_box(lbl, UISetupPage::SC_MARGIN_X, 40, UISetupPage::SC_DETAIL_TEXT_W, false);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "ESC: back");
        UISetupPage::apply_fixed_label_box(hint, UISetupPage::SC_MARGIN_X, UISetupPage::LIST_H - 14, UISetupPage::SC_BOTTOM_HINT_W, false);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
        return;
    }

    int visible = 5;
    int total   = (int)cards_.size();
    int offset  = card_sel_ - visible / 2;
    if (offset < 0) offset = 0;
    if (total > visible && offset > total - visible) offset = total - visible;

    for (int vi = 0; vi < visible && (vi + offset) < total; ++vi) {
        int ai  = vi + offset;
        bool sel = (ai == card_sel_);
        int  y   = 22 + vi * 22;

        if (sel) {
            lv_obj_t *bg = lv_obj_create(cont);
            lv_obj_set_size(bg, UISetupPage::SCREEN_W - 8, 20);
            lv_obj_set_pos(bg, 4, y);
            lv_obj_set_style_radius(bg, 2, LV_PART_MAIN);
            lv_obj_set_style_bg_color(bg, lv_color_hex(0x1F3A5F), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN);
            lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN);
            lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        }

        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, cards_[ai].name.c_str());
        UISetupPage::apply_fixed_label_box(lbl, UISetupPage::SC_ROW_X, y + 2, UISetupPage::SC_CARD_NAME_W, sel);
        lv_obj_set_style_text_color(lbl, lv_color_hex(sel ? 0xFFFFFF : 0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    }

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "OK: open  ESC: back");
    UISetupPage::apply_fixed_label_box(hint, UISetupPage::SC_MARGIN_X, UISetupPage::LIST_H - 14, UISetupPage::SC_BOTTOM_HINT_W, false);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

// ====================================================================
//  Build: control list view
// ====================================================================
void SoundCard::build_controls_view(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);

    // Title: card name
    char title_buf[80];
    if (!cards_.empty() && card_sel_ < (int)cards_.size())
        std::snprintf(title_buf, sizeof(title_buf), "%s", cards_[card_sel_].name.c_str());
    else
        std::snprintf(title_buf, sizeof(title_buf), "Card %d", card_idx_);

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, title_buf);
    UISetupPage::apply_fixed_label_box(title, UISetupPage::SC_MARGIN_X, 4, UISetupPage::SC_DETAIL_TEXT_W, true);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);

    if (controls_.empty()) {
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, "No controls found.");
        UISetupPage::apply_fixed_label_box(lbl, UISetupPage::SC_MARGIN_X, 40, UISetupPage::SC_DETAIL_TEXT_W, false);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "ESC: back");
        UISetupPage::apply_fixed_label_box(hint, UISetupPage::SC_MARGIN_X, UISetupPage::LIST_H - 14, UISetupPage::SC_BOTTOM_HINT_W, false);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
        return;
    }

    int visible = 5;
    int total   = (int)controls_.size();
    int offset  = ctrl_sel_ - visible / 2;
    if (offset < 0) offset = 0;
    if (total > visible && offset > total - visible) offset = total - visible;

    for (int vi = 0; vi < visible && (vi + offset) < total; ++vi) {
        int ai  = vi + offset;
        bool sel = (ai == ctrl_sel_);
        int  y   = 20 + vi * 22;

        if (sel) {
            lv_obj_t *bg = lv_obj_create(cont);
            lv_obj_set_size(bg, UISetupPage::SCREEN_W - 8, 20);
            lv_obj_set_pos(bg, 4, y);
            lv_obj_set_style_radius(bg, 2, LV_PART_MAIN);
            lv_obj_set_style_bg_color(bg, lv_color_hex(0x1F3A5F), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN);
            lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN);
            lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        }

        const auto &ctrl = controls_[ai];

        // Left: control name
        lv_obj_t *name_lbl = lv_label_create(cont);
        lv_label_set_text(name_lbl, ctrl.name.c_str());
        UISetupPage::apply_fixed_label_box(name_lbl, UISetupPage::SC_CTRL_NAME_X, y + 2, UISetupPage::SC_CTRL_NAME_W, sel);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(sel ? 0xFFFFFF : 0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, LV_PART_MAIN);

        // Right: current value summary
        if (!ctrl.current_str.empty()) {
            lv_obj_t *val_lbl = lv_label_create(cont);
            lv_label_set_text(val_lbl, ctrl.current_str.c_str());
            UISetupPage::apply_fixed_label_box(val_lbl, UISetupPage::SC_CTRL_VALUE_X, y + 2, UISetupPage::SC_CTRL_VALUE_W, sel);
            lv_obj_set_style_text_color(val_lbl, lv_color_hex(sel ? 0xAADDFF : 0x888888), LV_PART_MAIN);
            lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
        }
    }

    // Scroll arrows
    if (ctrl_sel_ > 0) {
        lv_obj_t *a = lv_img_create(cont);
        lv_img_set_src(a, page.img_arrow_up_.c_str());
        lv_obj_set_pos(a, UISetupPage::SCREEN_W / 2 - 8, 2);
    }
    if (ctrl_sel_ < total - 1) {
        lv_obj_t *a = lv_img_create(cont);
        lv_img_set_src(a, page.img_arrow_down_.c_str());
        lv_obj_set_pos(a, UISetupPage::SCREEN_W / 2 - 8, UISetupPage::LIST_H - 28);
    }

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "OK: edit  ESC: back");
    UISetupPage::apply_fixed_label_box(hint, UISetupPage::SC_MARGIN_X, UISetupPage::LIST_H - 14, UISetupPage::SC_BOTTOM_HINT_W, false);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

// ====================================================================
//  Build: detail / input view
// ====================================================================
void SoundCard::build_detail_view(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    input_lbl_ = nullptr;
    hint_lbl_ = nullptr;

    // Control name (header)
    lv_obj_t *name_lbl = lv_label_create(cont);
    lv_label_set_text(name_lbl, detail_.name.c_str());
    UISetupPage::apply_fixed_label_box(name_lbl, UISetupPage::SC_MARGIN_X, 4, UISetupPage::SC_DETAIL_TEXT_W, true);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(name_lbl, launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);

    // Info block: Limits + current value
    char info_buf[128];
    std::snprintf(info_buf, sizeof(info_buf),
                  "Limits: %d - %d", detail_.min_val, detail_.max_val);
    lv_obj_t *limits_lbl = lv_label_create(cont);
    lv_label_set_text(limits_lbl, info_buf);
    UISetupPage::apply_fixed_label_box(limits_lbl, UISetupPage::SC_MARGIN_X, 26, UISetupPage::SC_DETAIL_TEXT_W, false);
    lv_obj_set_style_text_color(limits_lbl, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(limits_lbl, &lv_font_montserrat_12, LV_PART_MAIN);

    if (!detail_.current_str.empty()) {
        lv_obj_t *cur_lbl = lv_label_create(cont);
        lv_label_set_text(cur_lbl, detail_.current_str.c_str());
        UISetupPage::apply_fixed_label_box(cur_lbl, UISetupPage::SC_MARGIN_X, 44, UISetupPage::SC_DETAIL_TEXT_W, true);
        lv_obj_set_style_text_color(cur_lbl, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(cur_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    }

    // Separator line
    lv_obj_t *sep = lv_obj_create(cont);
    lv_obj_set_size(sep, UISetupPage::SCREEN_W - 16, 1);
    lv_obj_set_pos(sep, 8, 64);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sep, 255, LV_PART_MAIN);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    // "New value:" label
    lv_obj_t *new_lbl = lv_label_create(cont);
    lv_label_set_text(new_lbl, "New value:");
    UISetupPage::apply_fixed_label_box(new_lbl, UISetupPage::SC_MARGIN_X, 72, UISetupPage::SC_INPUT_X - UISetupPage::SC_MARGIN_X - 4, false);
    lv_obj_set_style_text_color(new_lbl, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(new_lbl, &lv_font_montserrat_12, LV_PART_MAIN);

    // Input field (digits + cursor)
    cursor_vis_ = true;
    input_lbl_ = lv_label_create(cont);
    input_update_display();
    UISetupPage::apply_fixed_label_box(input_lbl_, UISetupPage::SC_INPUT_X, 70, UISetupPage::SC_INPUT_W, false);
    lv_obj_set_style_text_color(input_lbl_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(input_lbl_, &lv_font_montserrat_14, LV_PART_MAIN);

    // Blinking cursor timer (500 ms period)
    cursor_timer_ = lv_timer_create([](lv_timer_t *timer) {
        auto *self = static_cast<SoundCard *>(lv_timer_get_user_data(timer));
        self->cursor_vis_ = !self->cursor_vis_;
        self->input_update_display();
    }, 500, this);

    // Range hint below input
    char range_buf[64];
    std::snprintf(range_buf, sizeof(range_buf), "Range: %d ~ %d",
                  detail_.min_val, detail_.max_val);
    hint_lbl_ = lv_label_create(cont);
    lv_label_set_text(hint_lbl_, range_buf);
    UISetupPage::apply_fixed_label_box(hint_lbl_, UISetupPage::SC_MARGIN_X, 92, UISetupPage::SC_DETAIL_TEXT_W, false);
    lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint_lbl_, &lv_font_montserrat_10, LV_PART_MAIN);

    // Bottom hint
    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "0-9:type  BS:del  OK:apply  ESC:back");
    UISetupPage::apply_fixed_label_box(hint, UISetupPage::SC_MARGIN_X, UISetupPage::LIST_H - 14, UISetupPage::SC_BOTTOM_HINT_W, true);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

void SoundCard::input_update_display()
{
    if (!input_lbl_) return;
    std::string disp = input_buf_ + (cursor_vis_ ? "_" : " ");
    lv_label_set_text(input_lbl_, disp.c_str());
}

void SoundCard::cursor_stop()
{
    if (cursor_timer_) {
        lv_timer_del(cursor_timer_);
        cursor_timer_ = nullptr;
    }
    cursor_vis_ = true;
}

// Apply the typed value via cp0_signal_soundcard_api
void SoundCard::apply_value(UISetupPage &page)
{
    if (input_buf_.empty()) return;

    int new_val = std::atoi(input_buf_.c_str());
    // Clamp to declared limits when they are known
    if (detail_.max_val > detail_.min_val) {
        if (new_val < detail_.min_val) new_val = detail_.min_val;
        if (new_val > detail_.max_val) new_val = detail_.max_val;
    }

    // Visual feedback while applying
    if (hint_lbl_) {
        lv_label_set_text(hint_lbl_, "Applying...");
        lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0xFFAA00), LV_PART_MAIN);
        lv_refr_now(NULL);
    }

    int rc = -1;
    cp0_signal_soundcard_api(
        {"SetControl", std::to_string(card_idx_), detail_.name, std::to_string(new_val)},
        [&rc](int code, std::string) { rc = code; });

    if (hint_lbl_) {
        if (rc == 0) {
            lv_label_set_text(hint_lbl_, "Applied OK");
            lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0x33CC33), LV_PART_MAIN);
        } else {
            lv_label_set_text(hint_lbl_, "Error (check amixer)");
            lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0xFF4444), LV_PART_MAIN);
        }
        lv_refr_now(NULL);
    }

    // Refresh the control list entry with the new value
    if (rc == 0 && ctrl_sel_ < (int)controls_.size()) {
        char val_str[32];
        std::snprintf(val_str, sizeof(val_str), "%d", new_val);
        controls_[ctrl_sel_].current_val = new_val;
        controls_[ctrl_sel_].current_str = val_str;
    }

    // Go back to control list after a short pause
    cursor_stop();
    input_lbl_  = nullptr;
    hint_lbl_  = nullptr;
    page.view_state_ = UISetupPage::ViewState::SOUNDCARD_CONTROLS;
    lv_timer_t *t = lv_timer_create([](lv_timer_t *timer) {
        auto *self = static_cast<UISetupPage *>(lv_timer_get_user_data(timer));
        lv_timer_del(timer);
        self->transition_back_level();
    }, 900, &page);
    (void)t;
}

// ====================================================================
//  Key handlers
// ====================================================================
void SoundCard::handle_cards_key(UISetupPage &page, uint32_t key)
{
    int total = (int)cards_.size();
    switch (key) {
    case KEY_UP:
        if (card_sel_ > 0) { --card_sel_; build_cards_view(page); }
        break;
    case KEY_DOWN:
        if (card_sel_ < total - 1) { ++card_sel_; build_cards_view(page); }
        break;
    case KEY_ENTER:
    case KEY_RIGHT:
        if (total > 0) { page.play_enter(); enter_controls(page); }
        break;
    case KEY_ESC:
    case KEY_LEFT:
        page.play_back();
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.transition_back_level();
        break;
    default:
        break;
    }
}

void SoundCard::handle_controls_key(UISetupPage &page, uint32_t key)
{
    int total = (int)controls_.size();
    switch (key) {
    case KEY_UP:
        if (ctrl_sel_ > 0) { --ctrl_sel_; build_controls_view(page); }
        break;
    case KEY_DOWN:
        if (ctrl_sel_ < total - 1) { ++ctrl_sel_; build_controls_view(page); }
        break;
    case KEY_ENTER:
    case KEY_RIGHT:
        if (total > 0) { page.play_enter(); enter_detail(page); }
        break;
    case KEY_ESC:
    case KEY_LEFT:
        page.play_back();
        page.view_state_ = UISetupPage::ViewState::SOUNDCARD_CARDS;
        page.transition_back_level();
        break;
    default:
        break;
    }
}

void SoundCard::handle_detail_key(UISetupPage &page, uint32_t key)
{
    // Digit keys: accumulate input
    if (key == KEY_0 || (key >= KEY_1 && key <= KEY_9)) {
        // KEY_1..KEY_9 map to '1'..'9', KEY_0 maps to '0'
        // Linux input key codes: KEY_1=2..KEY_9=10, KEY_0=11
        int digit = -1;
        if (key == KEY_0)         digit = 0;
        else if (key >= KEY_1 && key <= KEY_9) digit = (int)(key - KEY_1 + 1);
        if (digit >= 0 && input_buf_.size() < 8) {
            input_buf_ += (char)('0' + digit);
            input_update_display();
        }
        return;
    }

    switch (key) {
    case KEY_BACKSPACE:
        if (!input_buf_.empty()) {
            input_buf_.pop_back();
            input_update_display();
        }
        break;
    case KEY_ENTER:
    case KEY_RIGHT:
        apply_value(page);
        break;
    case KEY_ESC:
    case KEY_LEFT:
        cursor_stop();
        page.play_back();
        page.view_state_ = UISetupPage::ViewState::SOUNDCARD_CONTROLS;
        page.transition_back_level();
        break;
    default:
        // Also accept typed digit characters forwarded via page.cur_elm_->utf8
        if (page.cur_elm_ && page.cur_elm_->utf8[0] >= '0' && page.cur_elm_->utf8[0] <= '9') {
            if (input_buf_.size() < 8) {
                input_buf_ += page.cur_elm_->utf8[0];
                input_update_display();
            }
        }
        break;
    }
}


void SoundCard::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "SoundCard";
    m.sub_items = {{"Open Mixer", false, false, [page]() { page->soundcard_.enter_cards(*page); }}};
    menu.push_back(m);
}

void build_menu(UISetupPage &page)
{
    page.menu_items_.clear();
    Launcher::append(page, page.menu_items_);
    Boot::append(page, page.menu_items_);
    Screen::append(page, page.menu_items_);
    WiFi::append(page, page.menu_items_);
    Speaker::append(page, page.menu_items_);
    Camera::append(page, page.menu_items_);
    Info::append(page, page.menu_items_);
    About::append(page, page.menu_items_);
    Help::append(page, page.menu_items_);
    ExtPort::append(page, page.menu_items_);
    Developer::append(page, page.menu_items_);
    RTC::append(page, page.menu_items_);
    Bluetooth::append(page, page.menu_items_);
    Ethernet::append(page, page.menu_items_);
    Account::append(page, page.menu_items_);
    Update::append(page, page.menu_items_);
    SoundCard::append(page, page.menu_items_);
}

} // namespace setting
