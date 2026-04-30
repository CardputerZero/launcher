#pragma once
#include "ui_app_page.hpp"
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

// ============================================================
//  系统设置界面  UISetupPage
//  屏幕分辨率: 320 x 170  (顶栏20px, ui_APP_Container 320x150)
//
//  视图状态:
//    VIEW_MAIN    — 主菜单列表
//    VIEW_SUB     — 二级设置页面
//    VIEW_WIFI_PW — WiFi 密码输入
// ============================================================

class UISetupPage : public app_base
{
    enum class ViewState { MAIN, SUB, WIFI_PW };

    struct MenuItem
    {
        const char *icon;
        const char *label;
        const char *sub_title;
        std::function<void(lv_obj_t *container)> build_sub;
        std::function<void(uint32_t key)>        on_sub_key;
    };

public:
    UISetupPage() : app_base()
    {
        menu_init();
        creat_UI();
        event_handler_init();
    }
    ~UISetupPage() {}

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    std::vector<MenuItem> menu_items_;
    int   selected_idx_  = 0;
    ViewState view_state_ = ViewState::MAIN;

    static constexpr int ITEM_H       = 28;
    static constexpr int VISIBLE_ROWS = 4;
    static constexpr int LIST_Y       = 22;
    static constexpr int LIST_H       = 128;

    // ---- WiFi sub-page state ----
    hal_wifi_ap_t wifi_aps_[WIFI_AP_MAX];
    int wifi_ap_count_ = 0;
    int wifi_sel_      = 0;
    lv_obj_t *wifi_list_cont_ = nullptr;
    lv_obj_t *wifi_status_lbl_ = nullptr;
    bool wifi_scanning_ = false;

    // ---- WiFi password input state ----
    std::string wifi_pw_ssid_;
    std::string wifi_pw_buf_;
    lv_obj_t *pw_input_lbl_ = nullptr;
    lv_obj_t *pw_hint_lbl_  = nullptr;
    struct key_item *cur_elm_ = nullptr;

    // ---- Brightness sub-page state ----
    lv_obj_t *bright_bar_  = nullptr;
    lv_obj_t *bright_lbl_  = nullptr;
    int bright_val_ = 75;

    // ---- Volume sub-page state ----
    lv_obj_t *vol_bar_  = nullptr;
    lv_obj_t *vol_lbl_  = nullptr;
    lv_obj_t *mute_lbl_ = nullptr;
    int vol_val_  = 39;
    int vol_muted_ = 0;
    int vol_pre_mute_ = 39;

    // ---- Bluetooth state ----
    lv_obj_t *bt_status_lbl_ = nullptr;
    int bt_powered_ = 0;

    // ---- Power sub-page labels ----
    lv_obj_t *pwr_batt_lbl_ = nullptr;
    lv_obj_t *pwr_volt_lbl_ = nullptr;
    lv_obj_t *pwr_curr_lbl_ = nullptr;
    lv_obj_t *pwr_temp_lbl_ = nullptr;
    lv_obj_t *pwr_cap_lbl_  = nullptr;
    lv_obj_t *pwr_hint_lbl_ = nullptr;
    lv_obj_t *pwr_calib_row_ = nullptr;
    lv_timer_t *pwr_timer_ = nullptr;
    bool power_in_calib_ = false;

    // ---- Battery monitor sub-page labels ----
    lv_obj_t *bqmon_line_lbl_   = nullptr;
    lv_obj_t *bqmon_path_lbl_   = nullptr;
    lv_obj_t *bqmon_status_lbl_ = nullptr;

    // ---- BQ27220 calibration sub-page state ----
    lv_obj_t *bqcal_info_lbl_   = nullptr;
    lv_obj_t *bqcal_status_lbl_ = nullptr;
    lv_obj_t *bqcal_rows_[5]    = {nullptr};
    int bqcal_sel_ = 0;

    static uint32_t fzxc_to_arrow(uint32_t key)
    {
        switch (key) {
        case KEY_F: return KEY_UP;
        case KEY_X: return KEY_DOWN;
        case KEY_Z: return KEY_LEFT;
        case KEY_C: return KEY_RIGHT;
        default:    return key;
        }
    }

    // ==================== helper: styled label ====================
    static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                                int x, int y, uint32_t color = 0xE6EDF3,
                                const lv_font_t *font = &lv_font_montserrat_12)
    {
        lv_obj_t *lbl = lv_label_create(parent);
        lv_label_set_text(lbl, text);
        lv_obj_set_pos(lbl, x, y);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN | LV_STATE_DEFAULT);
        return lbl;
    }

    static lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h,
                               uint32_t bg = 0x161B22, uint32_t border = 0x30363D)
    {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, w, h);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_style_radius(card, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(card, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(card, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(card, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        return card;
    }

    static lv_obj_t *make_metric_card(lv_obj_t *parent, int x, int y, const char *title,
                                      const char *icon, uint32_t accent)
    {
        lv_obj_t *card = make_card(parent, x, y, 142, 36);
        make_label(card, icon, 7, 3, accent, &lv_font_montserrat_12);
        make_label(card, title, 26, 2, 0x8B949E, g_font_cn_12 ? g_font_cn_12 : &lv_font_montserrat_12);
        lv_obj_t *value = make_label(card, "--", 26, 16, 0xF0F6FC, &lv_font_montserrat_12);
        lv_obj_set_width(value, 108);
        lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
        return value;
    }

    // ==================== helper: progress bar ====================
    static lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h,
                              int val, int mx, uint32_t indicator_color)
    {
        lv_obj_t *bar = lv_bar_create(parent);
        lv_obj_set_size(bar, w, h);
        lv_obj_set_pos(bar, x, y);
        lv_bar_set_range(bar, 0, mx);
        lv_bar_set_value(bar, val, LV_ANIM_OFF);
        lv_obj_set_style_radius(bar, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bar, lv_color_hex(indicator_color), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bar, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        return bar;
    }

    // ==================== 菜单数据初始化 ====================
    void menu_init()
    {
        // ---- WiFi ----
        menu_items_.push_back({
            LV_SYMBOL_WIFI,
            "Wi-Fi",
            "Wi-Fi Settings",
            [this](lv_obj_t *c) { build_wifi_page(c); },
            [this](uint32_t key) { handle_wifi_key(key); }
        });

        // ---- Bluetooth ----
        menu_items_.push_back({
            LV_SYMBOL_BLUETOOTH,
            "Bluetooth",
            "Bluetooth Settings",
            [this](lv_obj_t *c) { build_bt_page(c); },
            [this](uint32_t key) { handle_bt_key(key); }
        });

        // ---- Display ----
        menu_items_.push_back({
            LV_SYMBOL_IMAGE,
            "Display",
            "Display Settings",
            [this](lv_obj_t *c) { build_display_page(c); },
            [this](uint32_t key) { handle_display_key(key); }
        });

        // ---- Sound ----
        menu_items_.push_back({
            LV_SYMBOL_AUDIO,
            "Sound",
            "Sound Settings",
            [this](lv_obj_t *c) { build_sound_page(c); },
            [this](uint32_t key) { handle_sound_key(key); }
        });

        // ---- Power ----
        menu_items_.push_back({
            LV_SYMBOL_POWER,
            "Power",
            "Battery Info",
            [this](lv_obj_t *c) { build_power_page(c); },
            [this](uint32_t key) { handle_power_key(key); }
        });

        // ---- Shutdown (confirm sub-page) ----
        menu_items_.push_back({
            LV_SYMBOL_POWER,
            "Shutdown",
            "Shutdown Device",
            [this](lv_obj_t *c) { build_poweraction_page(c, /*is_reboot=*/false); },
            [this](uint32_t key) { handle_poweraction_key(key, /*is_reboot=*/false); }
        });

        // ---- Reboot (confirm sub-page) ----
        menu_items_.push_back({
            LV_SYMBOL_REFRESH,
            "Reboot",
            "Reboot Device",
            [this](lv_obj_t *c) { build_poweraction_page(c, /*is_reboot=*/true); },
            [this](uint32_t key) { handle_poweraction_key(key, /*is_reboot=*/true); }
        });

        // ---- About ----
        menu_items_.push_back({
            LV_SYMBOL_LIST,
            "About",
            "About Device",
            [](lv_obj_t *c) {
                const char *lines[] = {
                    "Device  : M5Cardputer Zero",
                    "FW Ver  : v1.0.0",
                    "LVGL    : 9.x",
                    "Build   : " __DATE__,
                };
                for (int i = 0; i < 4; ++i) {
                    lv_obj_t *lbl = lv_label_create(c);
                    lv_label_set_text(lbl, lines[i]);
                    lv_obj_set_pos(lbl, 0, 4 + i * 26);
                    lv_obj_set_style_text_color(lbl, lv_color_hex(i == 0 ? 0x58A6FF : 0xE6EDF3),
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            },
            nullptr
        });
    }

    // ==================== WiFi sub-page ====================
    void build_wifi_page(lv_obj_t *c)
    {
        hal_wifi_status_t st = hal_wifi_get_status();
        char buf[128];
        if (st.connected)
            snprintf(buf, sizeof(buf), "%s  %s  %ddBm  %s",
                     LV_SYMBOL_WIFI, st.ssid, st.signal, st.ip);
        else
            snprintf(buf, sizeof(buf), "%s  Disconnected", LV_SYMBOL_WIFI);
        wifi_status_lbl_ = make_label(c, buf, 0, 2, 0x58A6FF);

        make_label(c, "Scanning...", 0, 22, 0x888888);
        wifi_list_cont_ = lv_obj_create(c);
        lv_obj_set_size(wifi_list_cont_, 296, 80);
        lv_obj_set_pos(wifi_list_cont_, 0, 20);
        lv_obj_set_style_bg_opa(wifi_list_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(wifi_list_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(wifi_list_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(wifi_list_cont_, LV_OBJ_FLAG_SCROLLABLE);

        make_label(c, "UP/DN:select ENTER:connect R:refresh", 0, 102, 0x555555, &lv_font_montserrat_10);

        wifi_do_scan();
    }

    void wifi_do_scan()
    {
        wifi_ap_count_ = hal_wifi_scan(wifi_aps_, WIFI_AP_MAX);
        wifi_sel_ = 0;
        wifi_build_ap_rows();
    }

    void wifi_build_ap_rows()
    {
        if (!wifi_list_cont_) return;
        lv_obj_clean(wifi_list_cont_);

        if (wifi_ap_count_ == 0) {
            make_label(wifi_list_cont_, "No networks found", 4, 8, 0x888888);
            return;
        }

        int visible = 4;
        int offset = wifi_sel_ - visible / 2;
        if (offset < 0) offset = 0;
        if (offset > wifi_ap_count_ - visible) offset = wifi_ap_count_ - visible;
        if (offset < 0) offset = 0;

        for (int vi = 0; vi < visible && (vi + offset) < wifi_ap_count_; ++vi) {
            int ai = vi + offset;
            bool sel = (ai == wifi_sel_);
            hal_wifi_ap_t *ap = &wifi_aps_[ai];

            lv_obj_t *row = lv_obj_create(wifi_list_cont_);
            lv_obj_set_size(row, 294, 18);
            lv_obj_set_pos(row, 0, vi * 20);
            lv_obj_set_style_radius(row, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            uint32_t bg = sel ? 0x1F3A5F : 0x161B22;
            lv_obj_set_style_bg_color(row, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

            char txt[128];
            const char *lock = (strcmp(ap->security, "Open") == 0 || ap->security[0] == 0)
                                ? "" : LV_SYMBOL_EYE_CLOSE;
            const char *conn = ap->in_use ? " *" : "";
            snprintf(txt, sizeof(txt), "%s %s%s  %d%%  %s",
                     LV_SYMBOL_WIFI, ap->ssid, conn, ap->signal, lock);

            uint32_t tc = sel ? 0xFFFFFF : 0xCCCCCC;
            if (ap->in_use) tc = 0x58A6FF;
            make_label(row, txt, 4, 1, tc);
        }
    }

    void handle_wifi_key(uint32_t key)
    {
        if (view_state_ == ViewState::WIFI_PW) {
            handle_wifi_pw_key(key);
            return;
        }
        switch (key) {
        case KEY_UP:
            if (wifi_sel_ > 0) { --wifi_sel_; wifi_build_ap_rows(); }
            break;
        case KEY_DOWN:
            if (wifi_sel_ < wifi_ap_count_ - 1) { ++wifi_sel_; wifi_build_ap_rows(); }
            break;
        case KEY_R:
        case KEY_F5:
            wifi_do_scan();
            break;
        case KEY_ENTER:
            if (wifi_ap_count_ > 0)
                wifi_try_connect(wifi_sel_);
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    void wifi_try_connect(int idx)
    {
        if (idx < 0 || idx >= wifi_ap_count_) return;
        hal_wifi_ap_t *ap = &wifi_aps_[idx];
        if (ap->in_use) return; // already connected

        if (strcmp(ap->security, "Open") == 0 || ap->security[0] == 0) {
            hal_wifi_connect(ap->ssid, NULL);
            wifi_do_scan();
            wifi_refresh_status();
        } else {
            wifi_pw_ssid_ = ap->ssid;
            wifi_pw_buf_.clear();
            show_wifi_pw_input();
        }
    }

    // ---- WiFi password input ----
    void show_wifi_pw_input()
    {
        view_state_ = ViewState::WIFI_PW;
        lv_obj_t *content = ui_obj_.count("sub_content") ? ui_obj_["sub_content"] : nullptr;
        if (!content) return;
        lv_obj_clean(content);

        char title[128];
        snprintf(title, sizeof(title), "Connect to: %s", wifi_pw_ssid_.c_str());
        make_label(content, title, 0, 2, 0x58A6FF);
        make_label(content, "Password:", 0, 24, 0xE6EDF3);

        pw_input_lbl_ = make_label(content, "_", 80, 24, 0xFFFFFF, &lv_font_montserrat_14);
        lv_obj_set_width(pw_input_lbl_, 200);
        lv_label_set_long_mode(pw_input_lbl_, LV_LABEL_LONG_CLIP);

        pw_hint_lbl_ = make_label(content, "Type password, ENTER to connect, ESC to cancel", 0, 50, 0x555555, &lv_font_montserrat_10);
    }

    void wifi_pw_update_display()
    {
        if (!pw_input_lbl_) return;
        std::string display = wifi_pw_buf_ + "_";
        lv_label_set_text(pw_input_lbl_, display.c_str());
    }

    void handle_wifi_pw_key(uint32_t key)
    {
        if (key == KEY_ESC) {
            view_state_ = ViewState::SUB;
            lv_obj_t *content = ui_obj_.count("sub_content") ? ui_obj_["sub_content"] : nullptr;
            if (content) {
                lv_obj_clean(content);
                build_wifi_page(content);
            }
            return;
        }
        if (key == KEY_ENTER) {
            if (pw_hint_lbl_)
                lv_label_set_text(pw_hint_lbl_, "Connecting...");
            int ret = hal_wifi_connect(wifi_pw_ssid_.c_str(), wifi_pw_buf_.c_str());
            view_state_ = ViewState::SUB;
            lv_obj_t *content = ui_obj_.count("sub_content") ? ui_obj_["sub_content"] : nullptr;
            if (content) {
                lv_obj_clean(content);
                build_wifi_page(content);
            }
            if (ret == 0)
                wifi_refresh_status();
            return;
        }
        if (key == KEY_BACKSPACE) {
            if (!wifi_pw_buf_.empty())
                wifi_pw_buf_.pop_back();
            wifi_pw_update_display();
            return;
        }
        if (cur_elm_ && cur_elm_->utf8[0]) {
            wifi_pw_buf_ += cur_elm_->utf8;
            wifi_pw_update_display();
        }
    }

    void wifi_refresh_status()
    {
        if (!wifi_status_lbl_) return;
        hal_wifi_status_t st = hal_wifi_get_status();
        char buf[128];
        if (st.connected)
            snprintf(buf, sizeof(buf), "%s  %s  %d%%  %s",
                     LV_SYMBOL_WIFI, st.ssid, st.signal, st.ip);
        else
            snprintf(buf, sizeof(buf), "%s  Disconnected", LV_SYMBOL_WIFI);
        lv_label_set_text(wifi_status_lbl_, buf);
    }

    // ==================== Bluetooth sub-page ====================
    void build_bt_page(lv_obj_t *c)
    {
        hal_bt_status_t st = hal_bt_get_status();
        bt_powered_ = st.powered;

        char buf[128];
        snprintf(buf, sizeof(buf), "%s  Bluetooth: %s",
                 LV_SYMBOL_BLUETOOTH, bt_powered_ ? "ON" : "OFF");
        bt_status_lbl_ = make_label(c, buf, 0, 8, bt_powered_ ? 0x58A6FF : 0xADD8E6, &lv_font_montserrat_14);

        char addr_buf[64];
        snprintf(addr_buf, sizeof(addr_buf), "Address: %s", st.address);
        make_label(c, addr_buf, 0, 34, 0x888888);

        make_label(c, "Press ENTER to toggle", 0, 58, 0x555555);
    }

    void handle_bt_key(uint32_t key)
    {
        switch (key) {
        case KEY_ENTER:
            bt_powered_ = !bt_powered_;
            hal_bt_set_power(bt_powered_);
            if (bt_status_lbl_) {
                char buf[128];
                snprintf(buf, sizeof(buf), "%s  Bluetooth: %s",
                         LV_SYMBOL_BLUETOOTH, bt_powered_ ? "ON" : "OFF");
                lv_label_set_text(bt_status_lbl_, buf);
                lv_obj_set_style_text_color(bt_status_lbl_,
                    lv_color_hex(bt_powered_ ? 0x58A6FF : 0xADD8E6),
                    LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    // ==================== Display sub-page ====================
    void build_display_page(lv_obj_t *c)
    {
        bright_val_ = hal_backlight_read();
        if (bright_val_ < 0) bright_val_ = 75;
        int mx = hal_backlight_max();

        make_label(c, "Brightness", 0, 4, 0xE6EDF3);
        bright_bar_ = make_bar(c, 0, 24, 220, 12, bright_val_, mx, 0x1F6FEB);

        char buf[32];
        snprintf(buf, sizeof(buf), "%d%%", bright_val_ * 100 / mx);
        bright_lbl_ = make_label(c, buf, 230, 21, 0xFFFFFF);

        make_label(c, "LEFT/RIGHT: adjust    ESC: back", 0, 50, 0x555555, &lv_font_montserrat_10);
    }

    void handle_display_key(uint32_t key)
    {
        int mx = hal_backlight_max();
        int step = mx / 20;  // 5% steps
        if (step < 1) step = 1;
        switch (key) {
        case KEY_LEFT:
            bright_val_ -= step;
            if (bright_val_ < 1) bright_val_ = 1;
            hal_backlight_write(bright_val_);
            update_bright_ui();
            break;
        case KEY_RIGHT:
            bright_val_ += step;
            if (bright_val_ > mx) bright_val_ = mx;
            hal_backlight_write(bright_val_);
            update_bright_ui();
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    void update_bright_ui()
    {
        int mx = hal_backlight_max();
        if (bright_bar_)
            lv_bar_set_value(bright_bar_, bright_val_, LV_ANIM_ON);
        if (bright_lbl_) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d%%", bright_val_ * 100 / mx);
            lv_label_set_text(bright_lbl_, buf);
        }
    }

    // ==================== Sound sub-page ====================
    void build_sound_page(lv_obj_t *c)
    {
        vol_val_ = hal_volume_read();
        if (vol_val_ < 0) vol_val_ = 39;
        vol_muted_ = (vol_val_ == 0) ? 1 : 0;

        make_label(c, "Volume", 0, 4, 0xE6EDF3);
        vol_bar_ = make_bar(c, 0, 24, 220, 12, vol_val_, 63, 0x2ECC71);

        char buf[32];
        snprintf(buf, sizeof(buf), "%d/%d", vol_val_, 63);
        vol_lbl_ = make_label(c, buf, 230, 21, 0xFFFFFF);

        char mute_buf[32];
        snprintf(mute_buf, sizeof(mute_buf), "Mute: %s", vol_muted_ ? "ON" : "OFF");
        mute_lbl_ = make_label(c, mute_buf, 0, 50, vol_muted_ ? 0xE74C3C : 0xE6EDF3);

        make_label(c, "LEFT/RIGHT: volume  ENTER: mute  ESC: back", 0, 74, 0x555555, &lv_font_montserrat_10);
    }

    void handle_sound_key(uint32_t key)
    {
        switch (key) {
        case KEY_LEFT:
            vol_val_ -= 3;
            if (vol_val_ < 0) vol_val_ = 0;
            hal_volume_write(vol_val_);
            vol_muted_ = (vol_val_ == 0) ? 1 : 0;
            update_vol_ui();
            break;
        case KEY_RIGHT:
            vol_val_ += 3;
            if (vol_val_ > 63) vol_val_ = 63;
            hal_volume_write(vol_val_);
            vol_muted_ = 0;
            update_vol_ui();
            break;
        case KEY_ENTER:
            if (vol_muted_) {
                vol_val_ = vol_pre_mute_;
                vol_muted_ = 0;
            } else {
                vol_pre_mute_ = vol_val_;
                vol_val_ = 0;
                vol_muted_ = 1;
            }
            hal_volume_write(vol_val_);
            update_vol_ui();
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    void update_vol_ui()
    {
        if (vol_bar_)
            lv_bar_set_value(vol_bar_, vol_val_, LV_ANIM_ON);
        if (vol_lbl_) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d/%d", vol_val_, 63);
            lv_label_set_text(vol_lbl_, buf);
        }
        if (mute_lbl_) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Mute: %s", vol_muted_ ? "ON" : "OFF");
            lv_label_set_text(mute_lbl_, buf);
            lv_obj_set_style_text_color(mute_lbl_,
                lv_color_hex(vol_muted_ ? 0xE74C3C : 0xE6EDF3),
                LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    // ==================== Power sub-page ====================
    void build_power_page(lv_obj_t *c)
    {
        power_in_calib_ = false;
        stop_power_timer();

        pwr_batt_lbl_ = make_metric_card(c, 0,   0,  "电量", LV_SYMBOL_BATTERY_FULL, 0x3FB950);
        pwr_volt_lbl_ = make_metric_card(c, 152, 0,  "电压", LV_SYMBOL_CHARGE,       0x58A6FF);
        pwr_curr_lbl_ = make_metric_card(c, 0,   42, "电流", LV_SYMBOL_REFRESH,      0xD29922);
        pwr_temp_lbl_ = make_metric_card(c, 152, 42, "温度", LV_SYMBOL_WARNING,      0xF85149);

        lv_obj_t *status_card = make_card(c, 0, 84, 294, 18, 0x0F1722, 0x1F6FEB);
        make_label(status_card, LV_SYMBOL_OK, 7, 1, 0x58A6FF, &lv_font_montserrat_10);
        pwr_cap_lbl_ = make_label(status_card, "状态: --", 26, 0, 0xC9D1D9, g_font_cn_12 ? g_font_cn_12 : &lv_font_montserrat_12);
        lv_obj_set_width(pwr_cap_lbl_, 258);
        lv_label_set_long_mode(pwr_cap_lbl_, LV_LABEL_LONG_CLIP);

        pwr_calib_row_ = lv_obj_create(c);
        lv_obj_set_size(pwr_calib_row_, 294, 18);
        lv_obj_set_pos(pwr_calib_row_, 0, 108);
        lv_obj_set_style_radius(pwr_calib_row_, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(pwr_calib_row_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(pwr_calib_row_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(pwr_calib_row_, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(pwr_calib_row_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(pwr_calib_row_, LV_OBJ_FLAG_SCROLLABLE);
        make_label(pwr_calib_row_, LV_SYMBOL_RIGHT " Battery Calib", 5, 1, 0xFFFFFF, &lv_font_montserrat_10);

        pwr_hint_lbl_ = make_label(c, "Auto refresh 1s   ENTER: calib   ESC: back", 0, 130, 0x6E7681, &lv_font_montserrat_10);
        refresh_power_page();
        pwr_timer_ = lv_timer_create(UISetupPage::power_timer_cb, 1000, this);
    }

    static void power_timer_cb(lv_timer_t *timer)
    {
        UISetupPage *self = static_cast<UISetupPage *>(lv_timer_get_user_data(timer));
        if (self && self->view_state_ == ViewState::SUB && !self->power_in_calib_)
            self->refresh_power_page();
    }

    void stop_power_timer()
    {
        if (pwr_timer_) {
            lv_timer_delete(pwr_timer_);
            pwr_timer_ = nullptr;
        }
    }

    void handle_power_key(uint32_t key)
    {
        if (power_in_calib_) {
            if (key == KEY_ESC) {
                lv_obj_t *content = ui_obj_.count("sub_content") ? ui_obj_["sub_content"] : nullptr;
                if (content) {
                    lv_obj_clean(content);
                    build_power_page(content);
                }
                return;
            }
            handle_bqcal_key(key);
            return;
        }
        switch (key) {
        case KEY_ENTER:
            open_power_calib_page();
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    // ==================== Shutdown / Reboot confirm sub-pages ====================
    void build_poweraction_page(lv_obj_t *c, bool is_reboot)
    {
        const char *verb = is_reboot ? "Reboot" : "Shutdown";
        uint32_t accent  = is_reboot ? 0xD29922 : 0xF85149;

        lv_obj_t *icon = make_label(c, is_reboot ? LV_SYMBOL_REFRESH : LV_SYMBOL_POWER,
                                    136, 10, accent, &lv_font_montserrat_14);
        (void)icon;

        char line[64];
        snprintf(line, sizeof(line), "%s this device?", verb);
        lv_obj_t *prompt = make_label(c, line, 0, 40, 0xE6EDF3, &lv_font_montserrat_14);
        lv_obj_set_width(prompt, 296);
        lv_obj_set_style_text_align(prompt, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

        make_label(c, "ENTER: confirm    ESC: cancel",
                   0, 96, 0x6E7681, &lv_font_montserrat_10);
    }

    void handle_poweraction_key(uint32_t key, bool is_reboot)
    {
        switch (key) {
        case KEY_ENTER:
            if (is_reboot) hal_system_reboot();
            else           hal_system_shutdown();
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    void refresh_power_page()
    {
        std::string bq_path = bqmon_find_power_supply();
        long capacity = 0, voltage_uv = 0, current_raw = 0, temp_raw = 0;
        char status[64] = "Unknown";
        bool ok = !bq_path.empty() &&
                  bqmon_read_long(bq_path + "/capacity", capacity) &&
                  bqmon_read_long(bq_path + "/voltage_now", voltage_uv) &&
                  bqmon_read_long(bq_path + "/current_now", current_raw) &&
                  bqmon_read_long(bq_path + "/temp", temp_raw);
        if (!bq_path.empty()) bqmon_read_string(bq_path + "/status", status, sizeof(status));

        char buf[96];
        if (pwr_batt_lbl_) {
            if (ok) snprintf(buf, sizeof(buf), "%ld%%", capacity);
            else snprintf(buf, sizeof(buf), "--");
            lv_label_set_text(pwr_batt_lbl_, buf);
        }
        if (pwr_volt_lbl_) {
            if (ok) snprintf(buf, sizeof(buf), "%.2f V", voltage_uv / 1000000.0);
            else snprintf(buf, sizeof(buf), "--");
            lv_label_set_text(pwr_volt_lbl_, buf);
        }
        if (pwr_curr_lbl_) {
            if (ok) snprintf(buf, sizeof(buf), "%.2f mA", bqmon_current_ma(current_raw));
            else snprintf(buf, sizeof(buf), "--");
            lv_label_set_text(pwr_curr_lbl_, buf);
        }
        if (pwr_temp_lbl_) {
            if (ok) snprintf(buf, sizeof(buf), "%.2f C", bqmon_temp_c(temp_raw));
            else snprintf(buf, sizeof(buf), "--");
            lv_label_set_text(pwr_temp_lbl_, buf);
        }
        if (pwr_cap_lbl_) {
            snprintf(buf, sizeof(buf), "状态: %s", ok ? status : "power_supply not found");
            lv_label_set_text(pwr_cap_lbl_, buf);
        }
    }

    void open_power_calib_page()
    {
        stop_power_timer();
        power_in_calib_ = true;
        lv_obj_t *content = ui_obj_.count("sub_content") ? ui_obj_["sub_content"] : nullptr;
        if (!content) return;
        lv_obj_clean(content);
        build_bqcal_page(content);
    }

    // ==================== BQ27220 battery monitor sub-page ====================
    void build_bqmon_page(lv_obj_t *c)
    {
        bqmon_line_lbl_ = make_label(c, "", 0, 4, 0x58A6FF, &lv_font_montserrat_12);
        lv_obj_set_width(bqmon_line_lbl_, 292);
        lv_label_set_long_mode(bqmon_line_lbl_, LV_LABEL_LONG_WRAP);

        bqmon_path_lbl_ = make_label(c, "", 0, 50, 0x888888, &lv_font_montserrat_10);
        lv_obj_set_width(bqmon_path_lbl_, 292);
        lv_label_set_long_mode(bqmon_path_lbl_, LV_LABEL_LONG_WRAP);

        bqmon_status_lbl_ = make_label(c, "ENTER: refresh  ESC: back", 0, 104, 0x555555, &lv_font_montserrat_10);
        refresh_bqmon_page();
    }

    void handle_bqmon_key(uint32_t key)
    {
        switch (key) {
        case KEY_ENTER:
        case KEY_R:
        case KEY_F5:
            refresh_bqmon_page();
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    void refresh_bqmon_page()
    {
        std::string bq_path = bqmon_find_power_supply();
        if (bq_path.empty()) {
            if (bqmon_line_lbl_)
                lv_label_set_text(bqmon_line_lbl_, "Battery monitor: power_supply node not found");
            if (bqmon_path_lbl_)
                lv_label_set_text(bqmon_path_lbl_, "Expected: /sys/class/power_supply/* with capacity/voltage_now/current_now/temp/status");
            if (bqmon_status_lbl_)
                lv_label_set_text(bqmon_status_lbl_, "Load bq27220 driver/device-tree first.");
            return;
        }

        long capacity = 0, voltage_uv = 0, current_raw = 0, temp_raw = 0;
        char status[64] = "Unknown";
        bool ok = bqmon_read_long(bq_path + "/capacity", capacity) &&
                  bqmon_read_long(bq_path + "/voltage_now", voltage_uv) &&
                  bqmon_read_long(bq_path + "/current_now", current_raw) &&
                  bqmon_read_long(bq_path + "/temp", temp_raw);
        bqmon_read_string(bq_path + "/status", status, sizeof(status));

        char line[192];
        snprintf(line, sizeof(line), "Power:%ld%% | Volt:%.2fV | Curr:%.2fmA\nTemp:%.2fC | %s",
                 capacity, voltage_uv / 1000000.0, bqmon_current_ma(current_raw),
                 bqmon_temp_c(temp_raw), status);
        if (bqmon_line_lbl_) lv_label_set_text(bqmon_line_lbl_, line);

        char path_buf[192];
        snprintf(path_buf, sizeof(path_buf), "sysfs: %s", bq_path.c_str());
        if (bqmon_path_lbl_) lv_label_set_text(bqmon_path_lbl_, path_buf);
        if (bqmon_status_lbl_)
            lv_label_set_text(bqmon_status_lbl_, ok ? "ENTER/R: refresh  ESC: back" : "Read failed: check bq27220 sysfs files.");
    }

    static bool bqmon_read_long(const std::string &path, long &value)
    {
        FILE *fp = fopen(path.c_str(), "r");
        if (!fp) return false;
        long v = 0;
        int ret = fscanf(fp, "%ld", &v);
        fclose(fp);
        if (ret != 1) return false;
        value = v;
        return true;
    }

    static bool bqmon_read_string(const std::string &path, char *buf, size_t len)
    {
        if (!buf || len == 0) return false;
        FILE *fp = fopen(path.c_str(), "r");
        if (!fp) return false;
        if (!fgets(buf, len, fp)) {
            fclose(fp);
            return false;
        }
        fclose(fp);
        size_t n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = 0;
        return true;
    }

    static double bqmon_current_ma(long current_now)
    {
        /*
         * Linux power_supply normally exports current_now in microamps.
         * Some vendor bq27220 trees/devices expose an already scaled value one
         * step larger; avoid showing impossible tens-of-amps readings on the UI.
         */
        // long abs_v = current_now < 0 ? -current_now : current_now;
        // return -current_now / (abs_v >= 10000000 ? 1000000.0 : 1000.0);
        return -(current_now / 1000.0);
    }

    static double bqmon_temp_c(long temp)
    {
        /* power_supply temp is usually 0.1 C.  If it is outside a plausible
         * battery range, fall back to 0.01 C used by some vendor exports. */
        double c = temp / 10.0;
        if (c > 100.0 || c < -40.0)
            c = temp / 100.0;
        return c;
    }

    static bool bqmon_has_file(const std::string &dir, const char *name)
    {
        std::string path = dir + "/" + name;
        return access(path.c_str(), R_OK) == 0;
    }

    static std::string bqmon_find_power_supply()
    {
        const char *base = "/sys/class/power_supply";
        DIR *dp = opendir(base);
        if (!dp) return "";

        std::string fallback;
        struct dirent *ent = nullptr;
        while ((ent = readdir(dp)) != nullptr) {
            if (ent->d_name[0] == '.') continue;
            std::string dir = std::string(base) + "/" + ent->d_name;
            if (!bqmon_has_file(dir, "capacity") ||
                !bqmon_has_file(dir, "voltage_now") ||
                !bqmon_has_file(dir, "current_now") ||
                !bqmon_has_file(dir, "temp") ||
                !bqmon_has_file(dir, "status"))
                continue;

            if (strstr(ent->d_name, "bq27220") || strstr(ent->d_name, "bq27")) {
                closedir(dp);
                return dir;
            }
            if (fallback.empty()) fallback = dir;
        }
        closedir(dp);
        return fallback;
    }

    // ==================== BQ27220 calibration sub-page ====================
    enum {
        BQCAL_ACT_REFRESH = 0,
        BQCAL_ACT_ENTER_CAL,
        BQCAL_ACT_CC_OFFSET,
        BQCAL_ACT_BOARD_OFFSET,
        BQCAL_ACT_EXIT_CAL,
        BQCAL_ACT_COUNT
    };

    static constexpr const char *BQ27220_I2C_DEV = "/dev/i2c-1";
    static constexpr int BQ27220_I2C_ADDR        = 0x55;
    static constexpr int BQ27220_REG_CONTROL     = 0x00;
    static constexpr int BQ27220_REG_FLAGS       = 0x0E;
    static constexpr int BQ27220_CMD_CC_OFFSET   = 0x000A;
    static constexpr int BQ27220_CMD_BOARD_OFFSET= 0x0009;
    static constexpr int BQ27220_CMD_ENTER_CAL   = 0x0081;
    static constexpr int BQ27220_CMD_EXIT_CAL    = 0x0080;

    void build_bqcal_page(lv_obj_t *c)
    {
        bqcal_sel_ = 0;
        bqcal_info_lbl_ = make_label(c, "", 0, 0, 0x58A6FF, &lv_font_montserrat_12);
        bqcal_status_lbl_ = make_label(c, "ENTER: run  UP/DN: select  ESC: back", 0, 106, 0x555555, &lv_font_montserrat_10);

        const char *actions[BQCAL_ACT_COUNT] = {
            "Refresh battery data",
            "1. Enter CAL mode",
            "2. CC offset calibration",
            "3. Board offset calibration",
            "4. Exit CAL mode / save",
        };
        for (int i = 0; i < BQCAL_ACT_COUNT; ++i) {
            bqcal_rows_[i] = lv_obj_create(c);
            lv_obj_set_size(bqcal_rows_[i], 294, 17);
            lv_obj_set_pos(bqcal_rows_[i], 0, 24 + i * 16);
            lv_obj_set_style_radius(bqcal_rows_[i], 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(bqcal_rows_[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_all(bqcal_rows_[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(bqcal_rows_[i], LV_OBJ_FLAG_SCROLLABLE);
            make_label(bqcal_rows_[i], actions[i], 5, 1, 0xCCCCCC, &lv_font_montserrat_10);
        }
        bqcal_update_rows();
        bqcal_refresh_info("Ready. Keep battery idle before offset calibration.");
    }

    void handle_bqcal_key(uint32_t key)
    {
        switch (key) {
        case KEY_UP:
            if (bqcal_sel_ > 0) { --bqcal_sel_; bqcal_update_rows(); }
            break;
        case KEY_DOWN:
            if (bqcal_sel_ < BQCAL_ACT_COUNT - 1) { ++bqcal_sel_; bqcal_update_rows(); }
            break;
        case KEY_ENTER:
            bqcal_run_selected();
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    void bqcal_update_rows()
    {
        for (int i = 0; i < BQCAL_ACT_COUNT; ++i) {
            if (!bqcal_rows_[i]) continue;
            bool sel = (i == bqcal_sel_);
            lv_obj_set_style_bg_color(bqcal_rows_[i], lv_color_hex(sel ? 0x1F3A5F : 0x161B22),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(bqcal_rows_[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            uint32_t tc = sel ? 0xFFFFFF : 0xCCCCCC;
            uint32_t cnt = lv_obj_get_child_count(bqcal_rows_[i]);
            for (uint32_t n = 0; n < cnt; ++n) {
                lv_obj_t *child = lv_obj_get_child(bqcal_rows_[i], n);
                lv_obj_set_style_text_color(child, lv_color_hex(tc), LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
    }

    void bqcal_refresh_info(const char *status)
    {
        hal_battery_info_t bat = hal_battery_read();
        char buf[160];
        snprintf(buf, sizeof(buf), "BQ27220 %s  %d%% %dmV %dmA F:%04X",
                 bat.valid ? "OK" : "N/A", bat.soc, bat.voltage_mv,
                 bat.avg_current_ma ? bat.avg_current_ma : bat.current_ma, bat.flags);
        if (bqcal_info_lbl_) lv_label_set_text(bqcal_info_lbl_, buf);
        if (bqcal_status_lbl_ && status) lv_label_set_text(bqcal_status_lbl_, status);
    }

    void bqcal_run_selected()
    {
        int ret = 0;
        const char *ok = "Done.";
        switch (bqcal_sel_) {
        case BQCAL_ACT_REFRESH:
            bqcal_refresh_info("Refreshed.");
            return;
        case BQCAL_ACT_ENTER_CAL:
            ret = bq27220_control_cmd(BQ27220_CMD_ENTER_CAL);
            ok = "CAL mode command sent.";
            break;
        case BQCAL_ACT_CC_OFFSET:
            ret = bq27220_control_cmd(BQ27220_CMD_CC_OFFSET);
            ok = "CC offset calibration command sent.";
            break;
        case BQCAL_ACT_BOARD_OFFSET:
            ret = bq27220_control_cmd(BQ27220_CMD_BOARD_OFFSET);
            ok = "Board offset calibration command sent.";
            break;
        case BQCAL_ACT_EXIT_CAL:
            ret = bq27220_control_cmd(BQ27220_CMD_EXIT_CAL);
            ok = "Exit CAL command sent.";
            break;
        default:
            return;
        }
        bqcal_refresh_info(ret == 0 ? ok : "I2C write failed: check /dev/i2c-1 permissions.");
    }

    static int bq27220_control_cmd(int cmd)
    {
#ifdef __linux__
        int fd = open(BQ27220_I2C_DEV, O_RDWR);
        if (fd < 0) return -1;

        unsigned char buf[3];
        struct i2c_msg msg;
        struct i2c_rdwr_ioctl_data data;
        buf[0] = BQ27220_REG_CONTROL;
        buf[1] = cmd & 0xFF;
        buf[2] = (cmd >> 8) & 0xFF;
        msg.addr  = BQ27220_I2C_ADDR;
        msg.flags = 0;
        msg.len   = sizeof(buf);
        msg.buf   = buf;
        data.msgs  = &msg;
        data.nmsgs = 1;

        int ret = ioctl(fd, I2C_RDWR, &data);
        close(fd);
        return ret < 0 ? -1 : 0;
#else
        (void)cmd;
        return -1;
#endif
    }

    // ==================== UI 构建（主菜单） ====================
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
        ui_obj_["bg"] = bg;

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
        lv_label_set_text(lbl_title, LV_SYMBOL_SETTINGS "  Settings");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, "UP/DN:select  ENTER:open  ESC:back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *list_cont = lv_obj_create(bg);
        lv_obj_set_size(list_cont, 320, LIST_H);
        lv_obj_set_pos(list_cont, 0, LIST_Y);
        lv_obj_set_style_radius(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["list_cont"] = list_cont;

        build_menu_rows();
    }

    // ==================== 构建菜单行 ====================
    void build_menu_rows()
    {
        lv_obj_t *list_cont = ui_obj_["list_cont"];
        lv_obj_clean(list_cont);

        int item_count = (int)menu_items_.size();
        int visible = LIST_H / ITEM_H;
        int offset_idx = selected_idx_ - visible / 2;
        if (offset_idx < 0) offset_idx = 0;
        if (offset_idx > item_count - visible) offset_idx = item_count - visible;
        if (offset_idx < 0) offset_idx = 0;

        for (int vi = 0; vi < visible && (vi + offset_idx) < item_count; ++vi) {
            int mi = vi + offset_idx;
            bool is_sel = (mi == selected_idx_);
            create_menu_row(list_cont, vi, mi, is_sel);
        }
    }

    void create_menu_row(lv_obj_t *parent, int visual_row, int menu_idx, bool selected)
    {
        const MenuItem &item = menu_items_[menu_idx];

        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_size(row, 318, ITEM_H - 2);
        lv_obj_set_pos(row, 1, visual_row * ITEM_H + 1);
        lv_obj_set_style_radius(row, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        if (selected) {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_t *sel_bar = lv_obj_create(row);
            lv_obj_set_size(sel_bar, 3, ITEM_H - 6);
            lv_obj_set_pos(sel_bar, 2, 2);
            lv_obj_set_style_radius(sel_bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(sel_bar, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(sel_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(sel_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(sel_bar, LV_OBJ_FLAG_SCROLLABLE);
        } else {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x161B22), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        if (menu_idx < (int)menu_items_.size() - 1) {
            lv_obj_t *div = lv_obj_create(parent);
            lv_obj_set_size(div, 310, 1);
            lv_obj_set_pos(div, 5, (visual_row + 1) * ITEM_H);
            lv_obj_set_style_radius(div, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(div, lv_color_hex(0x21262D), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(div, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(div, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
        }

        lv_obj_t *lbl_icon = lv_label_create(row);
        lv_label_set_text(lbl_icon, item.icon);
        lv_obj_set_pos(lbl_icon, 8, (ITEM_H - 16) / 2 - 1);
        lv_obj_set_style_text_color(lbl_icon,
            selected ? lv_color_hex(0x58A6FF) : lv_color_hex(0x4A7ABF),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_name = lv_label_create(row);
        lv_label_set_text(lbl_name, item.label);
        lv_obj_set_pos(lbl_name, 30, (ITEM_H - 16) / 2 - 1);
        lv_obj_set_width(lbl_name, 240);
        lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_name,
            selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xCCCCCC),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_arrow = lv_label_create(row);
        lv_label_set_text(lbl_arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_pos(lbl_arrow, 298, (ITEM_H - 14) / 2 - 1);
        lv_obj_set_style_text_color(lbl_arrow,
            selected ? lv_color_hex(0x58A6FF) : lv_color_hex(0x3A4A5A),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_arrow, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // ==================== 打开二级页面 ====================
    void open_sub_page(int idx)
    {
        if (idx < 0 || idx >= (int)menu_items_.size()) return;
        view_state_ = ViewState::SUB;

        const MenuItem &item = menu_items_[idx];

        lv_obj_t *panel = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(panel, 320, 150);
        lv_obj_set_pos(panel, 0, 0);
        lv_obj_set_style_radius(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["sub_panel"] = panel;

        lv_obj_t *title_bar = lv_obj_create(panel);
        lv_obj_set_size(title_bar, 320, 22);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title_bar, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

        char title_buf[64];
        snprintf(title_buf, sizeof(title_buf), "%s  %s", item.icon, item.sub_title);
        lv_obj_t *lbl_title = lv_label_create(title_bar);
        lv_label_set_text(lbl_title, title_buf);
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, LV_SYMBOL_LEFT "  ESC: Back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -6);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0xAECBFA), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *content = lv_obj_create(panel);
        lv_obj_set_size(content, 316, 124);
        lv_obj_set_pos(content, 2, 24);
        lv_obj_set_style_radius(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_hor(content, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_ver(content, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_scroll_dir(content, LV_DIR_VER);
        lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_width(content, 3, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(content, lv_color_hex(0x1F6FEB), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(content, 200, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(content, 2, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        ui_obj_["sub_content"] = content;

        if (item.build_sub)
            item.build_sub(content);
    }

    // ==================== 关闭二级页面 ====================
    void close_sub_page()
    {
        wifi_list_cont_  = nullptr;
        wifi_status_lbl_ = nullptr;
        bt_status_lbl_   = nullptr;
        bright_bar_ = nullptr;
        bright_lbl_ = nullptr;
        vol_bar_  = nullptr;
        vol_lbl_  = nullptr;
        mute_lbl_ = nullptr;
        pwr_batt_lbl_ = nullptr;
        pwr_volt_lbl_ = nullptr;
        pwr_curr_lbl_ = nullptr;
        pwr_temp_lbl_ = nullptr;
        pwr_cap_lbl_  = nullptr;
        pwr_hint_lbl_ = nullptr;
        pwr_calib_row_ = nullptr;
        stop_power_timer();
        power_in_calib_ = false;
        bqmon_line_lbl_   = nullptr;
        bqmon_path_lbl_   = nullptr;
        bqmon_status_lbl_ = nullptr;
        bqcal_info_lbl_   = nullptr;
        bqcal_status_lbl_ = nullptr;
        for (int i = 0; i < BQCAL_ACT_COUNT; ++i) bqcal_rows_[i] = nullptr;
        pw_input_lbl_ = nullptr;
        pw_hint_lbl_  = nullptr;

        if (ui_obj_.count("sub_panel") && ui_obj_["sub_panel"]) {
            lv_obj_del(ui_obj_["sub_panel"]);
            ui_obj_["sub_panel"]   = nullptr;
            ui_obj_["sub_content"] = nullptr;
        }
        view_state_ = ViewState::MAIN;
    }

    // ==================== 事件绑定 ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UISetupPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }
    static void static_lvgl_handler(lv_event_t *e)
    {
        UISetupPage *self = static_cast<UISetupPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }
    void event_handler(lv_event_t *e)
    {
        if(IS_KEY_RELEASED(e))
        {
            struct key_item *elm = (struct key_item *)lv_event_get_param(e);
            cur_elm_ = elm;
            uint32_t key = elm->key_code;
            switch (view_state_)
            {
            case ViewState::MAIN:    handle_main_key(fzxc_to_arrow(key)); break;
            case ViewState::SUB:     handle_sub_key(fzxc_to_arrow(key));  break;
            case ViewState::WIFI_PW: handle_sub_key(key);  break;
            }
        }
    }

    // ================================================================
    //  主菜单按键
    // ================================================================
    void handle_main_key(uint32_t key)
    {
        int count = (int)menu_items_.size();
        switch (key)
        {
        case KEY_UP:
            if (selected_idx_ > 0) {
                --selected_idx_;
                build_menu_rows();
            }
            break;

        case KEY_DOWN:
            if (selected_idx_ < count - 1) {
                ++selected_idx_;
                build_menu_rows();
            }
            break;

        case KEY_ENTER:
        case KEY_RIGHT:
            open_sub_page(selected_idx_);
            break;

        case KEY_ESC:
            if (go_back_home) go_back_home();
            break;

        default:
            break;
        }
    }

    // ================================================================
    //  二级页面按键
    // ================================================================
    void handle_sub_key(uint32_t key)
    {
        const MenuItem &item = menu_items_[selected_idx_];
        if (item.on_sub_key) {
            item.on_sub_key(key);
            return;
        }

        lv_obj_t *content = ui_obj_.count("sub_content") ? ui_obj_["sub_content"] : nullptr;
        switch (key)
        {
        case KEY_UP:
            if (content)
                lv_obj_scroll_by(content, 0, -20, LV_ANIM_ON);
            break;

        case KEY_DOWN:
            if (content)
                lv_obj_scroll_by(content, 0, 20, LV_ANIM_ON);
            break;

        case KEY_ESC:
            close_sub_page();
            break;

        default:
            break;
        }
    }
};
