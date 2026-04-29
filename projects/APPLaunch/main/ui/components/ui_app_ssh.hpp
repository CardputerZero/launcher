#pragma once
#include "ui_app_page.hpp"
#include "ui_app_console.hpp"
#include "compat/input_keys.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <functional>

// ============================================================
//  SSH Client  UISSHPage
//  Screen: 320 x 170  (top bar 20px, ui_APP_Container 320x150)
//
//  Views:
//    VIEW_INPUT    -- Host/Port/User input fields
//    VIEW_TERMINAL -- Embedded UIConsolePage running ssh
// ============================================================

class UISSHPage : public app_base
{
    enum class ViewState { INPUT, TERMINAL };

    struct InputField
    {
        const char *label;
        std::string value;
    };

public:
    UISSHPage() : app_base()
    {
        fields_.resize(3);
        fields_[0] = {"Host", "192.168.1.1"};
        fields_[1] = {"Port", "22"};
        fields_[2] = {"User", "pi"};
        creat_UI();
        event_handler_init();
    }

    ~UISSHPage()
    {
        console_page_.reset();
    }

private:
    // ==================== data members ====================
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    std::vector<InputField> fields_;
    int active_field_ = 0;
    ViewState view_state_ = ViewState::INPUT;
    std::shared_ptr<UIConsolePage> console_page_;

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
        // common symbols needed for IP addresses and hostnames
        if (key == 52) return '.';  // KEY_DOT
        if (key == 12) return '-';  // KEY_MINUS
        return 0;
    }

    // ==================== UI construction (input view) ====================
    void creat_UI()
    {
        // ---- background ----
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

        // ---- title bar ----
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
        lv_label_set_text(lbl_title, "SSH Client");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint_title = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint_title, "ENTER:Connect  ESC:Back");
        lv_obj_set_align(lbl_hint_title, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint_title, -4);
        lv_obj_set_style_text_color(lbl_hint_title, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint_title, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // ---- input fields ----
        build_input_fields();
    }

    void build_input_fields()
    {
        lv_obj_t *bg = ui_obj_["bg"];

        // remove old field objects if they exist
        for (int i = 0; i < 3; ++i)
        {
            std::string key_row = "field_row_" + std::to_string(i);
            if (ui_obj_.count(key_row) && ui_obj_[key_row])
            {
                lv_obj_del(ui_obj_[key_row]);
                ui_obj_[key_row] = nullptr;
            }
        }
        if (ui_obj_.count("hint_lbl") && ui_obj_["hint_lbl"])
        {
            lv_obj_del(ui_obj_["hint_lbl"]);
            ui_obj_["hint_lbl"] = nullptr;
        }

        int start_y = 30;
        int field_h = 30;
        int gap = 4;

        for (int i = 0; i < 3; ++i)
        {
            bool active = (i == active_field_);
            int y = start_y + i * (field_h + gap);

            // field container
            lv_obj_t *row = lv_obj_create(bg);
            lv_obj_set_size(row, 300, field_h);
            lv_obj_set_pos(row, 10, y);
            lv_obj_set_style_radius(row, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(row, active ? 1 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(row, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            uint32_t bg_color = active ? 0x1F3A5F : 0x161B22;
            lv_obj_set_style_bg_color(row, lv_color_hex(bg_color), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

            // active indicator bar
            if (active)
            {
                lv_obj_t *bar = lv_obj_create(row);
                lv_obj_set_size(bar, 3, field_h - 8);
                lv_obj_set_pos(bar, 2, 3);
                lv_obj_set_style_radius(bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(bar, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
            }

            // label (field name)
            lv_obj_t *lbl = lv_label_create(row);
            lv_label_set_text(lbl, fields_[i].label);
            lv_obj_set_pos(lbl, 10, (field_h - 14) / 2);
            lv_obj_set_style_text_color(lbl,
                active ? lv_color_hex(0x58A6FF) : lv_color_hex(0x7EA8D8),
                LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

            // value (editable text)
            lv_obj_t *val_lbl = lv_label_create(row);
            std::string display = fields_[i].value;
            if (active) display += "_";
            lv_label_set_text(val_lbl, display.c_str());
            lv_obj_set_pos(val_lbl, 60, (field_h - 14) / 2);
            lv_obj_set_width(val_lbl, 228);
            lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_color(val_lbl,
                active ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xCCCCCC),
                LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

            std::string key_row = "field_row_" + std::to_string(i);
            ui_obj_[key_row] = row;
        }

        // hint at bottom
        lv_obj_t *lbl_bottom = lv_label_create(bg);
        lv_label_set_text(lbl_bottom, "UP/DN:field  Type to edit  ENTER:Connect  ESC:Back");
        lv_obj_set_pos(lbl_bottom, 10, 135);
        lv_obj_set_width(lbl_bottom, 300);
        lv_label_set_long_mode(lbl_bottom, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_bottom, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_bottom, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["hint_lbl"] = lbl_bottom;
    }

    // ==================== connect via SSH ====================
    void do_connect()
    {
        std::string host = fields_[0].value;
        std::string port = fields_[1].value;
        std::string user = fields_[2].value;

        if (host.empty()) return;

        // Build ssh command
        std::string cmd = "ssh -o StrictHostKeyChecking=no " + user + "@" + host;
        if (!port.empty() && port != "22")
            cmd += " -p " + port;

        printf("[SSH] Launching: %s\n", cmd.c_str());

        // Create console page
        console_page_ = std::make_shared<UIConsolePage>();

        // Save our own go_back_home so we can restore the input view
        auto self_go_home = this->go_back_home;
        console_page_->go_back_home = [this, self_go_home]() {
            // Return to the SSH input view
            console_page_.reset();
            // Switch screen back to our root
            lv_disp_load_scr(this->get_ui());
            lv_indev_set_group(lv_indev_get_next(NULL), this->get_key_group());
        };

        // Switch to console screen
        view_state_ = ViewState::TERMINAL;
        lv_disp_load_scr(console_page_->get_ui());
        lv_indev_set_group(lv_indev_get_next(NULL), console_page_->get_key_group());

        // Launch ssh command
        console_page_->exec(cmd);
    }

    // ==================== event binding ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UISSHPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UISSHPage *self = static_cast<UISSHPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }

    void event_handler(lv_event_t *e)
    {
        // Only handle input view events; terminal view is handled by UIConsolePage
        if (view_state_ != ViewState::INPUT) return;

        if (IS_KEY_RELEASED(e))
        {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);

            switch (key)
            {
            case KEY_UP:
                if (active_field_ > 0)
                {
                    --active_field_;
                    build_input_fields();
                }
                break;

            case KEY_DOWN:
                if (active_field_ < 2)
                {
                    ++active_field_;
                    build_input_fields();
                }
                break;

            case KEY_ENTER:
                do_connect();
                break;

            case KEY_ESC:
                if (go_back_home) go_back_home();
                break;

            case KEY_BACKSPACE:
                if (!fields_[active_field_].value.empty())
                {
                    fields_[active_field_].value.pop_back();
                    build_input_fields();
                }
                break;

            default:
            {
                char ch = keycode_to_char(key);
                if (ch)
                {
                    fields_[active_field_].value += ch;
                    build_input_fields();
                }
                break;
            }
            }
        }
    }
};
