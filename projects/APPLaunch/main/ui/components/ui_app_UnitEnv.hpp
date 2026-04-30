#pragma once

#include "../ui.h"
#include "ui_app_page.hpp"
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>

#include "compat/input_keys.h"
#include <keyboard_input.h>


/*
 * ============================================================
 *  UIUnitEnvPage
 *
 *  全屏温湿度界面
 *  Screen: 320 x 170
 *
 *  显示内容：
 *    - Temperature
 *    - Humidity
 *    - 日期时间
 *    - 温湿度趋势曲线图
 *
 *  按键：
 *    ESC 返回主页
 * ============================================================
 */
class UIUnitEnvPage : public app_
{
public:
    UIUnitEnvPage() : app_()
    {
        creat_UI();
        event_handler_init();

        set_env_values(26.5f, 60);
        start_refresh_timer();
    }

    ~UIUnitEnvPage()
    {
        if (refresh_timer_) {
            lv_timer_delete(refresh_timer_);
            refresh_timer_ = nullptr;
        }
    }

public:
    /*
     * 外部更新温湿度接口
     */
    void set_env_values(float temperature, int humidity)
    {
        temperature_ = temperature;
        humidity_ = humidity;

        if (humidity_ < 0) humidity_ = 0;
        if (humidity_ > 100) humidity_ = 100;

        char temp_buf[16];
        snprintf(temp_buf, sizeof(temp_buf), "%.1f", temperature_);
        lv_label_set_text(ui_obj_["lbl_temp_value"], temp_buf);

        char hum_buf[16];
        snprintf(hum_buf, sizeof(hum_buf), "%d", humidity_);
        lv_label_set_text(ui_obj_["lbl_hum_value"], hum_buf);
    }

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;

    lv_timer_t *refresh_timer_ = nullptr;

    float temperature_ = 26.5f;
    int humidity_ = 60;

    lv_chart_series_t *temp_series_ = nullptr;
    lv_chart_series_t *hum_series_ = nullptr;

    std::vector<int> temp_points_ = {28, 32, 30, 38, 45, 37, 30, 40};
    std::vector<int> hum_points_  = {18, 35, 62, 70, 56, 30, 45, 66};

private:
    /*
     * ============================================================
     * UI 构建
     * ============================================================
     */
    void creat_UI()
    {
        /*
         * 背景
         */
        lv_obj_t *bg = lv_obj_create(ui_root);
        lv_obj_set_size(bg, 320, 170);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x1F1F22), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        create_top_info(bg);
        create_chart_panel(bg);
    }

    void create_top_info(lv_obj_t *parent)
    {
        /*
         * 左侧温度标题图标
         */
        lv_obj_t *temp_icon = lv_obj_create(parent);
        lv_obj_set_size(temp_icon, 7, 14);
        lv_obj_set_pos(temp_icon, 27, 12);
        lv_obj_set_style_radius(temp_icon, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(temp_icon, lv_color_hex(0x1DB56C), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(temp_icon, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(temp_icon, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(temp_icon, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_temp_title = lv_label_create(parent);
        lv_label_set_text(lbl_temp_title, "Temperature");
        lv_obj_set_pos(lbl_temp_title, 40, 9);
        lv_obj_set_style_text_color(lbl_temp_title, lv_color_hex(0x8B8B8B), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_temp_title, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        /*
         * 温度数值
         */
        lv_obj_t *lbl_temp_value = lv_label_create(parent);
        lv_label_set_text(lbl_temp_value, "26.5");
        lv_obj_set_pos(lbl_temp_value, 26, 28);
        lv_obj_set_style_text_color(lbl_temp_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_temp_value, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_temp_value"] = lbl_temp_value;

        lv_obj_t *lbl_temp_unit = lv_label_create(parent);
        lv_label_set_text(lbl_temp_unit, "°C");
        lv_obj_set_pos(lbl_temp_unit, 88, 42);
        lv_obj_set_style_text_color(lbl_temp_unit, lv_color_hex(0xD0D0D0), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_temp_unit, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        /*
         * 湿度标题图标
         */
        lv_obj_t *hum_icon = lv_obj_create(parent);
        lv_obj_set_size(hum_icon, 10, 10);
        lv_obj_set_pos(hum_icon, 132, 14);
        lv_obj_set_style_radius(hum_icon, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(hum_icon, lv_color_hex(0x168DFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(hum_icon, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(hum_icon, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(hum_icon, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_hum_title = lv_label_create(parent);
        lv_label_set_text(lbl_hum_title, "Humidity");
        lv_obj_set_pos(lbl_hum_title, 146, 9);
        lv_obj_set_style_text_color(lbl_hum_title, lv_color_hex(0x8B8B8B), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hum_title, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        /*
         * 湿度数值
         */
        lv_obj_t *lbl_hum_value = lv_label_create(parent);
        lv_label_set_text(lbl_hum_value, "60");
        lv_obj_set_pos(lbl_hum_value, 132, 28);
        lv_obj_set_style_text_color(lbl_hum_value, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hum_value, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_hum_value"] = lbl_hum_value;

        lv_obj_t *lbl_hum_unit = lv_label_create(parent);
        lv_label_set_text(lbl_hum_unit, "%");
        lv_obj_set_pos(lbl_hum_unit, 179, 42);
        lv_obj_set_style_text_color(lbl_hum_unit, lv_color_hex(0xD0D0D0), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hum_unit, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        /*
         * 右上角日期时间
         */
        lv_obj_t *lbl_date = lv_label_create(parent);
        lv_label_set_text(lbl_date, "Apr 25, 21:10");
        lv_obj_set_pos(lbl_date, 236, 13);
        lv_obj_set_style_text_color(lbl_date, lv_color_hex(0x777777), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_date"] = lbl_date;

        /*
         * 右上角绿色小状态块
         */
        lv_obj_t *status_box = lv_obj_create(parent);
        lv_obj_set_size(status_box, 14, 7);
        lv_obj_set_pos(status_box, 292, 29);
        lv_obj_set_style_radius(status_box, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(status_box, lv_color_hex(0x00B050), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(status_box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(status_box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(status_box, LV_OBJ_FLAG_SCROLLABLE);
    }

    void create_chart_panel(lv_obj_t *parent)
    {
        /*
         * 曲线图外框
         */
        lv_obj_t *panel = lv_obj_create(parent);
        lv_obj_set_size(panel, 302, 100);
        lv_obj_set_pos(panel, 9, 62);
        lv_obj_set_style_radius(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x202124), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(panel, lv_color_hex(0x3F3F3F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["panel"] = panel;

        /*
         * 半透明绿色区域，模拟湿度曲线下方阴影
         */
        lv_obj_t *green_area = lv_obj_create(panel);
        lv_obj_set_size(green_area, 260, 48);
        lv_obj_set_pos(green_area, 26, 32);
        lv_obj_set_style_radius(green_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(green_area, lv_color_hex(0x008C5A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(green_area, 55, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(green_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(green_area, LV_OBJ_FLAG_SCROLLABLE);

        /*
         * 半透明蓝色区域，模拟温度曲线下方阴影
         */
        lv_obj_t *blue_area = lv_obj_create(panel);
        lv_obj_set_size(blue_area, 260, 42);
        lv_obj_set_pos(blue_area, 26, 42);
        lv_obj_set_style_radius(blue_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(blue_area, lv_color_hex(0x0077AA), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(blue_area, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(blue_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(blue_area, LV_OBJ_FLAG_SCROLLABLE);

        /*
         * LVGL Chart
         */
        lv_obj_t *chart = lv_chart_create(panel);
        lv_obj_set_size(chart, 260, 78);
        lv_obj_set_pos(chart, 28, 6);

        lv_obj_set_style_bg_opa(chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_line_color(chart, lv_color_hex(0x4A4A4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_opa(chart, 180, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(chart, 8);
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 80);
        lv_chart_set_div_line_count(chart, 5, 6);

        /*
         * 隐藏点，只显示线
         */
        lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

        hum_series_ = lv_chart_add_series(chart, lv_color_hex(0x00A86B), LV_CHART_AXIS_PRIMARY_Y);
        temp_series_ = lv_chart_add_series(chart, lv_color_hex(0x00AEEF), LV_CHART_AXIS_PRIMARY_Y);

        for (int i = 0; i < 8; ++i) {
            lv_chart_set_value_by_id(chart, hum_series_, i, hum_points_[i]);
            lv_chart_set_value_by_id(chart, temp_series_, i, temp_points_[i]);
        }

        lv_chart_refresh(chart);
        ui_obj_["chart"] = chart;

        create_axis_labels(panel);
    }

    void create_axis_labels(lv_obj_t *panel)
    {
        /*
         * 左侧湿度百分比刻度
         */
        const char *left_labels[] = {
            "80%", "70%", "60%", "50%", "40%", "30%", "20%", "10%", "0%"
        };

        for (int i = 0; i < 9; ++i) {
            lv_obj_t *lbl = lv_label_create(panel);
            lv_label_set_text(lbl, left_labels[i]);
            lv_obj_set_pos(lbl, 4, 3 + i * 9);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x8A8A8A), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        /*
         * 右侧温度刻度
         */
        const char *right_labels[] = {
            "30°", "25°", "20°", "15°", "10°", "5°", "0°", "-5°", "-10°"
        };

        for (int i = 0; i < 9; ++i) {
            lv_obj_t *lbl = lv_label_create(panel);
            lv_label_set_text(lbl, right_labels[i]);
            lv_obj_set_pos(lbl, 286, 3 + i * 9);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x8A8A8A), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        /*
         * 底部时间刻度
         */
        const char *time_labels[] = {
            "13 PM", "14 PM", "15 PM", "16 PM"
        };

        int x_pos[] = {30, 98, 166, 234};

        for (int i = 0; i < 4; ++i) {
            lv_obj_t *lbl = lv_label_create(panel);
            lv_label_set_text(lbl, time_labels[i]);
            lv_obj_set_pos(lbl, x_pos[i], 84);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x777777), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

private:
    /*
     * ============================================================
     * 定时刷新
     * ============================================================
     */
    void start_refresh_timer()
    {
        if (!refresh_timer_) {
            refresh_timer_ = lv_timer_create(refresh_timer_cb, 5000, this);
        }
    }

    static void refresh_timer_cb(lv_timer_t *timer)
    {
        UIUnitEnvPage *self = static_cast<UIUnitEnvPage *>(lv_timer_get_user_data(timer));
        if (self) {
            self->on_refresh_timer();
        }
    }

    void on_refresh_timer()
    {
        /*
         * 如果项目里有 hal_time_str，可以改成真实时间。
         * 这里保持静态日期格式。
         */
        static int minute = 10;
        minute += 5;
        if (minute >= 60) minute = 0;

        char date_buf[32];
        snprintf(date_buf, sizeof(date_buf), "Apr 25, 21:%02d", minute);
        lv_label_set_text(ui_obj_["lbl_date"], date_buf);

        /*
         * 模拟温湿度轻微变化。
         * 如果有真实传感器，在这里替换成真实数据即可。
         */
        int temp_delta = (rand() % 3) - 1;
        int hum_delta = (rand() % 5) - 2;

        float new_temp = temperature_ + temp_delta * 0.2f;
        int new_hum = humidity_ + hum_delta;

        if (new_hum < 30) new_hum = 30;
        if (new_hum > 80) new_hum = 80;

        set_env_values(new_temp, new_hum);

        update_chart_data();
    }

    void update_chart_data()
    {
        if (!ui_obj_.count("chart")) return;

        lv_obj_t *chart = ui_obj_["chart"];

        /*
         * 温度映射到 0~80 范围
         * 简单映射：-10°C ~ 30°C -> 0 ~ 80
         */
        int temp_mapped = (int)((temperature_ + 10.0f) * 2.0f);
        if (temp_mapped < 0) temp_mapped = 0;
        if (temp_mapped > 80) temp_mapped = 80;

        int hum_mapped = humidity_;
        if (hum_mapped < 0) hum_mapped = 0;
        if (hum_mapped > 80) hum_mapped = 80;

        if (!temp_points_.empty()) {
            temp_points_.erase(temp_points_.begin());
            temp_points_.push_back(temp_mapped);
        }

        if (!hum_points_.empty()) {
            hum_points_.erase(hum_points_.begin());
            hum_points_.push_back(hum_mapped);
        }

        for (int i = 0; i < 8; ++i) {
            lv_chart_set_value_by_id(chart, hum_series_, i, hum_points_[i]);
            lv_chart_set_value_by_id(chart, temp_series_, i, temp_points_[i]);
        }

        lv_chart_refresh(chart);
    }

private:
    /*
     * ============================================================
     * 按键事件
     * ============================================================
     */
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIUnitEnvPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIUnitEnvPage *self = static_cast<UIUnitEnvPage *>(lv_event_get_user_data(e));
        if (self) {
            self->event_handler(e);
        }
    }

    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e)) {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            handle_key(key);
        }
    }

    void handle_key(uint32_t key)
    {
        switch (key) {
        case KEY_ESC:
            if (go_back_home) {
                go_back_home();
            }
            break;

        default:
            break;
        }
    }
};