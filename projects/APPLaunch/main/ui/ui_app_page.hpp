/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <unordered_map>
#include <list>
#include <memory>
#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <vector>
#include <utility>
#include <sstream>
#include <keyboard_input.h>
#include <compat/input_keys.h>
#include <functional>
#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_file.hpp"
#define APP_CONSOLE_EXIT_EVENT (lv_event_code_t)(LV_EVENT_LAST + 1)

namespace launcher_wifi {

inline std::vector<std::string> split_colon(const std::string &line)
{
    std::vector<std::string> cols;
    std::string current;
    for (char ch : line) {
        if (ch == ':') {
            cols.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    cols.push_back(current);
    return cols;
}

inline void copy_string(char *dst, size_t dst_size, const std::string &src)
{
    if (!dst || dst_size == 0)
        return;
    std::snprintf(dst, dst_size, "%s", src.c_str());
}

inline cp0_wifi_status_t get_status()
{
    cp0_wifi_status_t st{};
    cp0_signal_wifi_api({"Status"}, [&](int code, std::string data) {
        if (code != 0)
            return;
        auto cols = split_colon(data);
        if (cols.size() < 4)
            return;
        st.connected = std::atoi(cols[0].c_str());
        copy_string(st.ssid, sizeof(st.ssid), cols[1]);
        copy_string(st.ip, sizeof(st.ip), cols[2]);
        st.signal = std::atoi(cols[3].c_str());
        if (cols.size() >= 5)
            st.ethernet = std::atoi(cols[4].c_str());
    });
    return st;
}

inline int scan(cp0_wifi_ap_t *out, int max_aps)
{
    int count = 0;
    cp0_signal_wifi_api({"Scan", std::to_string(max_aps)}, [&](int code, std::string data) {
        if (!out || max_aps <= 0) {
            count = code;
            return;
        }
        std::istringstream lines(data);
        std::string line;
        while (count < max_aps && std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto cols = split_colon(line);
            if (cols.size() < 4 || cols[0].empty())
                continue;
            cp0_wifi_ap_t ap{};
            copy_string(ap.ssid, sizeof(ap.ssid), cols[0]);
            ap.signal = std::atoi(cols[1].c_str());
            copy_string(ap.security, sizeof(ap.security), cols[2]);
            ap.in_use = std::atoi(cols[3].c_str());
            out[count++] = ap;
        }
    });
    return count;
}

inline int simple(const std::list<std::string> &args)
{
    int result = -1;
    cp0_signal_wifi_api(args, [&](int code, std::string) { result = code; });
    return result;
}

inline int connect(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0])
        return -1;
    if (password && password[0])
        return simple({"Connect", ssid, password});
    return simple({"Connect", ssid});
}

inline int profile_forget(const char *ssid)
{
    return (!ssid || !ssid[0]) ? -1 : simple({"ProfileForget", ssid});
}

inline int profile_exists(const char *ssid)
{
    return (!ssid || !ssid[0]) ? 0 : simple({"ProfileExists", ssid});
}

inline int profile_disconnect_active()
{
    return simple({"ProfileDisconnectActive"});
}

inline int radio_enabled()
{
    return simple({"RadioEnabled"});
}

inline int radio_set_enabled(bool enabled)
{
    return simple({"RadioSetEnabled", enabled ? "on" : "off"});
}

} // namespace launcher_wifi

namespace launcher_battery_ui {

constexpr int kLowBatteryBlinkThreshold = 3;
constexpr uint32_t kLowBatteryBlinkMs = 450;

inline bool should_blink(int soc)
{
    return soc < kLowBatteryBlinkThreshold;
}

inline void blink_opa_exec_cb(void *obj, int32_t opa)
{
    lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), static_cast<lv_opa_t>(opa),
                         LV_PART_MAIN | LV_STATE_DEFAULT);
}

inline void set_blink(lv_obj_t *battery_panel, bool enabled, bool &active)
{
    if (!battery_panel || enabled == active)
        return;

    active = enabled;
    lv_anim_del(battery_panel, blink_opa_exec_cb);
    if (enabled) {
        lv_obj_set_style_opa(battery_panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, battery_panel);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_20);
        lv_anim_set_time(&a, kLowBatteryBlinkMs);
        lv_anim_set_playback_time(&a, kLowBatteryBlinkMs);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_linear);
        lv_anim_set_exec_cb(&a, blink_opa_exec_cb);
        lv_anim_start(&a);
    } else {
        lv_obj_set_style_opa(battery_panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

} // namespace launcher_battery_ui

class UIAppTopBar
{
public:
    UIAppTopBar() = default;
    explicit UIAppTopBar(std::string title) : title_(std::move(title)) {}

    lv_obj_t *create(lv_obj_t *parent)
    {
        return create(parent, title_);
    }

    void set_height(int height)
    {
        height_ = height;
        if (ui_TOP_Container)
            lv_obj_set_height(ui_TOP_Container, height_);
        if (title_label_)
            lv_obj_set_height(title_label_, height_);
    }

    lv_obj_t *create(lv_obj_t *parent, const std::string &title)
    {
        title_ = title;
        ui_TOP_Container = lv_obj_create(parent);
        lv_obj_remove_style_all(ui_TOP_Container);
        lv_obj_set_size(ui_TOP_Container, 320, height_);
        lv_obj_set_pos(ui_TOP_Container, 0, 0);
        lv_obj_clear_flag(ui_TOP_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_set_flex_flow(ui_TOP_Container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(ui_TOP_Container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(ui_TOP_Container, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_TOP_Container, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_TOP_Container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_TOP_Container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_column(ui_TOP_Container, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

        create_title(ui_TOP_Container);
        create_spacer(ui_TOP_Container);
        create_ethernet(ui_TOP_Container);
        create_wifi(ui_TOP_Container);
        create_time(ui_TOP_Container);
        create_battery(ui_TOP_Container);
        return ui_TOP_Container;
    }

    void set_title(const std::string &title)
    {
        title_ = title;
        if (title_label_)
            lv_label_set_text(title_label_, title.c_str());
    }

    void update_time()
    {
        if (!time_label_)
            return;
        char time_buf[16];
        cp0_time_str(time_buf, sizeof(time_buf));
        lv_label_set_text(time_label_, time_buf);
    }

    void update_wifi()
    {
        cp0_wifi_status_t ws = launcher_wifi::get_status();
        set_wifi_signal(ws.connected ? ws.signal : 0);
        if (eth_icon_)
        {
            if (ws.ethernet)
                lv_obj_clear_flag(eth_icon_, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(eth_icon_, LV_OBJ_FLAG_HIDDEN);
        }
        if (!wifi_panel_)
            return;
        if (ws.connected)
            lv_obj_clear_flag(wifi_panel_, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(wifi_panel_, LV_OBJ_FLAG_HIDDEN);
    }

    void update_battery(const cp0_battery_info_t &bat)
    {
        if (!bat.valid || !power_label_)
            return;
        int soc = bat.soc;
        if (soc > 100)
            soc = 100;
        if (soc < 0)
            soc = 0;

        char pwr_buf[16];
        snprintf(pwr_buf, sizeof(pwr_buf), "%d%%", soc);
        lv_label_set_text(power_label_, pwr_buf);
        if (battery_bar_)
            lv_bar_set_value(battery_bar_, soc, LV_ANIM_OFF);
        set_battery_charging((bat.flags & 1) != 0);
        set_battery_low_blink(launcher_battery_ui::should_blink(soc));
    }

    void update_status()
    {
        update_time();
        update_wifi();
    }

private:
    std::string title_ = "APP";
    lv_obj_t *ui_TOP_Container = nullptr;
    lv_obj_t *title_label_ = nullptr;
    lv_obj_t *battery_panel_ = nullptr;
    lv_obj_t *battery_bar_ = nullptr;
    lv_obj_t *battery_charge_wave_ = nullptr;
    lv_obj_t *power_label_ = nullptr;
    lv_obj_t *time_panel_ = nullptr;
    lv_obj_t *time_label_ = nullptr;
    lv_obj_t *wifi_panel_ = nullptr;
    lv_obj_t *wifi_bars_[4] = {};
    lv_obj_t *eth_icon_ = nullptr;
    int height_ = 20;
    bool battery_charging_ = false;
    bool battery_low_blink_ = false;

    static lv_font_t *top_title_font()
    {
        return launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD);
    }


    static void clear_status_panel_style(lv_obj_t *obj)
    {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(obj, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void create_title(lv_obj_t *parent)
    {
        title_label_ = lv_label_create(parent);
        lv_label_set_long_mode(title_label_, LV_LABEL_LONG_DOT);
        lv_label_set_text(title_label_, title_.c_str());
        lv_obj_set_width(title_label_, 150);
        lv_obj_set_height(title_label_, height_);
        lv_obj_set_style_text_font(title_label_, top_title_font(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(title_label_, lv_color_hex(0xCCAA00), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(title_label_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(title_label_, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void create_spacer(lv_obj_t *parent)
    {
        lv_obj_t *spacer = lv_obj_create(parent);
        lv_obj_remove_style_all(spacer);
        lv_obj_set_size(spacer, 0, height_);
        lv_obj_set_flex_grow(spacer, 1);
        lv_obj_clear_flag(spacer, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    }

    void create_ethernet(lv_obj_t *parent)
    {
        eth_icon_ = lv_img_create(parent);
        lv_img_set_src(eth_icon_, cp0_file_path_c("status_ethernet.png"));
        lv_obj_clear_flag(eth_icon_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_add_flag(eth_icon_, LV_OBJ_FLAG_HIDDEN);
    }

    void create_wifi(lv_obj_t *parent)
    {
        static const int bar_heights[4] = {6, 9, 12, 15};

        wifi_panel_ = lv_obj_create(parent);
        lv_obj_set_size(wifi_panel_, 22, 15);
        clear_status_panel_style(wifi_panel_);
        lv_obj_set_flex_flow(wifi_panel_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(wifi_panel_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_column(wifi_panel_, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(wifi_panel_, LV_OBJ_FLAG_HIDDEN);

        for (int i = 0; i < 4; ++i)
        {
            wifi_bars_[i] = lv_obj_create(wifi_panel_);
            lv_obj_set_size(wifi_bars_[i], 4, bar_heights[i]);
            lv_obj_clear_flag(wifi_bars_[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(wifi_bars_[i], 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(wifi_bars_[i], lv_color_hex(0x4D4D4D), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(wifi_bars_[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(wifi_bars_[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void create_time(lv_obj_t *parent)
    {
        time_panel_ = lv_obj_create(parent);
        lv_obj_set_size(time_panel_, 40, 16);
        clear_status_panel_style(time_panel_);
        lv_obj_set_style_bg_img_src(time_panel_, cp0_file_path_c("status_time_background.png"), LV_PART_MAIN | LV_STATE_DEFAULT);

        time_label_ = lv_label_create(time_panel_);
        lv_obj_set_align(time_label_, LV_ALIGN_CENTER);
        lv_label_set_text(time_label_, "15:21");
        lv_obj_set_style_text_color(time_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(time_label_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void create_battery(lv_obj_t *parent)
    {
        battery_panel_ = lv_obj_create(parent);
        lv_obj_set_size(battery_panel_, 36, 16);
        clear_status_panel_style(battery_panel_);
        lv_obj_set_style_bg_img_src(battery_panel_, cp0_file_path_c("status_battery_background.png"), LV_PART_MAIN | LV_STATE_DEFAULT);

        battery_bar_ = lv_bar_create(battery_panel_);
        lv_bar_set_value(battery_bar_, 96, LV_ANIM_OFF);
        lv_bar_set_start_value(battery_bar_, 0, LV_ANIM_OFF);
        lv_obj_set_size(battery_bar_, 33, 14);
        lv_obj_set_align(battery_bar_, LV_ALIGN_CENTER);
        lv_obj_set_style_radius(battery_bar_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(battery_bar_, lv_color_hex(0x484847), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(battery_bar_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(battery_bar_, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(battery_bar_, lv_color_hex(0x66CC33), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(battery_bar_, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

        battery_charge_wave_ = lv_obj_create(battery_panel_);
        lv_obj_set_size(battery_charge_wave_, 8, 14);
        lv_obj_set_pos(battery_charge_wave_, -8, 1);
        lv_obj_clear_flag(battery_charge_wave_, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_set_style_radius(battery_charge_wave_, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(battery_charge_wave_, lv_color_hex(0xFFF176), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(battery_charge_wave_, 190, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(battery_charge_wave_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(battery_charge_wave_, LV_OBJ_FLAG_HIDDEN);

        power_label_ = lv_label_create(battery_panel_);
        lv_obj_set_align(power_label_, LV_ALIGN_CENTER);
        lv_label_set_text(power_label_, "--");
        lv_obj_set_style_text_color(power_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(power_label_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void set_battery_low_blink(bool enabled)
    {
        launcher_battery_ui::set_blink(battery_panel_, enabled, battery_low_blink_);
    }

    void set_battery_charging(bool charging)
    {
        if (!battery_panel_ || !battery_bar_ || !power_label_ || !battery_charge_wave_)
            return;
        // No progress fill: the battery level is shown by the rounded background
        // image + "%" label only. Keep the bar fully transparent in all states.
        lv_obj_set_style_bg_opa(battery_panel_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(battery_bar_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(battery_bar_, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(power_label_, lv_color_hex(charging ? 0xFFF2A8 : 0xFFFFFF),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);

        if (charging == battery_charging_)
            return;
        battery_charging_ = charging;
        if (charging) {
            lv_obj_clear_flag(battery_charge_wave_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(power_label_);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, battery_charge_wave_);
            lv_anim_set_values(&a, -8, 36);
            lv_anim_set_time(&a, 850);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_path_cb(&a, lv_anim_path_linear);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
            lv_anim_start(&a);
        } else {
            lv_anim_del(battery_charge_wave_, nullptr);
            lv_obj_set_x(battery_charge_wave_, -8);
            lv_obj_add_flag(battery_charge_wave_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void set_wifi_signal(int signal)
    {
        static const int thresholds[4] = {1, 30, 60, 80};
        const uint32_t on_color = 0x33CC33;
        const uint32_t off_color = 0x4D4D4D;

        for (int i = 0; i < 4; ++i)
        {
            if (!wifi_bars_[i])
                continue;
            lv_obj_set_style_bg_color(wifi_bars_[i],
                                      lv_color_hex(signal >= thresholds[i] ? on_color : off_color),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
};

class UIAppContainer
{
public:
    UIAppContainer() = default;
    explicit UIAppContainer(int height) : height_(height) {}

    void set_height(int height)
    {
        height_ = height;
        if (ui_APP_Container)
            lv_obj_set_height(ui_APP_Container, height_);
    }

    lv_obj_t *create(lv_obj_t *parent)
    {
        ui_APP_Container = lv_obj_create(parent);
        lv_obj_remove_style_all(ui_APP_Container);
        lv_obj_set_width(ui_APP_Container, 320);
        lv_obj_set_height(ui_APP_Container, height_);
        lv_obj_set_pos(ui_APP_Container, 0, 20);
        lv_obj_clear_flag(ui_APP_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        return ui_APP_Container;
    }

    lv_obj_t *get() const
    {
        return ui_APP_Container;
    }

private:
    int height_ = 150;
    lv_obj_t *ui_APP_Container = nullptr;
};


class AppPageRoot
{

public:
    std::string page_title_ = "APP";
    lv_group_t *input_group_ = nullptr;
    lv_obj_t *root_screen_ = nullptr;
    lv_obj_t *screen() { return root_screen_; }
    lv_group_t *input_group() { return input_group_; }
    std::function<void(void)> navigate_home;
    bool has_bottom_bar_ = false;
    int top_bar_height_px_ = 20;
public:
    AppPageRoot()
    {
        creat_base_UI();
        creat_input_group();
    }
    virtual ~AppPageRoot()
    {
        if (root_screen_)
            lv_obj_del(root_screen_);
        if (input_group_)
            lv_group_delete(input_group_);
    }

    template <typename Component>
    lv_obj_t *add_bar(Component &&component)
    {
        return component.create(root_screen_);
    }

private:
    /* ================================================================== */
    /*  UI construction                                                             */
    /* ================================================================== */
    void creat_base_UI()
    {
        root_screen_ = lv_obj_create(NULL);
        lv_obj_clear_flag(root_screen_, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_pad_all(root_screen_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(root_screen_, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(root_screen_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void creat_input_group()
    {
        input_group_ = lv_group_create();
        lv_group_add_obj(input_group_, root_screen_);
    }
};

class AppTopBarRegion : virtual public AppPageRoot
{
public:
    AppTopBarRegion()
    {
        top_bar_.set_title(page_title_);
        top_bar_.set_height(top_bar_height_px_);
        add_bar(top_bar_);
        UI_bind_event();
        update_datetime_status();
        update_status_bar();
        update_battery_status(cp0_battery_read());
        time_timer_ = lv_timer_create(app_time_timer_cb, 1000, this);
        status_timer_ = lv_timer_create(app_status_timer_cb, 5000, this);
    }

    virtual ~AppTopBarRegion()
    {
        if (time_timer_)
            lv_timer_delete(time_timer_);
        if (status_timer_)
            lv_timer_delete(status_timer_);
    }

    void update_status_bar()
    {
        top_bar_.update_wifi();
    }

    void update_datetime_status()
    {
        top_bar_.update_time();
    }

    void update_battery_status(const cp0_battery_info_t &bat)
    {
        top_bar_.update_battery(bat);
    }

    void set_page_title(const std::string &title)
    {
        page_title_ = title;
        top_bar_.set_title(title);
    }

private:
    UIAppTopBar top_bar_;
    lv_timer_t *time_timer_ = nullptr;
    lv_timer_t *status_timer_ = nullptr;

    static void app_battery_event_cb(lv_event_t *e)
    {
        AppTopBarRegion *self = static_cast<AppTopBarRegion *>(lv_event_get_user_data(e));
        if (!self || lv_event_get_code(e) != launcher_ui::events::battery_event())
            return;
        const cp0_battery_info_t *bat = launcher_ui::events::battery_info(e);
        if (bat)
            self->update_battery_status(*bat);
    }

    static void app_status_timer_cb(lv_timer_t *timer)
    {
        AppTopBarRegion *self = static_cast<AppTopBarRegion *>(lv_timer_get_user_data(timer));
        if (self)
            self->update_status_bar();
    }

    static void app_time_timer_cb(lv_timer_t *timer)
    {
        AppTopBarRegion *self = static_cast<AppTopBarRegion *>(lv_timer_get_user_data(timer));
        if (self)
            self->update_datetime_status();
    }

    void UI_bind_event()
    {
        lv_obj_add_event_cb(root_screen_, app_battery_event_cb, launcher_ui::events::battery_event(), this);
    }
};

class AppContentRegion : virtual public AppPageRoot
{
public:
    AppContentRegion()
    {
        refresh();
        ui_APP_Container = add_bar(app_container_);
    }

    void refresh()
    {
        app_container_.set_height(has_bottom_bar_ ? 130 : 150);
    }

    void refash()
    {
        refresh();
    }

    virtual ~AppContentRegion() = default;

    lv_obj_t *ui_APP_Container = nullptr;

private:
    UIAppContainer app_container_;
};

class AppBottomBarRegion : virtual public AppPageRoot, virtual public AppContentRegion
{
public:
    AppBottomBarRegion()
    {
        has_bottom_bar_ = true;
        refresh();

        ui_BOTTOM_Container = lv_obj_create(root_screen_);
        lv_obj_remove_style_all(ui_BOTTOM_Container);
        lv_obj_set_width(ui_BOTTOM_Container, 320);
        lv_obj_set_height(ui_BOTTOM_Container, 20);
        lv_obj_set_pos(ui_BOTTOM_Container, 0, 150);
        lv_obj_clear_flag(ui_BOTTOM_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    }

    virtual ~AppBottomBarRegion() = default;

    lv_obj_t *ui_BOTTOM_Container = nullptr;
};

class AppPageLayout : virtual public AppTopBarRegion, virtual public AppContentRegion
{
public:
    AppPageLayout() : AppPageRoot(), AppTopBarRegion(), AppContentRegion()
    {
    }

    virtual ~AppPageLayout() = default;
};

class AppPageWithBottomBarLayout : virtual public AppTopBarRegion, virtual public AppContentRegion, virtual public AppBottomBarRegion
{
public:
    AppPageWithBottomBarLayout() : AppPageRoot(), AppTopBarRegion(), AppContentRegion(), AppBottomBarRegion()
    {
    }

    virtual ~AppPageWithBottomBarLayout() = default;
};

class home_base : public AppPageRoot
{
private:
    lv_obj_t *ui_TOP_Container = nullptr;
    lv_obj_t *ui_TOP_logo = nullptr;
    lv_obj_t *ui_TOP_wifiPanel = nullptr;
    lv_obj_t *ui_TOP_wifiBars[4] = {};
    lv_obj_t *ui_TOP_eth = nullptr;
    lv_obj_t *ui_TOP_time = nullptr;
    lv_obj_t *ui_TOP_time_Label = nullptr;
    lv_obj_t *ui_TOP_battery_panel = nullptr;
    lv_obj_t *ui_TOP_Power = nullptr;
    lv_obj_t *ui_TOP_charge_wave = nullptr;
    lv_obj_t *ui_TOP_power_Label = nullptr;
    lv_timer_t *time_timer_ = nullptr;
    lv_timer_t *status_timer_ = nullptr;
    bool ui_TOP_charging = false;
    bool ui_TOP_battery_low_blink = false;

public:
    lv_obj_t *ui_APP_Container = nullptr;

public:
    home_base() : AppPageRoot()
    {
        creat_Top_UI();
        UI_bind_event();
        update_time_status();
        update_status_bar();
        time_timer_ = lv_timer_create(home_time_timer_cb, 1000, this);
        status_timer_ = lv_timer_create(home_status_timer_cb, 5000, this);
    }
    ~home_base()
    {
        if (time_timer_)
            lv_timer_delete(time_timer_);
        if (status_timer_)
            lv_timer_delete(status_timer_);
    }

    static void home_battery_event_cb(lv_event_t *e)
    {
        home_base *self = static_cast<home_base *>(lv_event_get_user_data(e));
        if (!self || lv_event_get_code(e) != launcher_ui::events::battery_event())
            return;
        const cp0_battery_info_t *bat = launcher_ui::events::battery_info(e);
        if (bat)
            self->update_battery_status(*bat);
    }

    static void home_status_timer_cb(lv_timer_t *timer)
    {
        home_base *self = static_cast<home_base *>(lv_timer_get_user_data(timer));
        if (self)
            self->update_status_bar();
    }

    static void home_time_timer_cb(lv_timer_t *timer)
    {
        home_base *self = static_cast<home_base *>(lv_timer_get_user_data(timer));
        if (self)
            self->update_time_status();
    }

    void update_status_bar()
    {
        update_wifi_status();
    }

    void use_bold_home_title_font()
    {
#ifndef APPLAUNCH_LOGO_USE_PNG
        if (!ui_TOP_logo)
            return;
        lv_obj_set_style_text_font(ui_TOP_logo,
                                   launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
#endif
    }

    void update_battery_status(const cp0_battery_info_t &bat)
    {
        if (bat.valid)
        {
            int soc = bat.soc;
            if (soc > 100)
                soc = 100;
            if (soc < 0)
                soc = 0;
            lv_bar_set_value(ui_TOP_Power, soc, LV_ANIM_ON);
            char pwr_buf[16];
            snprintf(pwr_buf, sizeof(pwr_buf), "%d%%", soc);
            lv_label_set_text(ui_TOP_power_Label, pwr_buf);
            lv_obj_set_style_text_font(ui_TOP_power_Label, LV_FONT_DEFAULT, LV_PART_MAIN | LV_STATE_DEFAULT);
            const bool charging = (bat.flags & 1) != 0;
            set_home_battery_charging(charging);
            set_home_battery_low_blink(launcher_battery_ui::should_blink(soc));

            uint32_t color = 0x66CC33;
            if (soc <= 20)
                color = 0xE74C3C;
            else if (soc <= 50)
                color = 0xF39C12;
            if (!charging) {
                lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(color),
                                          LV_PART_INDICATOR | LV_STATE_DEFAULT);
            }
        }
    }

    lv_obj_t *content_container() const
    {
        return ui_APP_Container;
    }

    lv_obj_t *top_container() const
    {
        return ui_TOP_Container;
    }

private:
    /* ================================================================== */
    /*  UI construction                                                             */
    /* ================================================================== */
    void creat_Top_UI()
    {
        ui_TOP_Container = lv_obj_create(root_screen_);
        lv_obj_remove_style_all(ui_TOP_Container);
        lv_obj_set_size(ui_TOP_Container, 320, 20);
        lv_obj_set_pos(ui_TOP_Container, 0, 0);
        lv_obj_clear_flag(ui_TOP_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_set_flex_flow(ui_TOP_Container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(ui_TOP_Container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_left(ui_TOP_Container, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_TOP_Container, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_TOP_Container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_TOP_Container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_column(ui_TOP_Container, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

#ifdef APPLAUNCH_LOGO_USE_PNG
        ui_TOP_logo = lv_img_create(ui_TOP_Container);
        lv_img_set_src(ui_TOP_logo, cp0_file_path_c("launcher_brand_logo.png"));
        lv_obj_set_size(ui_TOP_logo, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_add_flag(ui_TOP_logo, LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_clear_flag(ui_TOP_logo, LV_OBJ_FLAG_SCROLLABLE);
#else
        ui_TOP_logo = lv_label_create(ui_TOP_Container);
        lv_label_set_text(ui_TOP_logo, "ZERO");
        lv_obj_set_style_text_font(ui_TOP_logo, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_TOP_logo, lv_color_hex(0xCCAA00), LV_PART_MAIN | LV_STATE_DEFAULT);
#endif

        // Spacer: pushes right-side icons to the far right
        lv_obj_t *spacer = lv_obj_create(ui_TOP_Container);
        lv_obj_remove_style_all(spacer);
        lv_obj_set_size(spacer, 0, 20);
        lv_obj_set_flex_grow(spacer, 1);
        lv_obj_clear_flag(spacer, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

        ui_TOP_eth = lv_img_create(ui_TOP_Container);
        lv_img_set_src(ui_TOP_eth, cp0_file_path_c("status_ethernet.png"));
        lv_obj_clear_flag(ui_TOP_eth, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_add_flag(ui_TOP_eth, LV_OBJ_FLAG_HIDDEN);

        create_wifi_status(ui_TOP_Container);

        ui_TOP_time = lv_obj_create(ui_TOP_Container);
        lv_obj_set_width(ui_TOP_time, 40);
        lv_obj_set_height(ui_TOP_time, 16);
        lv_obj_clear_flag(ui_TOP_time, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_time, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(ui_TOP_time, cp0_file_path_c("status_time_background.png"), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_TOP_time_Label = lv_label_create(ui_TOP_time);
        lv_obj_set_width(ui_TOP_time_Label, LV_SIZE_CONTENT);
        lv_obj_set_height(ui_TOP_time_Label, LV_SIZE_CONTENT);
        lv_obj_set_align(ui_TOP_time_Label, LV_ALIGN_CENTER);
        lv_label_set_text(ui_TOP_time_Label, "15:21");
        lv_obj_set_style_text_color(ui_TOP_time_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_TOP_time_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_battery_panel = lv_obj_create(ui_TOP_Container);
        lv_obj_set_width(ui_TOP_battery_panel, 36);
        lv_obj_set_height(ui_TOP_battery_panel, 16);
        lv_obj_clear_flag(ui_TOP_battery_panel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(ui_TOP_battery_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_battery_panel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_battery_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(ui_TOP_battery_panel, cp0_file_path_c("status_battery_background.png"), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_battery_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(ui_TOP_battery_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_Power = lv_bar_create(ui_TOP_battery_panel);
        lv_bar_set_value(ui_TOP_Power, 96, LV_ANIM_OFF);
        lv_bar_set_start_value(ui_TOP_Power, 0, LV_ANIM_OFF);
        lv_obj_set_width(ui_TOP_Power, 33);
        lv_obj_set_height(ui_TOP_Power, 14);
        lv_obj_set_align(ui_TOP_Power, LV_ALIGN_CENTER);
        lv_obj_set_style_radius(ui_TOP_Power, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(0x484847), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_Power, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_radius(ui_TOP_Power, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(0x66CC33), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_Power, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

        ui_TOP_charge_wave = lv_obj_create(ui_TOP_Power);
        lv_obj_set_width(ui_TOP_charge_wave, 8);
        lv_obj_set_height(ui_TOP_charge_wave, 14);
        lv_obj_set_pos(ui_TOP_charge_wave, -8, 1);
        lv_obj_clear_flag(ui_TOP_charge_wave, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        lv_obj_set_style_radius(ui_TOP_charge_wave, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_charge_wave, lv_color_hex(0xFFF176), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_charge_wave, 190, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_charge_wave, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(ui_TOP_charge_wave, LV_OBJ_FLAG_HIDDEN);

        ui_TOP_power_Label = lv_label_create(ui_TOP_Power);
        lv_obj_set_width(ui_TOP_power_Label, LV_SIZE_CONTENT);
        lv_obj_set_height(ui_TOP_power_Label, LV_SIZE_CONTENT);
        lv_obj_set_align(ui_TOP_power_Label, LV_ALIGN_CENTER);
        lv_label_set_text(ui_TOP_power_Label, "--");
        lv_obj_set_style_text_color(ui_TOP_power_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_TOP_power_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_APP_Container = lv_obj_create(root_screen_);
        lv_obj_remove_style_all(ui_APP_Container);
        lv_obj_set_size(ui_APP_Container, 320, 150);
        lv_obj_set_pos(ui_APP_Container, 0, 20);
        lv_obj_clear_flag(ui_APP_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE)); /// Flags
    }

    void set_home_battery_low_blink(bool enabled)
    {
        launcher_battery_ui::set_blink(ui_TOP_battery_panel, enabled, ui_TOP_battery_low_blink);
    }

    void set_home_battery_charging(bool charging)
    {
        if (!ui_TOP_battery_panel || !ui_TOP_Power || !ui_TOP_power_Label || !ui_TOP_charge_wave)
            return;
        // No progress fill: the battery level is shown by the rounded background
        // image + "%" label only. Keep the bar fully transparent in all states.
        lv_obj_set_style_bg_opa(ui_TOP_battery_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_Power, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_Power, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_TOP_power_Label, lv_color_hex(charging ? 0xFFF2A8 : 0xFFFFFF),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);

        if (charging == ui_TOP_charging)
            return;
        ui_TOP_charging = charging;
        if (charging) {
            lv_obj_clear_flag(ui_TOP_charge_wave, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(ui_TOP_power_Label);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, ui_TOP_charge_wave);
            lv_anim_set_values(&a, -8, 36);
            lv_anim_set_time(&a, 850);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_path_cb(&a, lv_anim_path_linear);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
            lv_anim_start(&a);
        } else {
            lv_anim_del(ui_TOP_charge_wave, nullptr);
            lv_obj_set_x(ui_TOP_charge_wave, -8);
            lv_obj_add_flag(ui_TOP_charge_wave, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void create_wifi_status(lv_obj_t *parent)
    {
        static const int bar_heights[4] = {6, 9, 12, 15};

        ui_TOP_wifiPanel = lv_obj_create(parent);
        lv_obj_set_width(ui_TOP_wifiPanel, 24);
        lv_obj_set_height(ui_TOP_wifiPanel, 15);
        lv_obj_clear_flag(ui_TOP_wifiPanel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(ui_TOP_wifiPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_wifiPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_wifiPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(ui_TOP_wifiPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(ui_TOP_wifiPanel, LV_OBJ_FLAG_HIDDEN);

        for (int i = 0; i < 4; ++i)
        {
            ui_TOP_wifiBars[i] = lv_obj_create(ui_TOP_wifiPanel);
            lv_obj_set_width(ui_TOP_wifiBars[i], 4);
            lv_obj_set_height(ui_TOP_wifiBars[i], bar_heights[i]);
            lv_obj_set_align(ui_TOP_wifiBars[i], LV_ALIGN_BOTTOM_LEFT);
            lv_obj_set_x(ui_TOP_wifiBars[i], i * 6);
            lv_obj_clear_flag(ui_TOP_wifiBars[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(ui_TOP_wifiBars[i], 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(ui_TOP_wifiBars[i], lv_color_hex(0x4D4D4D), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_TOP_wifiBars[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(ui_TOP_wifiBars[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    void update_time_status()
    {
        if (!ui_TOP_time_Label)
            return;

        char time_buf[16];
        cp0_time_str(time_buf, sizeof(time_buf));
        lv_label_set_text(ui_TOP_time_Label, time_buf);
    }

    void update_wifi_status()
    {
        if (!ui_TOP_wifiPanel)
            return;

        cp0_wifi_status_t ws = launcher_wifi::get_status();
        static const int thresholds[4] = {1, 30, 60, 80};

        for (int i = 0; i < 4; ++i)
        {
            if (!ui_TOP_wifiBars[i])
                continue;
            lv_obj_set_style_bg_color(ui_TOP_wifiBars[i],
                                      lv_color_hex(ws.connected && ws.signal >= thresholds[i] ? 0x33CC33 : 0x4D4D4D),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        if (ws.connected)
            lv_obj_clear_flag(ui_TOP_wifiPanel, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(ui_TOP_wifiPanel, LV_OBJ_FLAG_HIDDEN);

        if (ui_TOP_eth)
        {
            if (ws.ethernet)
                lv_obj_clear_flag(ui_TOP_eth, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(ui_TOP_eth, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void UI_bind_event()
    {
        lv_obj_add_event_cb(root_screen_, home_battery_event_cb, launcher_ui::events::battery_event(), this);
    }
};

class AppPage : public AppPageLayout
{
public:
    AppPage() : AppPageLayout()
    {
    }

    ~AppPage() override = default;
};

// ===========================================================================
//  SudoPrompt — floating sudo-password dialog
//
//  Usage:
//      SudoPrompt::show({"apt", "install", "-y", "htop"},
//          [](int code) { /* code==0: success, code==-2: cancelled */ });
//
//  The dialog floats above all screens (lv_layer_top). Keyboard events on
//  the current active object are intercepted by temporarily swapping the
//  indev group. Enter executes the command with the supplied password via
//  `sudo -S`; ESC cancels without running anything.
//  The caller is responsible for ensuring the command args are safe.
// ===========================================================================
class SudoPrompt
{
public:
    using Callback = std::function<void(int /*exit_code*/)>;
    static constexpr int kCancelled = -2;

    static void show(std::vector<std::string> cmd, Callback cb)
    {
        // Only one prompt at a time
        if (s_instance_)
            return;
        s_instance_ = new SudoPrompt(std::move(cmd), std::move(cb));
        s_instance_->build_ui();
        s_instance_->grab_input();
    }

private:
    // ---- state ----
    std::vector<std::string> cmd_;
    Callback                 cb_;
    std::string              pw_buf_;
    bool                     cursor_vis_ = true;

    // ---- LVGL objects (all children of lv_layer_top) ----
    lv_obj_t   *overlay_  = nullptr; // full-screen dim
    lv_obj_t   *box_      = nullptr; // dialog card
    lv_obj_t   *pw_lbl_   = nullptr; // masked password + cursor
    lv_obj_t   *hint_lbl_ = nullptr; // status / hint line
    lv_timer_t *cursor_timer_ = nullptr;
    lv_timer_t *exec_timer_   = nullptr; // one-shot to run cmd after render

    // ---- saved indev group + screen event registration ----
    lv_group_t *saved_group_  = nullptr;
    lv_group_t *prompt_group_ = nullptr;
    lv_obj_t   *key_hook_obj_ = nullptr; // active screen where event is registered

    static SudoPrompt *s_instance_;

    // ---- construction ----
    SudoPrompt(std::vector<std::string> cmd, Callback cb)
        : cmd_(std::move(cmd)), cb_(std::move(cb)) {}

    void build_ui()
    {
        lv_obj_t *layer = lv_layer_top();

        // Semi-transparent full-screen backdrop
        overlay_ = lv_obj_create(layer);
        lv_obj_remove_style_all(overlay_);
        lv_obj_set_size(overlay_, 320, 170);
        lv_obj_set_pos(overlay_, 0, 0);
        lv_obj_set_style_bg_color(overlay_, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(overlay_, LV_OPA_70, 0);
        lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(overlay_);

        // Dialog card — centered, 220×110
        box_ = lv_obj_create(layer);
        lv_obj_remove_style_all(box_);
        lv_obj_set_size(box_, 220, 110);
        lv_obj_align(box_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(box_, lv_color_hex(0x1A1A2E), 0);
        lv_obj_set_style_bg_opa(box_, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(box_, 6, 0);
        lv_obj_set_style_border_color(box_, lv_color_hex(0x3A5A8A), 0);
        lv_obj_set_style_border_width(box_, 1, 0);
        lv_obj_set_style_shadow_width(box_, 12, 0);
        lv_obj_set_style_shadow_color(box_, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(box_, LV_OPA_60, 0);
        lv_obj_clear_flag(box_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(box_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(box_);

        const lv_font_t *font14 = &lv_font_montserrat_14;
        const lv_font_t *font12 = &lv_font_montserrat_12;
        const lv_font_t *font10 = &lv_font_montserrat_10;

        // Title
        lv_obj_t *title = lv_label_create(box_);
        lv_label_set_text(title, "Sudo Password");
        lv_obj_set_pos(title, 0, 8);
        lv_obj_set_width(title, 220);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0x4A9EFF), 0);
        lv_obj_set_style_text_font(title, font14, 0);

        // Command preview (first token, truncated)
        std::string cmd_preview = cmd_.empty() ? "" : cmd_[0];
        if (cmd_preview.size() > 24) cmd_preview = cmd_preview.substr(0, 21) + "...";
        lv_obj_t *cmd_lbl = lv_label_create(box_);
        lv_label_set_text(cmd_lbl, cmd_preview.c_str());
        lv_obj_set_pos(cmd_lbl, 0, 28);
        lv_obj_set_width(cmd_lbl, 220);
        lv_obj_set_style_text_align(cmd_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(cmd_lbl, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(cmd_lbl, font10, 0);

        // Password input box
        lv_obj_t *pw_box = lv_obj_create(box_);
        lv_obj_remove_style_all(pw_box);
        lv_obj_set_size(pw_box, 180, 22);
        lv_obj_set_pos(pw_box, 20, 46);
        lv_obj_set_style_bg_color(pw_box, lv_color_hex(0x0D0D1A), 0);
        lv_obj_set_style_bg_opa(pw_box, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(pw_box, 3, 0);
        lv_obj_set_style_border_color(pw_box, lv_color_hex(0x3A5A8A), 0);
        lv_obj_set_style_border_width(pw_box, 1, 0);
        lv_obj_set_style_pad_hor(pw_box, 4, 0);
        lv_obj_set_style_pad_ver(pw_box, 2, 0);
        lv_obj_clear_flag(pw_box, LV_OBJ_FLAG_SCROLLABLE);

        pw_lbl_ = lv_label_create(pw_box);
        lv_obj_set_width(pw_lbl_, 172);
        lv_obj_set_style_text_color(pw_lbl_, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(pw_lbl_, font12, 0);
        lv_label_set_long_mode(pw_lbl_, LV_LABEL_LONG_CLIP);
        lv_obj_align(pw_lbl_, LV_ALIGN_LEFT_MID, 0, 0);
        update_pw_display();

        // Hint line
        hint_lbl_ = lv_label_create(box_);
        lv_label_set_text(hint_lbl_, "Enter:OK  ESC:Cancel");
        lv_obj_set_pos(hint_lbl_, 0, 88);
        lv_obj_set_width(hint_lbl_, 220);
        lv_obj_set_style_text_align(hint_lbl_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0x555566), 0);
        lv_obj_set_style_text_font(hint_lbl_, font10, 0);

        // Cursor blink timer (500 ms)
        cursor_timer_ = lv_timer_create(cursor_timer_cb, 500, this);
    }

    void grab_input()
    {
        // Register on active screen so LV_EVENT_KEYBOARD reaches us
        key_hook_obj_ = lv_screen_active();
        if (key_hook_obj_)
            lv_obj_add_event_cb(key_hook_obj_, key_event_cb,
                                static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), this);
        // Also push a dedicated group so indev state is clean
        saved_group_ = lv_group_get_default();
        prompt_group_ = lv_group_create();
        lv_group_add_obj(prompt_group_, overlay_);
        lv_group_set_default(prompt_group_);
        lv_indev_t *indev = lv_indev_get_next(nullptr);
        if (indev)
            lv_indev_set_group(indev, prompt_group_);
    }

    void release_input()
    {
        if (key_hook_obj_) {
            lv_obj_remove_event_cb_with_user_data(key_hook_obj_, key_event_cb, this);
            key_hook_obj_ = nullptr;
        }
        lv_group_set_default(saved_group_);
        lv_indev_t *indev = lv_indev_get_next(nullptr);
        if (indev)
            lv_indev_set_group(indev, saved_group_);
        if (prompt_group_) {
            lv_group_delete(prompt_group_);
            prompt_group_ = nullptr;
        }
    }

    // ---- input ----
    void handle_key(const struct key_item *elm)
    {
        if (!elm || elm->key_state != KBD_KEY_RELEASED)
            return;

        uint32_t key = elm->key_code;

        if (key == KEY_ESC) {
            dismiss(kCancelled);
            return;
        }
        if (key == KEY_ENTER) {
            start_execute();
            return;
        }
        if (key == KEY_BACKSPACE) {
            if (!pw_buf_.empty()) pw_buf_.pop_back();
            update_pw_display();
            return;
        }
        if (elm->utf8[0] && (unsigned char)elm->utf8[0] >= 0x20) {
            pw_buf_ += elm->utf8;
            update_pw_display();
        }
    }

    void update_pw_display()
    {
        if (!pw_lbl_) return;
        std::string masked(pw_buf_.size(), '*');
        masked += (cursor_vis_ ? '_' : ' ');
        lv_label_set_text(pw_lbl_, masked.c_str());
    }

    // ---- execute ----
    void start_execute()
    {
        if (hint_lbl_)
            lv_label_set_text(hint_lbl_, "Running...");
        lv_refr_now(nullptr);

        // Run command via sudo -S on a one-shot timer so the frame renders first
        exec_timer_ = lv_timer_create(exec_timer_cb, 20, this);
        lv_timer_set_repeat_count(exec_timer_, 1);
    }

    int run_with_sudo()
    {
        if (cmd_.empty()) return -EINVAL;
        // Build null-terminated argv array for cp0_process_run_sudo
        std::vector<const char *> argv;
        for (const auto &s : cmd_)
            argv.push_back(s.c_str());
        argv.push_back(nullptr);
        return cp0_process_run_sudo(pw_buf_.c_str(), argv.data());
    }

    void finish_execute()
    {
        int ret = run_with_sudo();
        if (ret != 0 && ret != kCancelled) {
            // Show error, let user retry or cancel
            if (hint_lbl_) {
                char buf[48];
                snprintf(buf, sizeof(buf), "Failed (code %d)  ESC:close", ret);
                lv_label_set_text(hint_lbl_, buf);
                lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0xFF4444), 0);
            }
            pw_buf_.clear();
            update_pw_display();
            // Re-enable input for retry
            grab_input();
            return;
        }
        dismiss(ret);
    }

    void dismiss(int result)
    {
        stop_timers();
        release_input();
        if (box_)     { lv_obj_del(box_);     box_     = nullptr; }
        if (overlay_) { lv_obj_del(overlay_); overlay_ = nullptr; }
        Callback cb = std::move(cb_);
        delete s_instance_;
        s_instance_ = nullptr;
        if (cb) cb(result);
    }

    void stop_timers()
    {
        if (cursor_timer_) { lv_timer_delete(cursor_timer_); cursor_timer_ = nullptr; }
        if (exec_timer_)   { lv_timer_delete(exec_timer_);   exec_timer_   = nullptr; }
    }

    // ---- static LVGL callbacks ----
    static void cursor_timer_cb(lv_timer_t *t)
    {
        auto *self = static_cast<SudoPrompt *>(lv_timer_get_user_data(t));
        if (!self) return;
        self->cursor_vis_ = !self->cursor_vis_;
        self->update_pw_display();
    }

    static void exec_timer_cb(lv_timer_t *t)
    {
        auto *self = static_cast<SudoPrompt *>(lv_timer_get_user_data(t));
        if (!self) return;
        self->exec_timer_ = nullptr;
        // Release input before blocking exec so no stale keys queue up
        self->release_input();
        self->finish_execute();
    }

    static void key_event_cb(lv_event_t *e)
    {
        auto *self = static_cast<SudoPrompt *>(lv_event_get_user_data(e));
        if (!self) return;
        const struct key_item *elm = static_cast<const struct key_item *>(lv_event_get_param(e));
        self->handle_key(elm);
    }
};
