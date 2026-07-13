/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "setting/rtc_ntp_state.hpp"
#include "setting/adb_state.hpp"
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
#include "cp0_async_testable_utils.hpp"
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
    Developer();
    ~Developer();
    static void append(UISetupPage &p, std::vector<MenuItem> &menu);
    void toggle_adb(UISetupPage &page);
    void refresh_adb_status(UISetupPage &page);
    void enter_usb_guide(UISetupPage &page, bool enabling);
    void build_usb_guide_view(UISetupPage &page);
    void stop_usb_guide_anims();
    void handle_usb_guide_key(UISetupPage &page, uint32_t key);
    void handle_status_key(UISetupPage &page, uint32_t key);
    bool status_active() const { return status_overlay_ != nullptr; }
private:
    struct AsyncState { bool alive = true; uint64_t request_id = 0; };
    enum class Modal { NONE, BUSY, ERROR };
    static constexpr const char *kAdbHelper = "/usr/share/APPLaunch/adb/cardputer-adb";
    static lv_obj_t *guide_chip(lv_obj_t *parent, int x, int y, int w, int h,
                                uint32_t bg, uint32_t border, int radius, int border_w);
    static lv_obj_t *guide_label(lv_obj_t *parent, int x, int y, const char *txt,
                                 uint32_t color, const lv_font_t *font);
    void show_status(const char *title, const char *detail, Modal modal);
    void close_status();
    void update_toggle(UISetupPage &page, bool enabled, bool save);
    void show_result_error(cp0_sudo_result_t result, int exit_code);
    std::shared_ptr<AsyncState> async_state_;
    Modal modal_ = Modal::NONE;
    lv_obj_t *status_overlay_ = nullptr;
    bool usb_guide_enabling_ = true;
    lv_obj_t *usb_guide_knob_ = nullptr;
};

class RTC {
public:
    RTC();
    ~RTC();
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
    bool ntp_available() const { return ntp_available_; }
    bool write_confirm_active() const { return confirm_overlay_ != nullptr; }
    void clear_dirty() { dirty_ = false; }
private:
    struct AsyncState {
        bool alive = true;
        uint64_t request_id = 0;
    };
    enum class Modal { NONE, CONFIRM, BUSY, ERROR };
    static int days_in_month(int year, int month);
    void show_status(const char *title, const char *detail, Modal modal);
    void finish_request();
    void show_result_error(cp0_sudo_result_t result, int exit_code, const char *operation);
    void update_labels(UISetupPage &page);
    void update_write_confirm_buttons();
    int values_[6] = {2026, 1, 1, 0, 0, 0};
    int field_ = 0;
    bool ntp_on_ = true;
    bool dirty_ = false;
    bool ntp_available_ = true;
    bool ntp_previous_ = true;
    Modal modal_ = Modal::NONE;
    std::shared_ptr<AsyncState> async_state_;
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
            lv_label_set_text(hint, rtc_.ntp_available()
                                      ? (cur_sub.toggle_state ? "ok:disable" : "ok:enable")
                                      : "unavailable");
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

        if (developer_.status_active()) {
            if (released) developer_.handle_status_key(*this, key);
            return;
        }

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
