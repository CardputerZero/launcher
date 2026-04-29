#pragma once
#include "ui_app_page.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include "compat/input_keys.h"

// ============================================================
//  LoRa MESH  UIMeshPage
//  Screen: 320 x 170  (top bar 20px, ui_APP_Container 320x150)
//
//  View states:
//    VIEW_MAIN       - Main view with status, neighbors, messages
//    VIEW_INPUT      - Message input overlay
//
//  This is a UI placeholder/demo for LoRa MESH networking.
//  Actual LoRa hardware integration requires specific drivers.
// ============================================================

class UIMeshPage : public app_base
{
    enum class ViewState { MAIN, INPUT };

    struct MeshMessage
    {
        std::string timestamp;
        std::string sender;
        std::string text;
    };

    struct MeshNeighbor
    {
        std::string node_id;
        int         rssi;
        int         hops;
        std::string last_seen;
    };

public:
    UIMeshPage() : app_base()
    {
        srand((unsigned)time(nullptr));
        generate_node_id();
        creat_UI();
        event_handler_init();
        // Periodic heartbeat timer (every 8 seconds)
        heartbeat_timer_ = lv_timer_create(heartbeat_timer_cb, 8000, this);
    }
    ~UIMeshPage()
    {
        if (heartbeat_timer_) lv_timer_delete(heartbeat_timer_);
    }

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    ViewState view_state_ = ViewState::MAIN;

    // Node info
    char node_id_[16]    = {};
    int  channel_        = 7;
    int  frequency_mhz_  = 915;
    bool lora_detected_  = false;

    // Neighbors
    std::vector<MeshNeighbor> neighbors_;

    // Messages
    std::vector<MeshMessage> messages_;
    static constexpr int MAX_MESSAGES = 20;

    // Message input
    std::string msg_input_buf_;
    lv_obj_t *msg_input_lbl_   = nullptr;
    lv_obj_t *input_overlay_   = nullptr;

    // Message display area
    lv_obj_t *msg_area_        = nullptr;
    lv_obj_t *neighbor_area_   = nullptr;
    lv_obj_t *status_lbl_      = nullptr;
    lv_obj_t *hint_lbl_        = nullptr;

    // Heartbeat timer
    lv_timer_t *heartbeat_timer_ = nullptr;
    int heartbeat_count_ = 0;

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
        if (key == 52) return '.'; // KEY_DOT
        return 0;
    }

    // ==================== generate random node ID ====================
    void generate_node_id()
    {
        uint32_t r = (uint32_t)rand();
        snprintf(node_id_, sizeof(node_id_), "0x%04X", r & 0xFFFF);
    }

    // ==================== get current time string ====================
    static void get_time_str(char *buf, int sz)
    {
        time_t now = time(nullptr);
        struct tm *t = localtime(&now);
        snprintf(buf, sz, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    }

    // ==================== UI build ====================
    void creat_UI()
    {
        // ---- Background ----
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

        // ---- Title bar ----
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
        lv_label_set_text(lbl_title, LV_SYMBOL_WIFI "  LoRa MESH");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, "S:Send  R:Refresh  ESC:Back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // ---- Left panel: Status + Neighbors (width 158) ----
        lv_obj_t *left_panel = lv_obj_create(bg);
        lv_obj_set_size(left_panel, 158, 116);
        lv_obj_set_pos(left_panel, 2, 24);
        lv_obj_set_style_radius(left_panel, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x161B22), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(left_panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(left_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(left_panel, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["left_panel"] = left_panel;

        // Status
        char status_buf[64];
        snprintf(status_buf, sizeof(status_buf), "LoRa: %s",
                 lora_detected_ ? "Ready" : "Not Detected");
        status_lbl_ = make_label(left_panel, status_buf, 0, 0,
                                 lora_detected_ ? 0x2ECC71 : 0xE74C3C,
                                 &lv_font_montserrat_10);

        // Node info
        char node_buf[64];
        snprintf(node_buf, sizeof(node_buf), "Node: %s", node_id_);
        make_label(left_panel, node_buf, 0, 13, 0x58A6FF, &lv_font_montserrat_10);

        char freq_buf[64];
        snprintf(freq_buf, sizeof(freq_buf), "CH:%d  %dMHz", channel_, frequency_mhz_);
        make_label(left_panel, freq_buf, 0, 26, 0x888888, &lv_font_montserrat_10);

        // Separator
        lv_obj_t *sep = lv_obj_create(left_panel);
        lv_obj_set_size(sep, 146, 1);
        lv_obj_set_pos(sep, 0, 39);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x21262D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(sep, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

        make_label(left_panel, "Neighbors:", 0, 43, 0x7EA8D8, &lv_font_montserrat_10);

        // Neighbor list container
        neighbor_area_ = lv_obj_create(left_panel);
        lv_obj_set_size(neighbor_area_, 148, 56);
        lv_obj_set_pos(neighbor_area_, 0, 55);
        lv_obj_set_style_bg_opa(neighbor_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(neighbor_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(neighbor_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(neighbor_area_, LV_OBJ_FLAG_SCROLLABLE);

        build_neighbor_list();

        // ---- Right panel: Messages (width 154) ----
        lv_obj_t *right_panel = lv_obj_create(bg);
        lv_obj_set_size(right_panel, 156, 116);
        lv_obj_set_pos(right_panel, 162, 24);
        lv_obj_set_style_radius(right_panel, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(right_panel, lv_color_hex(0x161B22), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(right_panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(right_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(right_panel, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["right_panel"] = right_panel;

        make_label(right_panel, "Messages:", 0, 0, 0x7EA8D8, &lv_font_montserrat_10);

        msg_area_ = lv_obj_create(right_panel);
        lv_obj_set_size(msg_area_, 146, 96);
        lv_obj_set_pos(msg_area_, 0, 12);
        lv_obj_set_style_bg_opa(msg_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(msg_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(msg_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_scroll_dir(msg_area_, LV_DIR_VER);
        lv_obj_add_flag(msg_area_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(msg_area_, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_width(msg_area_, 2, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(msg_area_, lv_color_hex(0x1F6FEB), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(msg_area_, 200, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

        build_message_list();

        // ---- Bottom hint bar ----
        hint_lbl_ = make_label(bg, "S:Send  R:Refresh  ESC:Back", 6, 142, 0x555555, &lv_font_montserrat_10);
    }

    // ==================== build neighbor list ====================
    void build_neighbor_list()
    {
        if (!neighbor_area_) return;
        lv_obj_clean(neighbor_area_);

        if (neighbors_.empty()) {
            make_label(neighbor_area_, "No neighbors found", 0, 4, 0x555555, &lv_font_montserrat_10);
            return;
        }

        int y = 0;
        for (size_t i = 0; i < neighbors_.size() && i < 4; ++i) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s  %ddBm  %dhop",
                     neighbors_[i].node_id.c_str(),
                     neighbors_[i].rssi,
                     neighbors_[i].hops);
            make_label(neighbor_area_, buf, 0, y, 0xCCCCCC, &lv_font_montserrat_10);
            y += 13;
        }
    }

    // ==================== build message list ====================
    void build_message_list()
    {
        if (!msg_area_) return;
        lv_obj_clean(msg_area_);

        if (messages_.empty()) {
            make_label(msg_area_, "MESH messages will", 0, 4, 0x555555, &lv_font_montserrat_10);
            make_label(msg_area_, "appear here", 0, 17, 0x555555, &lv_font_montserrat_10);
            return;
        }

        int y = 0;
        // Show most recent messages (bottom of list)
        int start = (int)messages_.size() > 7 ? (int)messages_.size() - 7 : 0;
        for (int i = start; i < (int)messages_.size(); ++i) {
            // Timestamp + sender
            char header[64];
            snprintf(header, sizeof(header), "[%s] %s:",
                     messages_[i].timestamp.c_str(),
                     messages_[i].sender.c_str());

            uint32_t hdr_color = (messages_[i].sender == "ME") ? 0x58A6FF : 0x2ECC71;
            make_label(msg_area_, header, 0, y, hdr_color, &lv_font_montserrat_10);
            y += 12;

            // Message text (truncate)
            std::string text = messages_[i].text;
            if (text.size() > 22)
                text = text.substr(0, 22) + "..";
            make_label(msg_area_, text.c_str(), 4, y, 0xE6EDF3, &lv_font_montserrat_10);
            y += 14;
        }
    }

    // ==================== add a message ====================
    void add_message(const char *sender, const char *text)
    {
        MeshMessage msg;
        char ts[16];
        get_time_str(ts, sizeof(ts));
        msg.timestamp = ts;
        msg.sender = sender;
        msg.text = text;
        messages_.push_back(msg);
        if ((int)messages_.size() > MAX_MESSAGES)
            messages_.erase(messages_.begin());
        build_message_list();
    }

    // ==================== show message input overlay ====================
    void show_input_overlay()
    {
        view_state_ = ViewState::INPUT;
        msg_input_buf_.clear();

        lv_obj_t *bg = ui_obj_["bg"];

        input_overlay_ = lv_obj_create(bg);
        lv_obj_set_size(input_overlay_, 280, 50);
        lv_obj_set_pos(input_overlay_, 20, 50);
        lv_obj_set_style_radius(input_overlay_, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(input_overlay_, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(input_overlay_, 240, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(input_overlay_, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(input_overlay_, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(input_overlay_, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(input_overlay_, LV_OBJ_FLAG_SCROLLABLE);

        make_label(input_overlay_, "Send MESH Message:", 0, 0, 0x58A6FF, &lv_font_montserrat_10);

        msg_input_lbl_ = make_label(input_overlay_, "_", 0, 14, 0xFFFFFF, &lv_font_montserrat_12);
        lv_obj_set_width(msg_input_lbl_, 260);
        lv_label_set_long_mode(msg_input_lbl_, LV_LABEL_LONG_CLIP);

        make_label(input_overlay_, "ENTER:send  ESC:cancel", 0, 30, 0x888888, &lv_font_montserrat_10);
    }

    // ==================== close input overlay ====================
    void close_input_overlay()
    {
        if (input_overlay_) {
            lv_obj_del(input_overlay_);
            input_overlay_ = nullptr;
        }
        msg_input_lbl_ = nullptr;
        view_state_ = ViewState::MAIN;
    }

    // ==================== update input display ====================
    void update_input_display()
    {
        if (!msg_input_lbl_) return;
        std::string display = msg_input_buf_ + "_";
        lv_label_set_text(msg_input_lbl_, display.c_str());
    }

    // ==================== simulate refresh ====================
    void do_refresh()
    {
        // Simulate scanning for neighbors with random data
        neighbors_.clear();
        int n = rand() % 4; // 0-3 neighbors
        for (int i = 0; i < n; ++i) {
            MeshNeighbor nb;
            char id[16];
            snprintf(id, sizeof(id), "0x%04X", (uint32_t)(rand() & 0xFFFF));
            nb.node_id = id;
            nb.rssi = -(40 + rand() % 60); // -40 to -99 dBm
            nb.hops = 1 + rand() % 3;
            char ts[16];
            get_time_str(ts, sizeof(ts));
            nb.last_seen = ts;
            neighbors_.push_back(nb);
        }
        build_neighbor_list();

        // Add a system message
        if (n > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Scan: %d node(s) found", n);
            add_message("SYS", buf);
        } else {
            add_message("SYS", "Scan: no nodes nearby");
        }
    }

    // ==================== heartbeat timer ====================
    static void heartbeat_timer_cb(lv_timer_t *timer)
    {
        UIMeshPage *self = static_cast<UIMeshPage *>(lv_timer_get_user_data(timer));
        if (self) self->on_heartbeat();
    }

    void on_heartbeat()
    {
        heartbeat_count_++;

        // Every other heartbeat, simulate a received message or status
        if (heartbeat_count_ % 2 == 0 && !neighbors_.empty()) {
            int idx = rand() % (int)neighbors_.size();
            const char *msgs[] = {
                "PING",
                "ACK",
                "HELLO",
                "STATUS:OK",
                "HEARTBEAT"
            };
            int mi = rand() % 5;
            add_message(neighbors_[idx].node_id.c_str(), msgs[mi]);
        }

        // Periodically update RSSI of neighbors
        for (auto &nb : neighbors_) {
            nb.rssi += (rand() % 7) - 3; // fluctuate +/- 3
            if (nb.rssi > -30) nb.rssi = -30;
            if (nb.rssi < -100) nb.rssi = -100;
        }
        if (!neighbors_.empty())
            build_neighbor_list();
    }

    // ==================== event binding ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIMeshPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }
    static void static_lvgl_handler(lv_event_t *e)
    {
        UIMeshPage *self = static_cast<UIMeshPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }
    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e))
        {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            switch (view_state_)
            {
            case ViewState::MAIN:  handle_main_key(key); break;
            case ViewState::INPUT: handle_input_key(key); break;
            }
        }
    }

    // ==================== main view keys ====================
    void handle_main_key(uint32_t key)
    {
        switch (key)
        {
        case KEY_S:
            show_input_overlay();
            break;

        case KEY_R:
            do_refresh();
            break;

        case KEY_UP:
            if (msg_area_)
                lv_obj_scroll_by(msg_area_, 0, 20, LV_ANIM_ON);
            break;

        case KEY_DOWN:
            if (msg_area_)
                lv_obj_scroll_by(msg_area_, 0, -20, LV_ANIM_ON);
            break;

        case KEY_ESC:
            if (heartbeat_timer_) {
                lv_timer_delete(heartbeat_timer_);
                heartbeat_timer_ = nullptr;
            }
            if (go_back_home) go_back_home();
            break;

        default:
            break;
        }
    }

    // ==================== input overlay keys ====================
    void handle_input_key(uint32_t key)
    {
        if (key == KEY_ESC) {
            close_input_overlay();
            return;
        }

        if (key == KEY_ENTER) {
            if (!msg_input_buf_.empty()) {
                add_message("ME", msg_input_buf_.c_str());
            }
            close_input_overlay();
            return;
        }

        if (key == KEY_BACKSPACE) {
            if (!msg_input_buf_.empty())
                msg_input_buf_.pop_back();
            update_input_display();
            return;
        }

        char ch = keycode_to_char(key);
        if (ch) {
            if (msg_input_buf_.size() < 40) {
                msg_input_buf_ += ch;
                update_input_display();
            }
        }
    }
};
