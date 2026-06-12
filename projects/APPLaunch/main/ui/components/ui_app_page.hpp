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
    int height_ = 20;

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
        // lv_group_focus_obj(root_screen_);
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
        update_status_bar();
        status_timer_ = lv_timer_create(app_status_timer_cb, 5000, this);
    }

    virtual ~AppTopBarRegion()
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
        page_title_ = title;
        top_bar_.set_title(title);
    }

private:
    UIAppTopBar top_bar_;
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
    lv_obj_t *ui_TOP_time = nullptr;
    lv_obj_t *ui_TOP_time_Label = nullptr;
    lv_obj_t *ui_TOP_Power = nullptr;
    lv_obj_t *ui_TOP_power_Label = nullptr;
    lv_timer_t *status_timer_ = nullptr;

public:
    lv_obj_t *ui_APP_Container = nullptr;

public:
    home_base() : AppPageRoot()
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

    void update_status_bar()
    {
        update_time_status();
        update_wifi_status();
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
            if (soc == 100)
                lv_obj_set_style_text_font(ui_TOP_power_Label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
            else
                lv_obj_set_style_text_font(ui_TOP_power_Label, LV_FONT_DEFAULT, LV_PART_MAIN | LV_STATE_DEFAULT);

            uint32_t color = 0x66CC33;
            if (soc <= 20)
                color = 0xE74C3C;
            else if (soc <= 50)
                color = 0xF39C12;
            lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(color),
                                      LV_PART_INDICATOR | LV_STATE_DEFAULT);
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

#ifdef APPLAUNCH_LOGO_USE_PNG
        ui_TOP_logo = lv_img_create(ui_TOP_Container);
        lv_img_set_src(ui_TOP_logo, cp0_file_path_c("launcher_brand_logo.png"));
        lv_obj_set_width(ui_TOP_logo, LV_SIZE_CONTENT);
        lv_obj_set_height(ui_TOP_logo, LV_SIZE_CONTENT);
        lv_obj_set_x(ui_TOP_logo, 5);
        lv_obj_set_y(ui_TOP_logo, 5);
        lv_obj_add_flag(ui_TOP_logo, LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_clear_flag(ui_TOP_logo, LV_OBJ_FLAG_SCROLLABLE);
#else
        ui_TOP_logo = lv_label_create(ui_TOP_Container);
        lv_label_set_text(ui_TOP_logo, "ZERO");
        lv_obj_set_x(ui_TOP_logo, 5);
        lv_obj_set_y(ui_TOP_logo, 2);
        lv_obj_set_style_text_font(ui_TOP_logo, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_TOP_logo, lv_color_hex(0xCCAA00), LV_PART_MAIN | LV_STATE_DEFAULT);
#endif

        create_wifi_status(ui_TOP_Container);

        ui_TOP_time = lv_obj_create(ui_TOP_Container);
        lv_obj_set_width(ui_TOP_time, 40);
        lv_obj_set_height(ui_TOP_time, 16);
        lv_obj_set_x(ui_TOP_time, 236);
        lv_obj_set_y(ui_TOP_time, 4);
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

        lv_obj_t *battery_panel = lv_obj_create(ui_TOP_Container);
        lv_obj_set_width(battery_panel, 36);
        lv_obj_set_height(battery_panel, 16);
        lv_obj_set_x(battery_panel, 280);
        lv_obj_set_y(battery_panel, 4);
        lv_obj_clear_flag(battery_panel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(battery_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(battery_panel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(battery_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(battery_panel, cp0_file_path_c("status_battery_background.png"), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(battery_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(battery_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_Power = lv_bar_create(battery_panel);
        lv_bar_set_value(ui_TOP_Power, 96, LV_ANIM_OFF);
        lv_bar_set_start_value(ui_TOP_Power, 0, LV_ANIM_OFF);
        lv_obj_set_width(ui_TOP_Power, 33);
        lv_obj_set_height(ui_TOP_Power, 14);
        lv_obj_set_align(ui_TOP_Power, LV_ALIGN_CENTER);
        lv_obj_set_style_radius(ui_TOP_Power, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(0x484847), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_Power, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_radius(ui_TOP_Power, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(0x666633), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_Power, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

        ui_TOP_power_Label = lv_label_create(ui_TOP_Power);
        lv_obj_set_width(ui_TOP_power_Label, LV_SIZE_CONTENT);
        lv_obj_set_height(ui_TOP_power_Label, LV_SIZE_CONTENT);
        lv_obj_set_align(ui_TOP_power_Label, LV_ALIGN_CENTER);
        lv_label_set_text(ui_TOP_power_Label, "96%");
        lv_obj_set_style_text_color(ui_TOP_power_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_TOP_power_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_APP_Container = lv_obj_create(root_screen_);
        lv_obj_remove_style_all(ui_APP_Container);
        lv_obj_set_size(ui_APP_Container, 320, 150);
        lv_obj_set_pos(ui_APP_Container, 0, 20);
        lv_obj_clear_flag(ui_APP_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE)); /// Flags
    }

    void create_wifi_status(lv_obj_t *parent)
    {
        static const int bar_heights[4] = {6, 9, 12, 15};

        ui_TOP_wifiPanel = lv_obj_create(parent);
        lv_obj_set_width(ui_TOP_wifiPanel, 24);
        lv_obj_set_height(ui_TOP_wifiPanel, 15);
        lv_obj_set_x(ui_TOP_wifiPanel, 210);
        lv_obj_set_y(ui_TOP_wifiPanel, 4);
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

        cp0_wifi_status_t ws = cp0_wifi_get_status();
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
