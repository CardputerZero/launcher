#pragma once
#include "ui_app_page.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdlib>
#include "compat/input_keys.h"

// ============================================================
//  聊天界面  UIchatPage
//  屏幕分辨率: 320 x 170 
//
//  功能:
//    - 本地聊天 demo UI
//    - 可滚动消息区域 (绿色气泡=已发送, 灰色气泡=收到)
//    - 底部文字输入区, 键盘输入字符, ENTER 发送
//    - 发送后自动回复 (canned responses via LVGL timer)
//    - BACKSPACE 删除末字符, ESC 返回主页
//    - 原始按键输入（FZXC → 方向键仅在 launcher，app 内用原始字符）
// ============================================================

class UIchatPage : public app_
{
    struct ChatMsg
    {
        std::string text;
        bool is_mine; // true = sent (green right), false = received (gray left)
    };

public:
    UIchatPage() : app_()
    {
        creat_UI();
        event_handler_init();
        // seed a welcome message
        messages_.push_back({"Welcome to Chat!", false});
        rebuild_messages();
    }

    ~UIchatPage()
    {
        if (reply_timer_) {
            lv_timer_delete(reply_timer_);
            reply_timer_ = nullptr;
        }
    }

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    std::vector<ChatMsg> messages_;
    std::string input_buf_;
    lv_timer_t *reply_timer_ = nullptr;

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

    // ==================== keycode to char ====================
    static char keycode_to_char(uint32_t key)
    {
        if (key >= KEY_1 && key <= KEY_9) return '1' + (key - KEY_1);
        if (key == KEY_0) return '0';
        static const char qwerty[] = "qwertyuiop";
        if (key >= KEY_Q && key <= KEY_P) return qwerty[key - KEY_Q];
        static const char asdf[] = "asdfghjkl";
        if (key >= KEY_A && key <= KEY_L) return asdf[key - KEY_A];
        static const char zxcv[] = "zxcvbnm";
        if (key >= KEY_Z && key <= KEY_M) return zxcv[key - KEY_Z];
        if (key == KEY_SPACE) return ' ';
        return 0;
    }

    // ==================== canned auto-reply ====================
    static const char *pick_reply()
    {
        static const char *replies[] = {
            "Got it!",
            "OK",
            "Interesting...",
            "Tell me more!",
            "Sure thing.",
            "Haha, nice!",
            "I see.",
            "Roger that.",
        };
        return replies[rand() % 8];
    }

    // ==================== UI build ====================
    void creat_UI()
    {
        // background
        lv_obj_t *bg = lv_obj_create(ui_root);
        lv_obj_set_size(bg, 320, 170);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        // title bar
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
        lv_label_set_text(lbl_title, LV_SYMBOL_ENVELOPE "  Chat");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, "Type+ENTER:send  ESC:back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // message area (scrollable)
        lv_obj_t *msg_area = lv_obj_create(bg);
        lv_obj_set_size(msg_area, 320, 124);
        lv_obj_set_pos(msg_area, 0, 22);
        lv_obj_set_style_radius(msg_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(msg_area, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(msg_area, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(msg_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(msg_area, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_scroll_dir(msg_area, LV_DIR_VER);
        lv_obj_add_flag(msg_area, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(msg_area, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_width(msg_area, 3, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(msg_area, lv_color_hex(0x1F6FEB), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(msg_area, 200, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(msg_area, 2, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        ui_obj_["msg_area"] = msg_area;

        // separator line above input
        lv_obj_t *sep = lv_obj_create(bg);
        lv_obj_set_size(sep, 320, 1);
        lv_obj_set_pos(sep, 0, 146);
        lv_obj_set_style_radius(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x21262D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(sep, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

        // input area
        lv_obj_t *input_area = lv_obj_create(bg);
        lv_obj_set_size(input_area, 320, 23);
        lv_obj_set_pos(input_area, 0, 147);
        lv_obj_set_style_radius(input_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(input_area, lv_color_hex(0x161B22), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(input_area, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(input_area, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(input_area, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(input_area, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_prompt = lv_label_create(input_area);
        lv_label_set_text(lbl_prompt, ">");
        lv_obj_set_pos(lbl_prompt, 2, 4);
        lv_obj_set_style_text_color(lbl_prompt, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_prompt, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_input = lv_label_create(input_area);
        lv_label_set_text(lbl_input, "_");
        lv_obj_set_pos(lbl_input, 16, 5);
        lv_obj_set_width(lbl_input, 290);
        lv_label_set_long_mode(lbl_input, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_input, lv_color_hex(0xE6EDF3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_input, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["input_lbl"] = lbl_input;
    }

    // ==================== rebuild message bubbles ====================
    void rebuild_messages()
    {
        lv_obj_t *msg_area = ui_obj_["msg_area"];
        if (!msg_area) return;
        lv_obj_clean(msg_area);

        int y_off = 0;
        for (size_t i = 0; i < messages_.size(); ++i) {
            const ChatMsg &msg = messages_[i];

            // bubble dimensions
            int text_w = (int)msg.text.size() * 7; // approximate char width
            if (text_w < 30) text_w = 30;
            if (text_w > 220) text_w = 220;
            int bubble_w = text_w + 12;
            int bubble_h = 18;

            int x_pos = msg.is_mine ? (308 - bubble_w) : 0;
            uint32_t bg_color = msg.is_mine ? 0x238636 : 0x30363D;

            // bubble container
            lv_obj_t *bubble = lv_obj_create(msg_area);
            lv_obj_set_size(bubble, bubble_w, bubble_h);
            lv_obj_set_pos(bubble, x_pos, y_off);
            lv_obj_set_style_radius(bubble, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(bubble, lv_color_hex(bg_color), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(bubble, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(bubble, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_all(bubble, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

            // bubble text
            lv_obj_t *lbl = lv_label_create(bubble);
            lv_label_set_text(lbl, msg.text.c_str());
            lv_obj_set_pos(lbl, 6, 2);
            lv_obj_set_width(lbl, text_w);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xE6EDF3), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

            y_off += bubble_h + 4;
        }

        // scroll to bottom
        lv_obj_scroll_to_y(msg_area, y_off, LV_ANIM_ON);
    }

    // ==================== update input display ====================
    void update_input_display()
    {
        lv_obj_t *lbl = ui_obj_["input_lbl"];
        if (!lbl) return;
        std::string display = input_buf_ + "_";
        lv_label_set_text(lbl, display.c_str());
    }

    // ==================== send message ====================
    void send_message()
    {
        if (input_buf_.empty()) return;
        messages_.push_back({input_buf_, true});
        input_buf_.clear();
        update_input_display();
        rebuild_messages();

        // schedule auto-reply after 800ms
        if (reply_timer_) {
            lv_timer_delete(reply_timer_);
            reply_timer_ = nullptr;
        }
        reply_timer_ = lv_timer_create(reply_timer_cb, 800, this);
        lv_timer_set_repeat_count(reply_timer_, 1);
    }

    static void reply_timer_cb(lv_timer_t *timer)
    {
        UIchatPage *self = static_cast<UIchatPage *>(lv_timer_get_user_data(timer));
        if (self) {
            self->messages_.push_back({pick_reply(), false});
            self->rebuild_messages();
            self->reply_timer_ = nullptr;
        }
    }

    // ==================== event binding ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIchatPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIchatPage *self = static_cast<UIchatPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }

    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e))
        {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            handle_key(key);
        }
    }

    void handle_key(uint32_t key)
    {
        switch (key) {
        case KEY_ESC:
            if (go_back_home) go_back_home();
            return;

        case KEY_ENTER:
            send_message();
            return;

        case KEY_BACKSPACE:
            if (!input_buf_.empty()) {
                input_buf_.pop_back();
                update_input_display();
            }
            return;

        case KEY_UP:
        {
            lv_obj_t *msg_area = ui_obj_["msg_area"];
            if (msg_area)
                lv_obj_scroll_by(msg_area, 0, 20, LV_ANIM_ON);
            return;
        }
        case KEY_DOWN:
        {
            lv_obj_t *msg_area = ui_obj_["msg_area"];
            if (msg_area)
                lv_obj_scroll_by(msg_area, 0, -20, LV_ANIM_ON);
            return;
        }

        default:
            break;
        }

        // try printable character
        char ch = keycode_to_char(key);
        if (ch) {
            input_buf_ += ch;
            update_input_display();
        }
    }
};
