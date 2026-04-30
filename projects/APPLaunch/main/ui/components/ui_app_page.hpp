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
#include <keyboard_input.h>
#include <functional>
#include "hal/hal_settings.h"
#define APP_CONSOLE_EXIT_EVENT (lv_event_code_t)(LV_EVENT_LAST + 1)

static inline std::string img_path(const char *name)
{
    return std::string(hal_path_images_dir()) + "/" + name;
}

class app_
{

public:
    lv_group_t *key_group;
    lv_obj_t *ui_root;
    lv_obj_t *get_ui() { return ui_root; }
    lv_group_t *get_key_group() { return key_group; }
    std::function<void(void)> go_back_home;

public:
    app_()
    {
        creat_base_UI();
        creat_input_group();
    }
    ~app_()
    {
        lv_obj_del(ui_root);
    }


private:
    /* ================================================================== */
    /*  UI 构建                                                             */
    /* ================================================================== */
    void creat_base_UI()
    {
        ui_root = lv_obj_create(NULL);
        lv_obj_clear_flag(ui_root, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
        lv_obj_set_style_bg_color(ui_root, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_root, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void creat_input_group()
    {
        key_group = lv_group_create();
        lv_group_add_obj(key_group, ui_root);
    }

    // static void static_event_handler(lv_event_t * e)
    // {
    //     app_base *instance = static_cast<app_base *>(lv_event_get_user_data(e));
    //     if (instance)
    //     {
    //         instance->event_handler(e);
    //     }
    // }

    // virtual void event_handler(lv_event_t * e)
    // {

    // }

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
    home_base(): app_()
    {
        creat_Top_UI();
        UI_bind_event();
        update_status_bar();
        status_timer_ = lv_timer_create(home_status_timer_cb, 5000, this);
    }
    ~home_base()
    {
        if (status_timer_) lv_timer_delete(status_timer_);
    }

    static void home_battery_event_cb(lv_event_t *e)
    {
        home_base *self = static_cast<home_base *>(lv_event_get_user_data(e));
        if (!self || lv_event_get_code(e) != LV_EVENT_BATTERY) return;
        const hal_battery_info_t *bat = LV_EVENT_BATTERY_GET_INFO(e);
        if (bat) self->update_battery_status(*bat);
    }

    static void home_status_timer_cb(lv_timer_t *timer)
    {
        home_base *self = static_cast<home_base *>(lv_timer_get_user_data(timer));
        if (self) self->update_status_bar();
    }

    void update_status_bar()
    {
        char time_buf[16];
        hal_time_str(time_buf, sizeof(time_buf));
        lv_label_set_text(ui_TOP_time_Label, time_buf);

    }

    void update_battery_status(const hal_battery_info_t &bat)
    {
        if (bat.valid) {
            int soc = bat.soc;
            if (soc > 100) soc = 100;
            if (soc < 0) soc = 0;
            lv_bar_set_value(ui_TOP_Power, soc, LV_ANIM_ON);
            char pwr_buf[16];
            snprintf(pwr_buf, sizeof(pwr_buf), "%d%%", soc);
            lv_label_set_text(ui_TOP_power_Label, pwr_buf);

            uint32_t color = 0x66CC33;
            if (soc <= 20) color = 0xE74C3C;
            else if (soc <= 50) color = 0xF39C12;
            lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(color),
                                      LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
    }

private:
    /* ================================================================== */
    /*  UI 构建                                                             */
    /* ================================================================== */
    void creat_Top_UI()
    {
        ui_TOP_logo = lv_img_create(ui_root);
        lv_img_set_src(ui_TOP_logo, ui_img_zero_png);
        lv_obj_set_width(ui_TOP_logo, LV_SIZE_CONTENT);   /// 49
        lv_obj_set_height(ui_TOP_logo, LV_SIZE_CONTENT);    /// 12
        lv_obj_set_x(ui_TOP_logo, 5);
        lv_obj_set_y(ui_TOP_logo, 5);
        lv_obj_add_flag(ui_TOP_logo, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
        lv_obj_clear_flag(ui_TOP_logo, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

        ui_TOP_time = lv_obj_create(ui_root);
        lv_obj_set_width(ui_TOP_time, 45);
        lv_obj_set_height(ui_TOP_time, 16);
        lv_obj_set_x(ui_TOP_time, 237);
        lv_obj_set_y(ui_TOP_time, 5);
        lv_obj_clear_flag(ui_TOP_time, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
        lv_obj_set_style_radius(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_time, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(ui_TOP_time, ui_img_time_png, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_time_Label = lv_label_create(ui_TOP_time);
        lv_obj_set_width(ui_TOP_time_Label, LV_SIZE_CONTENT);   /// 1
        lv_obj_set_height(ui_TOP_time_Label, LV_SIZE_CONTENT);    /// 1
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
        lv_obj_set_width(ui_TOP_power_Label, LV_SIZE_CONTENT);   /// 1
        lv_obj_set_height(ui_TOP_power_Label, LV_SIZE_CONTENT);    /// 1
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
        lv_obj_clear_flag(ui_APP_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    }

    void UI_bind_event()
    {
        lv_obj_add_event_cb(ui_root, home_battery_event_cb, (lv_event_code_t)LV_EVENT_BATTERY, this);
    }
};



class app_base : public app_
{
private:
    lv_obj_t *ui_TOP_logo;
    lv_obj_t *ui_TOP_time;
    lv_obj_t *ui_TOP_time_Label;
    lv_obj_t *ui_TOP_SignalStrength;
    lv_obj_t *ui_TOP_SignalStrength_one;
    lv_obj_t *ui_TOP_SignalStrength_two;
    lv_obj_t *ui_TOP_SignalStrength_three;
    lv_obj_t *ui_TOP_SignalStrength_four;
    lv_obj_t *ui_TOP_Power;
    lv_obj_t *ui_TOP_power_Label;
    lv_timer_t *status_timer_ = nullptr;

public:
    lv_obj_t *ui_APP_Container;


public:
    app_base(): app_()
    {
        creat_Top_UI();
        UI_bind_event();
        update_status_bar();
        status_timer_ = lv_timer_create(app_status_timer_cb, 5000, this);
    }
    ~app_base()
    {
        if (status_timer_) lv_timer_delete(status_timer_);
    }

    static void app_battery_event_cb(lv_event_t *e)
    {
        app_base *self = static_cast<app_base *>(lv_event_get_user_data(e));
        if (!self || lv_event_get_code(e) != LV_EVENT_BATTERY) return;
        const hal_battery_info_t *bat = LV_EVENT_BATTERY_GET_INFO(e);
        if (bat) self->update_battery_status(*bat);
    }

    static void app_status_timer_cb(lv_timer_t *timer)
    {
        app_base *self = static_cast<app_base *>(lv_timer_get_user_data(timer));
        if (self) self->update_status_bar();
    }

    void update_status_bar()
    {
        char time_buf[16];
        hal_time_str(time_buf, sizeof(time_buf));
        lv_label_set_text(ui_TOP_time_Label, time_buf);

        hal_wifi_status_t ws = hal_wifi_get_status();
        int sig = ws.connected ? ws.signal : 0;
        uint32_t on_color  = 0x00CCFF;
        uint32_t off_color = 0x4D4D4D;
        lv_obj_set_style_bg_color(ui_TOP_SignalStrength_one,
            lv_color_hex(sig > 0 ? on_color : off_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_SignalStrength_two,
            lv_color_hex(sig >= 30 ? on_color : off_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_SignalStrength_three,
            lv_color_hex(sig >= 60 ? on_color : off_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_SignalStrength_four,
            lv_color_hex(sig >= 80 ? on_color : off_color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void update_battery_status(const hal_battery_info_t &bat)
    {
        if (bat.valid) {
            int soc = bat.soc;
            if (soc > 100) soc = 100;
            if (soc < 0) soc = 0;
            char pwr_buf[16];
            snprintf(pwr_buf, sizeof(pwr_buf), "%d%%", soc);
            lv_label_set_text(ui_TOP_power_Label, pwr_buf);
        }
    }

private:
    /* ================================================================== */
    /*  UI 构建                                                             */
    /* ================================================================== */
    void creat_Top_UI()
    {
        ui_TOP_logo = lv_img_create(ui_root);
        lv_img_set_src(ui_TOP_logo, ui_img_zero_logo_w_png);
        lv_obj_set_width(ui_TOP_logo, LV_SIZE_CONTENT);   /// 58
        lv_obj_set_height(ui_TOP_logo, LV_SIZE_CONTENT);    /// 12
        lv_obj_set_x(ui_TOP_logo, 5);
        lv_obj_set_y(ui_TOP_logo, 5);
        lv_obj_add_flag(ui_TOP_logo, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
        lv_obj_clear_flag(ui_TOP_logo, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

        ui_TOP_time = lv_obj_create(ui_root);
        lv_obj_set_width(ui_TOP_time, 40);
        lv_obj_set_height(ui_TOP_time, 13);
        lv_obj_set_x(ui_TOP_time, 206);
        lv_obj_set_y(ui_TOP_time, 3);
        lv_obj_clear_flag(ui_TOP_time, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
        lv_obj_set_style_radius(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_time, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_time, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_time, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_time_Label = lv_label_create(ui_TOP_time);
        lv_obj_set_width(ui_TOP_time_Label, LV_SIZE_CONTENT);   /// 1
        lv_obj_set_height(ui_TOP_time_Label, LV_SIZE_CONTENT);    /// 1
        lv_obj_set_align(ui_TOP_time_Label, LV_ALIGN_CENTER);
        lv_label_set_text(ui_TOP_time_Label, "19:45");
        lv_obj_set_style_text_color(ui_TOP_time_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_TOP_time_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_TOP_time_Label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_SignalStrength = lv_obj_create(ui_root);
        lv_obj_set_width(ui_TOP_SignalStrength, 30);
        lv_obj_set_height(ui_TOP_SignalStrength, 13);
        lv_obj_set_x(ui_TOP_SignalStrength, 248);
        lv_obj_set_y(ui_TOP_SignalStrength, 3);
        lv_obj_clear_flag(ui_TOP_SignalStrength, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
        lv_obj_set_style_radius(ui_TOP_SignalStrength, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_SignalStrength, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_SignalStrength, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_SignalStrength, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_SignalStrength_one = lv_obj_create(ui_TOP_SignalStrength);
        lv_obj_set_width(ui_TOP_SignalStrength_one, 5);
        lv_obj_set_height(ui_TOP_SignalStrength_one, 3);
        lv_obj_set_x(ui_TOP_SignalStrength_one, -11);
        lv_obj_set_y(ui_TOP_SignalStrength_one, 2);
        lv_obj_set_align(ui_TOP_SignalStrength_one, LV_ALIGN_CENTER);
        lv_obj_clear_flag(ui_TOP_SignalStrength_one, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
        lv_obj_set_style_radius(ui_TOP_SignalStrength_one, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_SignalStrength_one, lv_color_hex(0x00CCFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_SignalStrength_one, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_SignalStrength_one, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_SignalStrength_two = lv_obj_create(ui_TOP_SignalStrength);
        lv_obj_set_width(ui_TOP_SignalStrength_two, 5);
        lv_obj_set_height(ui_TOP_SignalStrength_two, 6);
        lv_obj_set_x(ui_TOP_SignalStrength_two, -4);
        lv_obj_set_y(ui_TOP_SignalStrength_two, 1);
        lv_obj_set_align(ui_TOP_SignalStrength_two, LV_ALIGN_CENTER);
        lv_obj_clear_flag(ui_TOP_SignalStrength_two, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
        lv_obj_set_style_radius(ui_TOP_SignalStrength_two, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_SignalStrength_two, lv_color_hex(0x00CCFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_SignalStrength_two, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_SignalStrength_two, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_SignalStrength_three = lv_obj_create(ui_TOP_SignalStrength);
        lv_obj_set_width(ui_TOP_SignalStrength_three, 5);
        lv_obj_set_height(ui_TOP_SignalStrength_three, 7);
        lv_obj_set_x(ui_TOP_SignalStrength_three, 3);
        lv_obj_set_y(ui_TOP_SignalStrength_three, 0);
        lv_obj_set_align(ui_TOP_SignalStrength_three, LV_ALIGN_CENTER);
        lv_obj_clear_flag(ui_TOP_SignalStrength_three, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
        lv_obj_set_style_radius(ui_TOP_SignalStrength_three, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_SignalStrength_three, lv_color_hex(0x00CCFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_SignalStrength_three, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_SignalStrength_three, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_SignalStrength_four = lv_obj_create(ui_TOP_SignalStrength);
        lv_obj_set_width(ui_TOP_SignalStrength_four, 5);
        lv_obj_set_height(ui_TOP_SignalStrength_four, 9);
        lv_obj_set_x(ui_TOP_SignalStrength_four, 10);
        lv_obj_set_y(ui_TOP_SignalStrength_four, -1);
        lv_obj_set_align(ui_TOP_SignalStrength_four, LV_ALIGN_CENTER);
        lv_obj_clear_flag(ui_TOP_SignalStrength_four, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
        lv_obj_set_style_radius(ui_TOP_SignalStrength_four, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_SignalStrength_four, lv_color_hex(0x4D4D4D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_SignalStrength_four, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_SignalStrength_four, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_Power = lv_obj_create(ui_root);
        lv_obj_set_width(ui_TOP_Power, 38);
        lv_obj_set_height(ui_TOP_Power, 13);
        lv_obj_set_x(ui_TOP_Power, 280);
        lv_obj_set_y(ui_TOP_Power, 3);
        lv_obj_clear_flag(ui_TOP_Power, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
        lv_obj_set_style_radius(ui_TOP_Power, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_TOP_Power, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TOP_Power, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_TOP_Power, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_TOP_power_Label = lv_label_create(ui_TOP_Power);
        lv_obj_set_width(ui_TOP_power_Label, LV_SIZE_CONTENT);   /// 1
        lv_obj_set_height(ui_TOP_power_Label, LV_SIZE_CONTENT);    /// 1
        lv_obj_set_align(ui_TOP_power_Label, LV_ALIGN_CENTER);
        lv_label_set_text(ui_TOP_power_Label, "100%");
        lv_obj_set_style_text_color(ui_TOP_power_Label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_TOP_power_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_TOP_power_Label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui_APP_Container = lv_obj_create(ui_root);
        lv_obj_remove_style_all(ui_APP_Container);
        lv_obj_set_width(ui_APP_Container, 320);
        lv_obj_set_height(ui_APP_Container, 150);
        lv_obj_set_x(ui_APP_Container, 0);
        lv_obj_set_y(ui_APP_Container, 10);
        lv_obj_set_align(ui_APP_Container, LV_ALIGN_CENTER);
        lv_obj_clear_flag(ui_APP_Container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    }

    void UI_bind_event()
    {
        lv_obj_add_event_cb(ui_root, app_battery_event_cb, (lv_event_code_t)LV_EVENT_BATTERY, this);
    }
};
