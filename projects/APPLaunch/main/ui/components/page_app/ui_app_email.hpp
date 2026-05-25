#pragma once
#include "../ui_app_page.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include "compat/input_keys.h"

// ============================================================
//  邮件客户端界面  UIEmailPage
//  屏幕分辨率: 320 x 170  (顶栏20px, ui_APP_Container 320x150)
//
//  视图状态:
//    VIEW_INBOX   — 收件箱列表
//    VIEW_DETAIL  — 邮件详情
// ============================================================

class UIEmailPage : public app_base
{
    enum class ViewState { INBOX, DETAIL };

    struct EmailItem
    {
        const char *from;
        const char *to;
        const char *date;
        const char *subject;
        const char *body;
    };

public:
    UIEmailPage() : app_base()
    {
        set_page_title("Email");
        email_init();
        creat_UI();
        event_handler_init();
    }
    ~UIEmailPage() {}

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    std::vector<EmailItem> emails_;
    int selected_idx_ = 0;
    ViewState view_state_ = ViewState::INBOX;

    static constexpr int ITEM_H       = 28;
    static constexpr int LIST_Y       = 22;
    static constexpr int LIST_H       = 128;

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

    // ==================== email data ====================
    void email_init()
    {
        emails_.push_back({
            "alice@example.com",
            "me@cardputer.local",
            "Apr 28, 2026",
            "Weekly Team Sync Notes",
            "Hi,\n\nHere are the notes from today's sync:\n\n"
            "1. Backend API v2 is on track for Friday.\n"
            "2. The UI redesign mockups are ready for\n"
            "   review in Figma.\n"
            "3. QA will start regression testing Monday.\n\n"
            "Let me know if anything is missing.\n\n"
            "Best,\nAlice"
        });
        emails_.push_back({
            "bob@devops.io",
            "me@cardputer.local",
            "Apr 27, 2026",
            "Server Maintenance Window",
            "Team,\n\nPlanned maintenance this Saturday\n"
            "from 02:00 to 06:00 UTC.\n\n"
            "Services affected:\n"
            "- Primary database cluster\n"
            "- CDN cache purge\n"
            "- CI/CD pipelines\n\n"
            "No action needed from your side.\n\n"
            "Regards,\nBob"
        });
        emails_.push_back({
            "carol@design.co",
            "me@cardputer.local",
            "Apr 26, 2026",
            "New Brand Assets Available",
            "Hello,\n\nThe updated brand kit is now live\n"
            "in the shared drive. Includes:\n\n"
            "- New logo variants (SVG + PNG)\n"
            "- Updated color palette\n"
            "- Typography guidelines\n\n"
            "Please migrate all materials by May 10.\n\n"
            "Thanks,\nCarol"
        });
        emails_.push_back({
            "dave@security.org",
            "me@cardputer.local",
            "Apr 25, 2026",
            "Security Audit Report Q1",
            "Hi,\n\nAttached is the Q1 security audit.\n\n"
            "Summary:\n"
            "- 3 critical findings resolved\n"
            "- 7 medium findings in progress\n"
            "- 2 low findings deferred to Q2\n\n"
            "Full report is in the shared folder.\n\n"
            "Best,\nDave"
        });
        emails_.push_back({
            "eve@newsletter.com",
            "me@cardputer.local",
            "Apr 24, 2026",
            "Tech Weekly: RISC-V Updates",
            "This week in tech:\n\n"
            "- RISC-V gains momentum in embedded\n"
            "  markets with new SoC announcements.\n"
            "- LVGL 9.2 released with improved\n"
            "  performance on low-memory targets.\n"
            "- ESP32-S3 gets official Zephyr support.\n\n"
            "Read more at newsletter.com/issue/412"
        });
        emails_.push_back({
            "frank@shipping.biz",
            "me@cardputer.local",
            "Apr 23, 2026",
            "Your Order Has Shipped!",
            "Great news!\n\nYour order #M5-20260423 has\n"
            "been shipped via express delivery.\n\n"
            "Tracking: ZX9876543210\n"
            "Estimated arrival: Apr 29, 2026\n\n"
            "Items: M5Stack Cardputer x1\n\n"
            "Thank you for your purchase!\n"
            "- Frank @ Shipping Dept"
        });
    }

    // ==================== UI build (inbox) ====================
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
        lv_label_set_text(lbl_title, LV_SYMBOL_ENVELOPE "  e-Mail");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, "UP/DN:select  OK:open  ESC:back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // list container
        lv_obj_t *list_cont = lv_obj_create(bg);
        lv_obj_set_size(list_cont, 320, LIST_H);
        lv_obj_set_pos(list_cont, 0, LIST_Y);
        lv_obj_set_style_radius(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["list_cont"] = list_cont;

        build_inbox_rows();
    }

    // ==================== build inbox rows ====================
    void build_inbox_rows()
    {
        lv_obj_t *list_cont = ui_obj_["list_cont"];
        lv_obj_clean(list_cont);

        int item_count = (int)emails_.size();
        int visible = LIST_H / ITEM_H;
        int offset_idx = selected_idx_ - visible / 2;
        if (offset_idx < 0) offset_idx = 0;
        if (offset_idx > item_count - visible) offset_idx = item_count - visible;
        if (offset_idx < 0) offset_idx = 0;

        for (int vi = 0; vi < visible && (vi + offset_idx) < item_count; ++vi) {
            int ei = vi + offset_idx;
            bool is_sel = (ei == selected_idx_);
            create_inbox_row(list_cont, vi, ei, is_sel);
        }
    }

    void create_inbox_row(lv_obj_t *parent, int visual_row, int email_idx, bool selected)
    {
        const EmailItem &email = emails_[email_idx];

        // row background
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
            // selection bar
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

        // divider
        if (email_idx < (int)emails_.size() - 1) {
            lv_obj_t *div = lv_obj_create(parent);
            lv_obj_set_size(div, 310, 1);
            lv_obj_set_pos(div, 5, (visual_row + 1) * ITEM_H);
            lv_obj_set_style_radius(div, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(div, lv_color_hex(0x21262D), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(div, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(div, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
        }

        // from (sender name, extract before @)
        char from_short[32];
        const char *at = strchr(email.from, '@');
        if (at) {
            int len = (int)(at - email.from);
            if (len > 30) len = 30;
            memcpy(from_short, email.from, len);
            from_short[len] = '\0';
        } else {
            snprintf(from_short, sizeof(from_short), "%s", email.from);
        }

        lv_obj_t *lbl_from = lv_label_create(row);
        lv_label_set_text(lbl_from, from_short);
        lv_obj_set_pos(lbl_from, 8, 1);
        lv_obj_set_width(lbl_from, 60);
        lv_label_set_long_mode(lbl_from, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_from,
            lv_color_hex(selected ? 0x58A6FF : 0x8AABCF),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_from, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // subject
        lv_obj_t *lbl_subj = lv_label_create(row);
        lv_label_set_text(lbl_subj, email.subject);
        lv_obj_set_pos(lbl_subj, 8, 13);
        lv_obj_set_width(lbl_subj, 230);
        lv_label_set_long_mode(lbl_subj, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_subj,
            lv_color_hex(selected ? 0xFFFFFF : 0xCCCCCC),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_subj, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // date (right-aligned)
        lv_obj_t *lbl_date = lv_label_create(row);
        lv_label_set_text(lbl_date, email.date);
        lv_obj_set_pos(lbl_date, 246, 1);
        lv_obj_set_width(lbl_date, 70);
        lv_label_set_long_mode(lbl_date, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_date,
            lv_color_hex(selected ? 0x7EA8D8 : 0x555555),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // arrow
        lv_obj_t *lbl_arrow = lv_label_create(row);
        lv_label_set_text(lbl_arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_pos(lbl_arrow, 302, (ITEM_H - 14) / 2 - 1);
        lv_obj_set_style_text_color(lbl_arrow,
            lv_color_hex(selected ? 0x58A6FF : 0x3A4A5A),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_arrow, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // ==================== open email detail ====================
    void open_detail(int idx)
    {
        if (idx < 0 || idx >= (int)emails_.size()) return;
        view_state_ = ViewState::DETAIL;
        const EmailItem &email = emails_[idx];

        lv_obj_t *panel = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(panel, 320, 150);
        lv_obj_set_pos(panel, 0, 0);
        lv_obj_set_style_radius(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["detail_panel"] = panel;

        // title bar
        lv_obj_t *title_bar = lv_obj_create(panel);
        lv_obj_set_size(title_bar, 320, 22);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title_bar, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_title = lv_label_create(title_bar);
        lv_label_set_text(lbl_title, LV_SYMBOL_ENVELOPE "  e-Mail");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, LV_SYMBOL_LEFT "  ESC: Inbox");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -6);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0xAECBFA), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        // scrollable content area
        lv_obj_t *content = lv_obj_create(panel);
        lv_obj_set_size(content, 316, 124);
        lv_obj_set_pos(content, 2, 24);
        lv_obj_set_style_radius(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_hor(content, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_ver(content, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_scroll_dir(content, LV_DIR_VER);
        lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_width(content, 3, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(content, lv_color_hex(0x1F6FEB), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(content, 200, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(content, 2, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        ui_obj_["detail_content"] = content;

        // email header fields
        int y = 0;
        char buf[256];

        snprintf(buf, sizeof(buf), "From: %s", email.from);
        make_label(content, buf, 0, y, 0x58A6FF, &lv_font_montserrat_10);
        y += 14;

        snprintf(buf, sizeof(buf), "To: %s", email.to);
        make_label(content, buf, 0, y, 0x8AABCF, &lv_font_montserrat_10);
        y += 14;

        snprintf(buf, sizeof(buf), "Date: %s", email.date);
        make_label(content, buf, 0, y, 0x7EA8D8, &lv_font_montserrat_10);
        y += 16;

        // subject (bold/larger)
        lv_obj_t *lbl_subj = make_label(content, email.subject, 0, y, 0xFFFFFF, &lv_font_montserrat_14);
        lv_obj_set_width(lbl_subj, 296);
        lv_label_set_long_mode(lbl_subj, LV_LABEL_LONG_WRAP);
        y += 20;

        // separator
        lv_obj_t *sep = lv_obj_create(content);
        lv_obj_set_size(sep, 296, 1);
        lv_obj_set_pos(sep, 0, y);
        lv_obj_set_style_radius(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x21262D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(sep, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
        y += 6;

        // body text
        lv_obj_t *lbl_body = make_label(content, email.body, 0, y, 0xE6EDF3, &lv_font_montserrat_12);
        lv_obj_set_width(lbl_body, 296);
        lv_label_set_long_mode(lbl_body, LV_LABEL_LONG_WRAP);
    }

    // ==================== close detail ====================
    void close_detail()
    {
        if (ui_obj_.count("detail_panel") && ui_obj_["detail_panel"]) {
            lv_obj_del(ui_obj_["detail_panel"]);
            ui_obj_["detail_panel"]   = nullptr;
            ui_obj_["detail_content"] = nullptr;
        }
        view_state_ = ViewState::INBOX;
    }

    // ==================== event binding ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIEmailPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIEmailPage *self = static_cast<UIEmailPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }

    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e))
        {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            switch (view_state_) {
            case ViewState::INBOX:  handle_inbox_key(key);  break;
            case ViewState::DETAIL: handle_detail_key(key); break;
            }
        }
    }

    // ==================== inbox key handling ====================
    void handle_inbox_key(uint32_t key)
    {
        int count = (int)emails_.size();
        switch (key) {
        case KEY_UP:
            if (selected_idx_ > 0) {
                --selected_idx_;
                build_inbox_rows();
            }
            break;
        case KEY_DOWN:
            if (selected_idx_ < count - 1) {
                ++selected_idx_;
                build_inbox_rows();
            }
            break;
        case KEY_ENTER:
        case KEY_RIGHT:
            open_detail(selected_idx_);
            break;
        case KEY_ESC:
            if (go_back_home) go_back_home();
            break;
        default:
            break;
        }
    }

    // ==================== detail key handling ====================
    void handle_detail_key(uint32_t key)
    {
        lv_obj_t *content = ui_obj_.count("detail_content") ? ui_obj_["detail_content"] : nullptr;
        switch (key) {
        case KEY_UP:
            if (content)
                lv_obj_scroll_by(content, 0, 20, LV_ANIM_ON);
            break;
        case KEY_DOWN:
            if (content)
                lv_obj_scroll_by(content, 0, -20, LV_ANIM_ON);
            break;
        case KEY_ESC:
        case KEY_LEFT:
            close_detail();
            break;
        default:
            break;
        }
    }
};
