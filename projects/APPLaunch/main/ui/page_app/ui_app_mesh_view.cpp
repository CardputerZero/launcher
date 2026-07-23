#define APP_PAGE_IMPLEMENTATION_UNIT
#include "ui_app_mesh.hpp"

#include <cstdio>

lv_obj_t *UIMeshPage::make_label(lv_obj_t *parent,
                                 const char *text,
                                 int x,
                                 int y,
                                 uint32_t color,
                                 const lv_font_t *font)
{
    if (!parent) return nullptr;
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return nullptr;
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    return label;
}

void UIMeshPage::create_ui()
{
    if (!ui_APP_Container) return;
    lv_obj_t *bg = lv_obj_create(ui_APP_Container);
    if (!bg) return;
    lv_obj_add_event_cb(bg, owned_obj_delete_cb, LV_EVENT_DELETE, this);
    lv_obj_set_size(bg, 320, 150);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    ui_obj_["bg"] = bg;

    lv_obj_t *title_bar = lv_obj_create(bg);
    if (!title_bar) return;
    lv_obj_set_size(title_bar, 320, 22);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(title_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(title_bar, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(title_bar);
    if (title) {
        lv_label_set_text(title, LV_SYMBOL_WIFI "  LoRa MESH");
        lv_obj_set_align(title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_obj_t *title_hint = lv_label_create(title_bar);
    if (title_hint) {
        lv_label_set_text(title_hint, "S:Send  R:Refresh  ESC:Back");
        lv_obj_set_align(title_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(title_hint, -4);
        lv_obj_set_style_text_color(title_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(title_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_obj_t *left_panel = lv_obj_create(bg);
    if (!left_panel) return;
    lv_obj_set_size(left_panel, 158, 116);
    lv_obj_set_pos(left_panel, 2, 24);
    lv_obj_set_style_radius(left_panel, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x161B22), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(left_panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(left_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(left_panel, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);
    ui_obj_["left_panel"] = left_panel;

    char status_buf[64];
    std::snprintf(status_buf, sizeof(status_buf), "LoRa: %s",
                  model_.lora_detected() ? "Ready" : "Not Detected");
    status_lbl_ = make_label(left_panel,
                             status_buf,
                             0,
                             0,
                             model_.lora_detected() ? 0x2ECC71 : 0xE74C3C,
                             &lv_font_montserrat_10);
    if (status_lbl_)
        lv_obj_add_event_cb(status_lbl_, owned_obj_delete_cb, LV_EVENT_DELETE, this);

    char node_buf[64];
    std::snprintf(node_buf, sizeof(node_buf), "Node: %s", model_.node_id().c_str());
    make_label(left_panel, node_buf, 0, 13, 0x58A6FF, &lv_font_montserrat_10);

    char frequency_buf[64];
    std::snprintf(frequency_buf, sizeof(frequency_buf), "CH:%d  %dMHz",
                  model_.channel(), model_.frequency_mhz());
    make_label(left_panel, frequency_buf, 0, 26, 0x888888, &lv_font_montserrat_10);

    lv_obj_t *separator = lv_obj_create(left_panel);
    if (separator) {
        lv_obj_set_size(separator, 146, 1);
        lv_obj_set_pos(separator, 0, 39);
        lv_obj_set_style_bg_color(separator, lv_color_hex(0x21262D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(separator, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(separator, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(separator, LV_OBJ_FLAG_SCROLLABLE);
    }

    make_label(left_panel, "Neighbors:", 0, 43, 0x7EA8D8, &lv_font_montserrat_10);
    neighbor_area_ = lv_obj_create(left_panel);
    if (!neighbor_area_) return;
    lv_obj_add_event_cb(neighbor_area_, owned_obj_delete_cb, LV_EVENT_DELETE, this);
    lv_obj_set_size(neighbor_area_, 148, 56);
    lv_obj_set_pos(neighbor_area_, 0, 55);
    lv_obj_set_style_bg_opa(neighbor_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(neighbor_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(neighbor_area_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(neighbor_area_, LV_OBJ_FLAG_SCROLLABLE);
    build_neighbor_list();

    lv_obj_t *right_panel = lv_obj_create(bg);
    if (!right_panel) return;
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
    if (!msg_area_) return;
    lv_obj_add_event_cb(msg_area_, owned_obj_delete_cb, LV_EVENT_DELETE, this);
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

    hint_lbl_ = make_label(bg, "S:Send  R:Refresh  ESC:Back", 6, 142, 0x555555, &lv_font_montserrat_10);
    if (hint_lbl_)
        lv_obj_add_event_cb(hint_lbl_, owned_obj_delete_cb, LV_EVENT_DELETE, this);
}

void UIMeshPage::build_neighbor_list()
{
    if (!neighbor_area_) return;
    lv_obj_clean(neighbor_area_);

    const auto &neighbors = model_.neighbors();
    if (neighbors.empty()) {
        make_label(neighbor_area_, "No neighbors found", 0, 4, 0x555555, &lv_font_montserrat_10);
        return;
    }

    int y = 0;
    for (size_t i = 0; i < neighbors.size() && i < 4; ++i) {
        char text[64];
        std::snprintf(text,
                      sizeof(text),
                      "%s  %ddBm  %dhop",
                      neighbors[i].node_id.c_str(),
                      neighbors[i].rssi,
                      neighbors[i].hops);
        make_label(neighbor_area_, text, 0, y, 0xCCCCCC, &lv_font_montserrat_10);
        y += 13;
    }
}

void UIMeshPage::build_message_list()
{
    if (!msg_area_) return;
    lv_obj_clean(msg_area_);

    const auto &messages = model_.messages();
    if (messages.empty()) {
        make_label(msg_area_, "MESH messages will", 0, 4, 0x555555, &lv_font_montserrat_10);
        make_label(msg_area_, "appear here", 0, 17, 0x555555, &lv_font_montserrat_10);
        return;
    }

    int y = 0;
    const size_t start = messages.size() > 7 ? messages.size() - 7 : 0;
    for (size_t i = start; i < messages.size(); ++i) {
        char header[64];
        std::snprintf(header,
                      sizeof(header),
                      "[%s] %s:",
                      messages[i].timestamp.c_str(),
                      messages[i].sender.c_str());
        const uint32_t color = messages[i].sender == "ME" ? 0x58A6FF : 0x2ECC71;
        make_label(msg_area_, header, 0, y, color, &lv_font_montserrat_10);
        y += 12;

        std::string text = messages[i].text;
        if (text.size() > 22) text = text.substr(0, 22) + "..";
        make_label(msg_area_, text.c_str(), 4, y, 0xE6EDF3, &lv_font_montserrat_10);
        y += 14;
    }
}

void UIMeshPage::add_message(const char *sender, const char *text)
{
    model_.add_message(current_time(), sender, text);
    build_message_list();
}
