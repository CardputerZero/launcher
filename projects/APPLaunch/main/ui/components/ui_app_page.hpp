#pragma once
#include "../ui.h"
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
#include <keyboard_input.h>
#include <functional>
#include "cp0_lvgl_app.h"
#include "cp0_lvgl_file.hpp"
#define APP_CONSOLE_EXIT_EVENT (lv_event_code_t)(LV_EVENT_LAST + 1)

static inline std::string img_path(const char *name)
{
    return cp0_file_path(name);
}
static inline std::string audio_path(const char *name)
{
    return cp0_file_path(name);
}

class UIAppTopBar
{
public:
    UIAppTopBar() = default;
    explicit UIAppTopBar(std::string title) : title_(std::move(title)) {}

    lv_obj_t *create(lv_obj_t *parent)
    {
        return create(parent, title_);
    }

    lv_obj_t *create(lv_obj_t *parent, const std::string &title)
    {
        title_ = title;
        ui_TOP_Container = lv_obj_create(parent);
        lv_obj_remove_style_all(ui_TOP_Container);
        lv_obj_set_size(ui_TOP_Container, 320, 20);
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
        cp0_wifi_status_t ws = cp0_wifi_get_status();
        set_wifi_signal(ws.connected ? ws.signal : 0);
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
    lv_obj_t *power_label_ = nullptr;
    lv_obj_t *time_panel_ = nullptr;
    lv_obj_t *time_label_ = nullptr;
    lv_obj_t *wifi_panel_ = nullptr;
    lv_obj_t *wifi_bars_[4] = {};

    static lv_font_t *top_title_font()
    {
        return g_font_bold_14 ? g_font_bold_14 : (lv_font_t *)&lv_font_montserrat_14;
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
        lv_obj_set_height(title_label_, 20);
        lv_obj_set_style_text_font(title_label_, top_title_font(), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(title_label_, lv_color_hex(0xCCAA00), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(title_label_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(title_label_, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void create_spacer(lv_obj_t *parent)
    {
        lv_obj_t *spacer = lv_obj_create(parent);
        lv_obj_remove_style_all(spacer);
        lv_obj_set_size(spacer, 0, 20);
        lv_obj_set_flex_grow(spacer, 1);
        lv_obj_clear_flag(spacer, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
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
        lv_obj_set_style_bg_img_src(time_panel_, ui_img_time_png, LV_PART_MAIN | LV_STATE_DEFAULT);

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
        lv_obj_set_style_bg_img_src(battery_panel_, ui_img_battery_bg_png, LV_PART_MAIN | LV_STATE_DEFAULT);

        battery_bar_ = lv_bar_create(battery_panel_);
        lv_bar_set_value(battery_bar_, 96, LV_ANIM_OFF);
        lv_bar_set_start_value(battery_bar_, 0, LV_ANIM_OFF);
        lv_obj_set_size(battery_bar_, 33, 14);
        lv_obj_set_align(battery_bar_, LV_ALIGN_CENTER);
        lv_obj_set_style_radius(battery_bar_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(battery_bar_, lv_color_hex(0x484847), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(battery_bar_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(battery_bar_, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(battery_bar_, lv_color_hex(0x666633), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(battery_bar_, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

        power_label_ = lv_label_create(battery_panel_);
        lv_obj_set_align(power_label_, LV_ALIGN_CENTER);
        lv_label_set_text(power_label_, "96%");
        lv_obj_set_style_text_color(power_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(power_label_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void set_wifi_signal(int signal)
    {
        static const int thresholds[4] = {1, 30, 60, 80};
        const uint32_t on_color = 0x00CCFF;
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

using UIAPPCOM = UIAppContainer;

class app_
{

public:
    std::string app_name = "APP";
    lv_group_t *key_group;
    lv_obj_t *ui_root;
    lv_obj_t *get_ui() { return ui_root; }
    lv_group_t *get_key_group() { return key_group; }
    std::function<void(void)> go_back_home;
    bool have_bottom = false;

public:
    app_()
    {
        creat_base_UI();
        creat_input_group();
    }
    virtual ~app_()
    {
        lv_obj_del(ui_root);
        lv_group_delete(key_group);
    }

    template <typename Component>
    lv_obj_t *add_bar(Component &&component)
    {
        return component.create(ui_root);
    }

private:
    /* ================================================================== */
    /*  UI construction                                                             */
    /* ================================================================== */
    void creat_base_UI()
    {
        ui_root = lv_obj_create(NULL);
        lv_obj_clear_flag(ui_root, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_flex_flow(ui_root, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(ui_root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(ui_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_row(ui_root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_root, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_root, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void creat_input_group()
    {
        key_group = lv_group_create();
        lv_group_add_obj(key_group, ui_root);
        // lv_group_focus_obj(ui_root);
    }
};

class UIAppTopBara : virtual public app_
{
public:
    UIAppTopBara()
    {
        top_bar_.set_title(app_name);
        add_bar(top_bar_);
        UI_bind_event();
        update_status_bar();
        status_timer_ = lv_timer_create(app_status_timer_cb, 5000, this);
    }

    virtual ~UIAppTopBara()
    {
        if (status_timer_)
            lv_timer_delete(status_timer_);
    }

    void update_status_bar()
    {
        top_bar_.update_status();
    }

    void update_battery_status(const cp0_battery_info_t &bat)
    {
        top_bar_.update_battery(bat);
    }

    void set_page_title(const std::string &title)
    {
        app_name = title;
        top_bar_.set_title(title);
    }

private:
    UIAppTopBar top_bar_;
    lv_timer_t *status_timer_ = nullptr;

    static void app_battery_event_cb(lv_event_t *e)
    {
        UIAppTopBara *self = static_cast<UIAppTopBara *>(lv_event_get_user_data(e));
        if (!self || lv_event_get_code(e) != LV_EVENT_BATTERY)
            return;
        const cp0_battery_info_t *bat = LV_EVENT_BATTERY_GET_INFO(e);
        if (bat)
            self->update_battery_status(*bat);
    }

    static void app_status_timer_cb(lv_timer_t *timer)
    {
        UIAppTopBara *self = static_cast<UIAppTopBara *>(lv_timer_get_user_data(timer));
        if (self)
            self->update_status_bar();
    }

    void UI_bind_event()
    {
        lv_obj_add_event_cb(ui_root, app_battery_event_cb, (lv_event_code_t)LV_EVENT_BATTERY, this);
    }
};

class UIAppAPPBara : virtual public app_
{
public:
    UIAppAPPBara()
    {
        refresh();
        ui_APP_Container = add_bar(app_container_);
    }

    void refresh()
    {
        app_container_.set_height(have_bottom ? 130 : 150);
    }

    void refash()
    {
        refresh();
    }

    virtual ~UIAppAPPBara() = default;

    lv_obj_t *ui_APP_Container = nullptr;

private:
    UIAppContainer app_container_;
};

class UIAppbottomBara : virtual public app_, virtual public UIAppAPPBara
{
public:
    UIAppbottomBara()
    {
        have_bottom = true;
        refresh();

        ui_BOTTOM_Container = lv_obj_create(ui_root);
        lv_obj_remove_style_all(ui_BOTTOM_Container);
        lv_obj_set_width(ui_BOTTOM_Container, 320);
        lv_obj_set_height(ui_BOTTOM_Container, 20);
        lv_obj_clear_flag(ui_BOTTOM_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    }

    virtual ~UIAppbottomBara() = default;

    lv_obj_t *ui_BOTTOM_Container = nullptr;
};

class app_page_base : virtual public UIAppTopBara, virtual public UIAppAPPBara
{
public:
    app_page_base() : app_(), UIAppTopBara(), UIAppAPPBara()
    {
    }

    virtual ~app_page_base() = default;
};

class app_with_bottom_bar_base : virtual public UIAppTopBara, virtual public UIAppAPPBara, virtual public UIAppbottomBara
{
public:
    app_with_bottom_bar_base() : app_(), UIAppTopBara(), UIAppAPPBara(), UIAppbottomBara()
    {
    }

    virtual ~app_with_bottom_bar_base() = default;
};

class home_base : public app_
{
private:
    lv_obj_t *ui_TOP_logo;
    lv_obj_t *ui_TOP_time;
    lv_obj_t *ui_TOP_time_Label;
    lv_obj_t *ui_TOP_Power;
    lv_obj_t *ui_TOP_power_Label;
    lv_timer_t *status_timer_ = nullptr;

public:
    lv_obj_t *ui_APP_Container;

public:
    home_base() : app_()
    {
        creat_Top_UI();
        UI_bind_event();
        update_status_bar();
        status_timer_ = lv_timer_create(home_status_timer_cb, 5000, this);
    }
    ~home_base()
    {
        if (status_timer_)
            lv_timer_delete(status_timer_);
    }

    static void home_battery_event_cb(lv_event_t *e)
    {
        home_base *self = static_cast<home_base *>(lv_event_get_user_data(e));
        if (!self || lv_event_get_code(e) != LV_EVENT_BATTERY)
            return;
        const cp0_battery_info_t *bat = LV_EVENT_BATTERY_GET_INFO(e);
        if (bat)
            self->update_battery_status(*bat);
    }

    static void home_status_timer_cb(lv_timer_t *timer)
    {
        home_base *self = static_cast<home_base *>(lv_timer_get_user_data(timer));
        if (self)
            self->update_status_bar();
    }

    void update_status_bar()
    {
        char time_buf[16];
        cp0_time_str(time_buf, sizeof(time_buf));
        lv_label_set_text(ui_TOP_time_Label, time_buf);
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

            uint32_t color = 0x66CC33;
            if (soc <= 20)
                color = 0xE74C3C;
            else if (soc <= 50)
                color = 0xF39C12;
            lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(color),
                                      LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
    }

private:
    /* ================================================================== */
    /*  UI construction                                                             */
    /* ================================================================== */
    void creat_Top_UI()
    {
        ui_TOP_logo = lv_img_create(ui_root);
        lv_img_set_src(ui_TOP_logo, ui_img_zero_png);
        lv_obj_set_width(ui_TOP_logo, LV_SIZE_CONTENT);  /// 49
        lv_obj_set_height(ui_TOP_logo, LV_SIZE_CONTENT); /// 12
        lv_obj_set_x(ui_TOP_logo, 5);
        lv_obj_set_y(ui_TOP_logo, 5);
        lv_obj_add_flag(ui_TOP_logo, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
        lv_obj_clear_flag(ui_TOP_logo, LV_OBJ_FLAG_SCROLLABLE); /// Flags

        ui_TOP_time = lv_obj_create(ui_root);
        lv_obj_set_width(ui_TOP_time, 45);
        lv_obj_set_height(ui_TOP_time, 16);
        lv_obj_set_x(ui_TOP_time, 237);
        lv_obj_set_y(ui_TOP_time, 5);
        lv_obj_clear_flag(ui_TOP_time, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_time, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(ui_TOP_time, ui_img_time_png, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_TOP_time_Label = lv_label_create(ui_TOP_time);
        lv_obj_set_width(ui_TOP_time_Label, LV_SIZE_CONTENT);  /// 1
        lv_obj_set_height(ui_TOP_time_Label, LV_SIZE_CONTENT); /// 1
        lv_obj_set_align(ui_TOP_time_Label, LV_ALIGN_CENTER);
        lv_label_set_text(ui_TOP_time_Label, "15:21");
        lv_obj_set_style_text_color(ui_TOP_time_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_TOP_time_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_Power = lv_bar_create(ui_root);
        lv_bar_set_value(ui_TOP_Power, 96, LV_ANIM_OFF);
        lv_bar_set_start_value(ui_TOP_Power, 0, LV_ANIM_OFF);
        lv_obj_set_width(ui_TOP_Power, 29);
        lv_obj_set_height(ui_TOP_Power, 13);
        lv_obj_set_x(ui_TOP_Power, 286);
        lv_obj_set_y(ui_TOP_Power, 5);
        lv_obj_set_style_radius(ui_TOP_Power, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(0x484847), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_Power, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_radius(ui_TOP_Power, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(0x666633), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_Power, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

        ui_TOP_power_Label = lv_label_create(ui_TOP_Power);
        lv_obj_set_width(ui_TOP_power_Label, LV_SIZE_CONTENT);  /// 1
        lv_obj_set_height(ui_TOP_power_Label, LV_SIZE_CONTENT); /// 1
        lv_obj_set_align(ui_TOP_power_Label, LV_ALIGN_CENTER);
        lv_label_set_text(ui_TOP_power_Label, "96%");
        lv_obj_set_style_text_color(ui_TOP_power_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_TOP_power_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_APP_Container = lv_obj_create(ui_root);
        lv_obj_remove_style_all(ui_APP_Container);
        lv_obj_set_width(ui_APP_Container, 320);
        lv_obj_set_height(ui_APP_Container, 150);
        lv_obj_set_x(ui_APP_Container, 0);
        lv_obj_set_y(ui_APP_Container, 10);
        lv_obj_set_align(ui_APP_Container, LV_ALIGN_CENTER);
        lv_obj_clear_flag(ui_APP_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE)); /// Flags
    }

    void UI_bind_event()
    {
        lv_obj_add_event_cb(ui_root, home_battery_event_cb, (lv_event_code_t)LV_EVENT_BATTERY, this);
    }
};

class tmp_app_bash : public app_page_base
{
public:
    tmp_app_bash() : app_page_base()
    {
    }
};

class app_base : public app_page_base
{
public:
    app_base() : app_page_base()
    {
    }

    ~app_base() override = default;
};
