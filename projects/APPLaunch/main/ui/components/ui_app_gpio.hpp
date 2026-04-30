#pragma once

#include "ui_app_page.hpp"
#include "compat/input_keys.h"

#include <unordered_map>
#include <string>
#include <vector>
#include <cstdio>

// ============================================================
//  GPIO 控制界面 UIGpioPage
//  屏幕分辨率: 320 x 170
//
//  注意:
//    本页面不绘制图片中的顶部状态栏。
//    页面内容绘制在 ui_APP_Container 内，尺寸按 320 x 150 处理。
//
//  操作:
//    FUNC 区:
//      UP / DOWN     : 选择功能
//      RIGHT         : 切换到 PIN 区
//      ENTER         : 应用当前功能
//      ESC           : 返回主页
//
//    PIN 区:
//      UP/DOWN/LEFT/RIGHT : 选择引脚
//      ENTER              : 回到 FUNC 区
//      DOWN 到底部        : 切换到 PWM 参数区
//      ESC                : 返回主页
//
//    PWM 参数区:
//      UP / DOWN     : 选择 FREQ / DUTY
//      LEFT / RIGHT  : 调整数值
//      ENTER         : 应用 PWM
//      ESC           : 返回主页
//
//  新增:
//    G26 / G23 / G22 三个引脚会闪烁。
// ============================================================

class UIGpioPage : public app_base
{
private:
    enum class FocusZone
    {
        FUNC,
        PIN,
        VALUE
    };

    struct PinItem
    {
        const char *name;
        bool is_gpio;
        bool is_power;
        uint32_t bg_color;
        uint32_t border_color;
    };

public:
    UIGpioPage() : app_base()
    {
        gpio_init();
        creat_UI();
        event_handler_init();

        // 启动 G26 / G23 / G22 闪烁定时器
        // 每 500ms 翻转一次状态
        blink_timer_ = lv_timer_create(UIGpioPage::static_blink_timer_cb, 500, this);
    }

    ~UIGpioPage()
    {
        if (blink_timer_) {
            lv_timer_del(blink_timer_);
            blink_timer_ = nullptr;
        }
    }

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    std::vector<PinItem> pins_;

    FocusZone focus_zone_ = FocusZone::FUNC;

    int selected_pin_   = 1;   // 默认 G23
    int selected_func_  = 3;   // 默认 PWM
    int selected_value_ = 0;   // 0: FREQ, 1: DUTY

    int pwm_freq_ = 10000;
    int pwm_duty_ = 50;

    // G26 / G23 / G22 闪烁相关
    lv_timer_t *blink_timer_ = nullptr;
    bool blink_on_ = false;

    // ui_APP_Container: 320 x 150
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 150;

    // 左侧功能面板
    static constexpr int LEFT_X = 6;
    static constexpr int LEFT_Y = 5;
    static constexpr int LEFT_W = 105;
    static constexpr int LEFT_H = 106;

    // 右侧引脚矩阵
    static constexpr int PIN_PANEL_X = 115;
    static constexpr int PIN_PANEL_Y = 5;
    static constexpr int PIN_PANEL_W = 200;
    static constexpr int PIN_PANEL_H = 50;

    // 右侧 PWM 参数面板
    static constexpr int PWM_PANEL_X = 115;
    static constexpr int PWM_PANEL_Y = 60;
    static constexpr int PWM_PANEL_W = 200;
    static constexpr int PWM_PANEL_H = 55;

    static constexpr int BOTTOM_Y = 122;

private:
    // ==================== helper ====================

    static lv_obj_t *make_label(lv_obj_t *parent,
                                const char *text,
                                int x,
                                int y,
                                uint32_t color = 0xFFFFFF,
                                const lv_font_t *font = &lv_font_montserrat_12)
    {
        lv_obj_t *lbl = lv_label_create(parent);
        lv_label_set_text(lbl, text);
        lv_obj_set_pos(lbl, x, y);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN | LV_STATE_DEFAULT);
        return lbl;
    }

    static lv_obj_t *make_panel(lv_obj_t *parent,
                                int x,
                                int y,
                                int w,
                                int h,
                                uint32_t border_color = 0x2F9BFF,
                                uint32_t bg_color = 0x000000)
    {
        lv_obj_t *obj = lv_obj_create(parent);
        lv_obj_set_size(obj, w, h);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(obj, lv_color_hex(bg_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj, lv_color_hex(border_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        return obj;
    }

    // 判断是否是需要闪烁的引脚
    static bool is_blink_pin(int idx)
    {
        // pins_ 中:
        // 0 = G26
        // 1 = G23
        // 2 = G22
        return idx == 0 || idx == 1 || idx == 2;
    }

    // ==================== blink timer ====================

    static void static_blink_timer_cb(lv_timer_t *timer)
    {
        UIGpioPage *self = static_cast<UIGpioPage *>(lv_timer_get_user_data(timer));
        if (self) {
            self->blink_timer_cb();
        }
    }

    void blink_timer_cb()
    {
        blink_on_ = !blink_on_;

        // 如果需要真实 GPIO 闪烁，可以在这里同步输出
        apply_blink_gpio(blink_on_);

        // 刷新 UI，实现 G26/G23/G22 格子闪烁
        refresh_UI();
    }

    void apply_blink_gpio(bool on)
    {
        // =====================================================
        // 这里接入真实 GPIO 输出逻辑。
        //
        // 例如 ESP-IDF 伪代码:
        //
        // gpio_set_level(GPIO_NUM_26, on ? 1 : 0);
        // gpio_set_level(GPIO_NUM_23, on ? 1 : 0);
        // gpio_set_level(GPIO_NUM_22, on ? 1 : 0);
        //
        // 注意:
        // 真实 GPIO 需要提前配置为输出模式。
        // =====================================================

        (void)on;
    }

    // ==================== data ====================

    void gpio_init()
    {
        pins_.push_back({"G26",    true,  false, 0x31D843, 0x0B5E18});
        pins_.push_back({"G23",    true,  false, 0x31D843, 0x0B5E18});
        pins_.push_back({"G22",    true,  false, 0x31D843, 0x0B5E18});
        pins_.push_back({"G11",    true,  false, 0x12364B, 0x2F9BFF});
        pins_.push_back({"G10",    true,  false, 0x12364B, 0x2F9BFF});
        pins_.push_back({"G9",     true,  false, 0x12364B, 0x2F9BFF});
        pins_.push_back({"G7",     true,  false, 0x12364B, 0x2F9BFF});

        pins_.push_back({"5VI",    false, true,  0x7B123C, 0xE84B8A});
        pins_.push_back({"GND",    false, true,  0x243044, 0x6AA9FF});
        pins_.push_back({"5VO",    false, true,  0x7B123C, 0xE84B8A});
        pins_.push_back({"G2",     true,  false, 0x12364B, 0x2F9BFF});
        pins_.push_back({"G3",     true,  false, 0x12364B, 0x2F9BFF});
        pins_.push_back({"G15",    true,  false, 0x12364B, 0x2F9BFF});
        pins_.push_back({"G14",    true,  false, 0x12364B, 0x2F9BFF});
    }

    // ==================== UI build ====================

    void creat_UI()
    {
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, SCREEN_W, SCREEN_H);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

        ui_obj_["bg"] = bg;

        refresh_UI();
    }

    void refresh_UI()
    {
        lv_obj_t *bg = ui_obj_["bg"];
        if (!bg) return;

        lv_obj_clean(bg);

        draw_left_func_panel(bg);
        draw_pin_matrix(bg);
        draw_pwm_panel(bg);
        draw_bottom_hint(bg);
    }

    // ==================== left function panel ====================

    void draw_left_func_panel(lv_obj_t *parent)
    {
        lv_obj_t *panel = make_panel(parent, LEFT_X, LEFT_Y, LEFT_W, LEFT_H);

        char title[32];
        std::snprintf(title, sizeof(title), "%s FUNC", pins_[selected_pin_].name);

        lv_obj_t *lbl_title = make_label(panel, title, 0, 13, 0xFFFFFF, &lv_font_montserrat_18);
        lv_obj_set_width(lbl_title, LEFT_W);
        lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

        create_func_button(panel, 0, "INPUT",  5,  40, 42, 16);
        create_func_button(panel, 1, "OUTPUT", 53, 40, 42, 16);
        create_func_button(panel, 2, "ADC",    5,  62, 42, 16);
        create_func_button(panel, 3, "PWM",    53, 62, 42, 16);
        create_func_button(panel, 4, "RESET",  5,  81, 42, 16);
    }

    void create_func_button(lv_obj_t *parent,
                            int idx,
                            const char *text,
                            int x,
                            int y,
                            int w,
                            int h)
    {
        bool is_active = idx == selected_func_;
        bool is_focus  = focus_zone_ == FocusZone::FUNC && idx == selected_func_;

        lv_obj_t *btn = lv_obj_create(parent);
        lv_obj_set_size(btn, w, h);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_style_radius(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        if (is_active) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x24C83A), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x064D6B), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        lv_obj_set_style_border_width(btn, is_focus ? 2 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        if (is_focus) {
            lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_border_color(btn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // ==================== pin matrix ====================

    void draw_pin_matrix(lv_obj_t *parent)
    {
        lv_obj_t *panel = make_panel(parent, PIN_PANEL_X, PIN_PANEL_Y, PIN_PANEL_W, PIN_PANEL_H);

        const int cell_w  = 24;
        const int cell_h  = 20;
        const int gap_x   = 4;
        const int gap_y   = 3;
        const int start_x = 2;
        const int start_y = 2;

        for (int i = 0; i < (int)pins_.size(); ++i) {
            int row = i / 7;
            int col = i % 7;

            int x = start_x + col * (cell_w + gap_x);
            int y = start_y + row * (cell_h + gap_y);

            create_pin_cell(panel, i, x, y, cell_w, cell_h);
        }
    }

    void create_pin_cell(lv_obj_t *parent,
                         int idx,
                         int x,
                         int y,
                         int w,
                         int h)
    {
        const PinItem &pin = pins_[idx];

        bool selected = idx == selected_pin_;
        bool focused  = focus_zone_ == FocusZone::PIN && selected;

        uint32_t bg_color = pin.bg_color;
        uint32_t border_color = pin.border_color;

        // G26 / G23 / G22 闪烁效果
        if (is_blink_pin(idx)) {
            if (blink_on_) {
                bg_color = 0xFFD21F;      // 亮黄色
                border_color = 0xFFFFFF;  // 白色边框
            } else {
                bg_color = 0x103018;      // 暗绿色
                border_color = 0x31D843;  // 绿色边框
            }
        }

        lv_obj_t *cell = lv_obj_create(parent);
        lv_obj_set_size(cell, w, h);
        lv_obj_set_pos(cell, x, y);
        lv_obj_set_style_radius(cell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_color(cell, lv_color_hex(bg_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(cell, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_border_width(cell, selected ? 2 : 1, LV_PART_MAIN | LV_STATE_DEFAULT);

        if (focused) {
            // 当前焦点在 PIN 区，并且选中该引脚
            lv_obj_set_style_border_color(cell, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else if (selected) {
            // 当前引脚被选中，但焦点不在 PIN 区
            lv_obj_set_style_border_color(cell, lv_color_hex(0xFFD21F), LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            // 普通状态
            lv_obj_set_style_border_color(cell, lv_color_hex(border_color), LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        lv_obj_set_style_pad_all(cell, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(cell);
        lv_label_set_text(lbl, pin.name);
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

        if (pin.is_power) {
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_8, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    // ==================== PWM panel ====================

    void draw_pwm_panel(lv_obj_t *parent)
    {
        lv_obj_t *panel = make_panel(parent, PWM_PANEL_X, PWM_PANEL_Y, PWM_PANEL_W, PWM_PANEL_H);

        // 右上绿色状态点
        lv_obj_t *led = lv_obj_create(panel);
        lv_obj_set_size(led, 8, 8);
        lv_obj_set_pos(led, PWM_PANEL_W - 19, 14);
        lv_obj_set_style_radius(led, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(led, lv_color_hex(0x32D74B), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(led, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(led, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(led, LV_OBJ_FLAG_SCROLLABLE);

        // FREQ 文本
        make_label(panel, "FREQ:", 10, 4, 0xFFFFFF, &lv_font_montserrat_18);

        char freq_buf[32];
        std::snprintf(freq_buf, sizeof(freq_buf), "%d Hz", pwm_freq_);

        lv_obj_t *lbl_freq = make_label(panel, freq_buf, 70, 4, 0xFFFFFF, &lv_font_montserrat_18);
        lv_obj_set_style_text_align(lbl_freq, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

        // DUTY 文本
        make_label(panel, "DUTY:", 10, 27, 0xFFFFFF, &lv_font_montserrat_18);

        char duty_buf[32];
        std::snprintf(duty_buf, sizeof(duty_buf), "%d %%", pwm_duty_);

        lv_obj_t *lbl_duty = make_label(panel, duty_buf, 70, 27, 0xFFFFFF, &lv_font_montserrat_18);
        lv_obj_set_style_text_align(lbl_duty, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

        // 绿色横线 1
        lv_obj_t *line1 = lv_obj_create(panel);
        lv_obj_set_size(line1, 120, 1);
        lv_obj_set_pos(line1, 68, 20);
        lv_obj_set_style_radius(line1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(line1, lv_color_hex(0x32D74B), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(line1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(line1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(line1, LV_OBJ_FLAG_SCROLLABLE);

        // 绿色横线 2
        lv_obj_t *line2 = lv_obj_create(panel);
        lv_obj_set_size(line2, 120, 1);
        lv_obj_set_pos(line2, 68, 43);
        lv_obj_set_style_radius(line2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(line2, lv_color_hex(0x32D74B), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(line2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(line2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(line2, LV_OBJ_FLAG_SCROLLABLE);

        // PWM 参数区焦点标记
        if (focus_zone_ == FocusZone::VALUE) {
            int marker_y = selected_value_ == 0 ? 13 : 36;

            lv_obj_t *marker = lv_obj_create(panel);
            lv_obj_set_size(marker, 5, 14);
            lv_obj_set_pos(marker, 12, marker_y);
            lv_obj_set_style_radius(marker, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(marker, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(marker, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(marker, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(marker, LV_OBJ_FLAG_SCROLLABLE);
        }
    }

    // ==================== bottom hint ====================

    void draw_bottom_hint(lv_obj_t *parent)
    {
        lv_obj_t *lbl_menu = make_label(parent, "[MENU]", 23, BOTTOM_Y, 0xFFFFFF, &lv_font_montserrat_18);
        lv_obj_set_width(lbl_menu, 85);
        lv_obj_set_style_text_align(lbl_menu, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_select = make_label(parent, "[SELECT]", 115, BOTTOM_Y, 0xFFFFFF, &lv_font_montserrat_18);
        lv_obj_set_width(lbl_select, 100);
        lv_obj_set_style_text_align(lbl_select, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_back = make_label(parent, "[BACK]", 225, BOTTOM_Y, 0xFFFFFF, &lv_font_montserrat_18);
        lv_obj_set_width(lbl_back, 80);
        lv_obj_set_style_text_align(lbl_back, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // ==================== event binding ====================

    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIGpioPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIGpioPage *self = static_cast<UIGpioPage *>(lv_event_get_user_data(e));
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

    // ==================== key handling ====================

    void handle_key(uint32_t key)
    {
        switch (focus_zone_) {
        case FocusZone::FUNC:
            handle_func_key(key);
            break;

        case FocusZone::PIN:
            handle_pin_key(key);
            break;

        case FocusZone::VALUE:
            handle_value_key(key);
            break;
        }
    }

    void handle_func_key(uint32_t key)
    {
        switch (key) {
        case KEY_UP:
            if (selected_func_ > 0) {
                --selected_func_;
                refresh_UI();
            }
            break;

        case KEY_DOWN:
            if (selected_func_ < 4) {
                ++selected_func_;
                refresh_UI();
            }
            break;

        case KEY_RIGHT:
            focus_zone_ = FocusZone::PIN;
            refresh_UI();
            break;

        case KEY_ENTER:
            apply_func();
            refresh_UI();
            break;

        case KEY_ESC:
            if (go_back_home) {
                go_back_home();
            }
            break;

        default:
            break;
        }
    }

    void handle_pin_key(uint32_t key)
    {
        int row = selected_pin_ / 7;
        int col = selected_pin_ % 7;

        switch (key) {
        case KEY_LEFT:
            if (col > 0) {
                --selected_pin_;
            } else {
                focus_zone_ = FocusZone::FUNC;
            }
            refresh_UI();
            break;

        case KEY_RIGHT:
            if (col < 6 && selected_pin_ + 1 < (int)pins_.size()) {
                ++selected_pin_;
                refresh_UI();
            }
            break;

        case KEY_UP:
            if (row > 0) {
                selected_pin_ -= 7;
                refresh_UI();
            }
            break;

        case KEY_DOWN:
            if (row == 0 && selected_pin_ + 7 < (int)pins_.size()) {
                selected_pin_ += 7;
            } else {
                focus_zone_ = FocusZone::VALUE;
            }
            refresh_UI();
            break;

        case KEY_ENTER:
            focus_zone_ = FocusZone::FUNC;
            refresh_UI();
            break;

        case KEY_ESC:
            if (go_back_home) {
                go_back_home();
            }
            break;

        default:
            break;
        }
    }

    void handle_value_key(uint32_t key)
    {
        switch (key) {
        case KEY_UP:
            if (selected_value_ > 0) {
                selected_value_ = 0;
            } else {
                focus_zone_ = FocusZone::PIN;
            }
            refresh_UI();
            break;

        case KEY_DOWN:
            if (selected_value_ < 1) {
                selected_value_ = 1;
                refresh_UI();
            }
            break;

        case KEY_LEFT:
            adjust_value(-1);
            refresh_UI();
            break;

        case KEY_RIGHT:
            adjust_value(1);
            refresh_UI();
            break;

        case KEY_ENTER:
            apply_pwm();
            refresh_UI();
            break;

        case KEY_ESC:
            if (go_back_home) {
                go_back_home();
            }
            break;

        default:
            break;
        }
    }

    // ==================== actions ====================

    void apply_func()
    {
        // 这里接入真实 GPIO 控制逻辑。
        //
        // selected_pin_:
        //   pins_[selected_pin_].name
        //
        // selected_func_:
        //   0 INPUT
        //   1 OUTPUT
        //   2 ADC
        //   3 PWM
        //   4 RESET

        if (selected_func_ == 3) {
            apply_pwm();
        }
    }

    void apply_pwm()
    {
        // 这里接入真实 PWM 输出逻辑。
        //
        // 示例:
        //   pin:       pins_[selected_pin_].name
        //   frequency: pwm_freq_
        //   duty:      pwm_duty_
    }

    void adjust_value(int dir)
    {
        if (selected_value_ == 0) {
            pwm_freq_ += dir * 1000;

            if (pwm_freq_ < 100) {
                pwm_freq_ = 100;
            }

            if (pwm_freq_ > 50000) {
                pwm_freq_ = 50000;
            }
        } else {
            pwm_duty_ += dir * 5;

            if (pwm_duty_ < 0) {
                pwm_duty_ = 0;
            }

            if (pwm_duty_ > 100) {
                pwm_duty_ = 100;
            }
        }
    }
};