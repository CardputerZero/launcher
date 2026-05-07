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
 *  UIStockPage
 *
 *  Account Overview Dashboard
 *  Screen: 320 x 170
 *
 *  按键：
 *    ESC 返回主页
 * ============================================================
 */
class UIStockPage : public app_
{
public:
    UIStockPage() : app_()
    {
        creat_UI();
        event_handler_init();
        start_refresh_timer();
    }

    ~UIStockPage()
    {
        if (refresh_timer_) {
            lv_timer_delete(refresh_timer_);
            refresh_timer_ = nullptr;
        }
    }

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;

    lv_timer_t *refresh_timer_ = nullptr;

    lv_chart_series_t *main_series_ = nullptr;

    float current_value_ = 8765.43f;
    float profit_value_  = 8765.43f;

    std::vector<int> chart_points_ = {
        38, 45, 41, 52, 48, 61, 56, 70, 63, 73, 68, 76
    };

private:
    /*
     * ============================================================
     * 基础样式工具
     * ============================================================
     */
    lv_obj_t *create_card(lv_obj_t *parent, int x, int y, int w, int h)
    {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, w, h);
        lv_obj_set_pos(card, x, y);

        lv_obj_set_style_radius(card, 9, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x2B2B31), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        return card;
    }

    lv_obj_t *create_label(lv_obj_t *parent,
                           const char *text,
                           int x,
                           int y,
                           const lv_font_t *font,
                           uint32_t color)
    {
        lv_obj_t *lbl = lv_label_create(parent);
        lv_label_set_text(lbl, text);
        lv_obj_set_pos(lbl, x, y);
        lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        return lbl;
    }

    lv_obj_t *create_small_box(lv_obj_t *parent,
                               int x,
                               int y,
                               int w,
                               int h,
                               uint32_t bg,
                               uint32_t border,
                               const char *text,
                               uint32_t txt_color)
    {
        lv_obj_t *box = lv_obj_create(parent);
        lv_obj_set_size(box, w, h);
        lv_obj_set_pos(box, x, y);

        lv_obj_set_style_radius(box, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(box, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(box, border ? 1 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(box, lv_color_hex(border), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(box);
        lv_label_set_text(lbl, text);
        lv_obj_center(lbl);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(lbl, lv_color_hex(txt_color), LV_PART_MAIN | LV_STATE_DEFAULT);

        return box;
    }

private:
    /*
     * ============================================================
     * UI 构建
     * ============================================================
     */
    void creat_UI()
    {
        lv_obj_t *bg = lv_obj_create(ui_root);
        lv_obj_set_size(bg, 320, 170);
        lv_obj_set_pos(bg, 0, 0);

        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x17171B), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        create_left_overview(bg);
        create_current_value_card(bg);
        create_asset_list_card(bg);
    }

    void create_left_overview(lv_obj_t *parent)
    {
        lv_obj_t *card = create_card(parent, 6, 6, 148, 158);
        ui_obj_["left_card"] = card;

        create_label(card, "Account Overview", 11, 8,
                     &lv_font_montserrat_12, 0xFFFFFF);

        create_label(card, "Apr 25, 21:10", 11, 24,
                     &lv_font_montserrat_10, 0x8E8E95);

        /*
         * 分割线
         */
        lv_obj_t *line = lv_obj_create(card);
        lv_obj_set_size(line, 126, 1);
        lv_obj_set_pos(line, 11, 47);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x55555C), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(line, 180, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);

        create_label(card, "Full", 11, 37,
                     &lv_font_montserrat_10, 0xFFFFFF);

        create_overview_row(card, 11, 62, "Account Value", "$ 283,456.35", 0xFFFFFF);
        create_overview_row(card, 11, 77, "Market Value",  "$ 273,456.78", 0xFFFFFF);
        create_overview_row(card, 11, 92, "Total Gain/Loss", "$ 101,842.21", 0x38D893);
        create_overview_row(card, 11, 107, "Today's Gain/Loss", "$ 23,561.71", 0x38D893);
        create_overview_row(card, 11, 122, "Cash Buying Power", "$ 10,543.78", 0xFFFFFF);
        create_overview_row(card, 11, 137, "Total Positions", "35", 0xFFFFFF);

        /*
         * 底部三个小按钮
         */
        create_small_box(card, 18, 143, 36, 10,
                         0x222227, 0x000000, "o", 0x00C985);

        create_small_box(card, 59, 143, 36, 10,
                         0x222227, 0x000000, "^", 0x32A7FF);

        create_small_box(card, 100, 143, 36, 10,
                         0x222227, 0x000000, "[]", 0xFF375F);
    }

    void create_overview_row(lv_obj_t *parent,
                             int x,
                             int y,
                             const char *name,
                             const char *value,
                             uint32_t value_color)
    {
        create_label(parent, name, x, y,
                     &lv_font_montserrat_10, 0xFFFFFF);

        lv_obj_t *val = create_label(parent, value, 88, y,
                                     &lv_font_montserrat_10, value_color);
        lv_obj_set_width(val, 54);
        lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void create_current_value_card(lv_obj_t *parent)
    {
        lv_obj_t *card = create_card(parent, 158, 6, 156, 58);
        ui_obj_["current_card"] = card;

        create_label(card, "Current Value", 9, 6,
                     &lv_font_montserrat_10, 0xD6D6DC);

        create_label(card, "Potential Value", 104, 6,
                     &lv_font_montserrat_10, 0x8E8E95);

        lv_obj_t *lbl_value = create_label(card, "$ 8765.43", 9, 21,
                                           &lv_font_montserrat_18, 0xFFFFFF);
        ui_obj_["lbl_current_value"] = lbl_value;

        lv_obj_t *lbl_profit = create_label(card, "$ 8765.43", 112, 21,
                                            &lv_font_montserrat_10, 0xFFFFFF);
        ui_obj_["lbl_profit_value"] = lbl_profit;

        /*
         * 右侧小胶囊按钮
         */
        lv_obj_t *pill = lv_obj_create(card);
        lv_obj_set_size(pill, 48, 15);
        lv_obj_set_pos(pill, 101, 38);
        lv_obj_set_style_radius(pill, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(pill, lv_color_hex(0x1F2025), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(pill, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(pill, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(pill, lv_color_hex(0x3A3B42), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

        create_label(pill, "New Clients  >", 5, 2,
                     &lv_font_montserrat_10, 0xAFAFB6);
    }

    void create_asset_list_card(lv_obj_t *parent)
    {
        lv_obj_t *card = create_card(parent, 158, 68, 156, 96);
        ui_obj_["asset_card"] = card;

        /*
         * 顶部趋势图
         */
        lv_obj_t *chart = lv_chart_create(card);
        lv_obj_set_size(chart, 145, 28);
        lv_obj_set_pos(chart, 5, 2);

        lv_obj_set_style_bg_opa(chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_width(chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(chart, chart_points_.size());
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
        lv_chart_set_div_line_count(chart, 0, 0);

        lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

        main_series_ = lv_chart_add_series(chart, lv_color_hex(0x00C985), LV_CHART_AXIS_PRIMARY_Y);

        for (uint32_t i = 0; i < chart_points_.size(); ++i) {
            lv_chart_set_value_by_id(chart, main_series_, i, chart_points_[i]);
        }

        lv_chart_refresh(chart);
        ui_obj_["main_chart"] = chart;

        /*
         * Tabs
         */
        create_label(card, "All", 8, 28,
                     &lv_font_montserrat_10, 0xFFFFFF);

        create_label(card, "Gainers", 33, 28,
                     &lv_font_montserrat_10, 0xB7B7BD);

        create_label(card, "Decliners", 78, 28,
                     &lv_font_montserrat_10, 0xB7B7BD);

        /*
         * Active underline
         */
        lv_obj_t *underline = lv_obj_create(card);
        lv_obj_set_size(underline, 13, 2);
        lv_obj_set_pos(underline, 8, 40);
        lv_obj_set_style_bg_color(underline, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(underline, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(underline, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(underline, LV_OBJ_FLAG_SCROLLABLE);

        create_asset_row(card, 8, 45,
                         "AAPL", "Apple Inc.",
                         "$ 128.34", "+5.25%", 0x38D893);

        create_asset_row(card, 8, 62,
                         "AMZN", "Amazon.com Inc.",
                         "$ 2345.45", "+1.55%", 0x38D893);

        create_asset_row(card, 8, 79,
                         "MRNA", "Moderna Inc.",
                         "$ 435.36", "-1.15%", 0xFF5B6E);

        create_asset_row(card, 8, 96,
                         "AZN", "AstraZeneca PLC",
                         "$ 128.34", "+1.12%", 0x38D893);
    }

    void create_asset_row(lv_obj_t *parent,
                          int x,
                          int y,
                          const char *symbol,
                          const char *name,
                          const char *price,
                          const char *change,
                          uint32_t change_color)
    {
        lv_obj_t *lbl_symbol = create_label(parent, symbol, x, y,
                                            &lv_font_montserrat_10, 0xFFFFFF);
        lv_obj_set_width(lbl_symbol, 32);

        lv_obj_t *lbl_name = create_label(parent, name, x, y + 9,
                                          &lv_font_montserrat_10, 0x77777E);
        lv_obj_set_width(lbl_name, 62);

        /*
         * 小型绿色趋势线
         */
        lv_obj_t *spark = lv_chart_create(parent);
        lv_obj_set_size(spark, 40, 14);
        lv_obj_set_pos(spark, 77, y + 2);

        lv_obj_set_style_bg_opa(spark, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(spark, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(spark, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_width(spark, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_chart_set_type(spark, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(spark, 7);
        lv_chart_set_range(spark, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
        lv_chart_set_div_line_count(spark, 0, 0);

        lv_obj_set_style_size(spark, 0, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);

        lv_chart_series_t *s =
            lv_chart_add_series(spark, lv_color_hex(change_color), LV_CHART_AXIS_PRIMARY_Y);

        int base[7] = {35, 45, 40, 58, 50, 70, 64};

        if (change_color == 0xFF5B6E) {
            int down[7] = {70, 62, 65, 48, 55, 42, 35};
            for (int i = 0; i < 7; ++i) {
                lv_chart_set_value_by_id(spark, s, i, down[i]);
            }
        } else {
            for (int i = 0; i < 7; ++i) {
                lv_chart_set_value_by_id(spark, s, i, base[i]);
            }
        }

        lv_chart_refresh(spark);

        lv_obj_t *lbl_price = create_label(parent, price, 119, y,
                                           &lv_font_montserrat_10, 0xFFFFFF);
        lv_obj_set_width(lbl_price, 34);
        lv_obj_set_style_text_align(lbl_price, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_change = create_label(parent, change, 119, y + 9,
                                            &lv_font_montserrat_10, change_color);
        lv_obj_set_width(lbl_change, 34);
        lv_obj_set_style_text_align(lbl_change, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
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
            refresh_timer_ = lv_timer_create(refresh_timer_cb, 3000, this);
        }
    }

    static void refresh_timer_cb(lv_timer_t *timer)
    {
        UIStockPage *self =
            static_cast<UIStockPage *>(lv_timer_get_user_data(timer));

        if (self) {
            self->on_refresh_timer();
        }
    }

    void on_refresh_timer()
    {
        /*
         * 模拟数值轻微变化
         */
        float delta = ((rand() % 200) - 100) / 100.0f;
        current_value_ += delta;

        if (current_value_ < 8000.0f) current_value_ = 8000.0f;
        if (current_value_ > 9500.0f) current_value_ = 9500.0f;

        profit_value_ = current_value_;

        char buf[32];

        snprintf(buf, sizeof(buf), "$ %.2f", current_value_);
        lv_label_set_text(ui_obj_["lbl_current_value"], buf);

        snprintf(buf, sizeof(buf), "$ %.2f", profit_value_);
        lv_label_set_text(ui_obj_["lbl_profit_value"], buf);

        update_main_chart();
    }

    void update_main_chart()
    {
        if (!ui_obj_.count("main_chart")) return;
        if (!main_series_) return;

        lv_obj_t *chart = ui_obj_["main_chart"];

        int next = 40 + rand() % 45;

        if (!chart_points_.empty()) {
            chart_points_.erase(chart_points_.begin());
            chart_points_.push_back(next);
        }

        for (uint32_t i = 0; i < chart_points_.size(); ++i) {
            lv_chart_set_value_by_id(chart, main_series_, i, chart_points_[i]);
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
        lv_obj_add_event_cb(ui_root,
                            UIStockPage::static_lvgl_handler,
                            LV_EVENT_ALL,
                            this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIStockPage *self =
            static_cast<UIStockPage *>(lv_event_get_user_data(e));

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