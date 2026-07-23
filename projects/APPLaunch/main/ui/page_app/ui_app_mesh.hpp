/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "../launcher_ui_app_page.hpp"
#include "../model/mesh_page_model.hpp"
#include "../model/mesh_page_contract.hpp"
#include "../model/page_timer_lifecycle.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct key_item;

class UIMeshPage : public AppPage
{
private:
    enum class ViewState
    {
        MAIN,
        INPUT
    };

public:
    UIMeshPage();
    ~UIMeshPage();

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    ViewState view_state_ = ViewState::MAIN;

    MeshPageModel model_;

    std::string msg_input_buf_;
    lv_obj_t *msg_input_lbl_ = nullptr;
    lv_obj_t *input_overlay_ = nullptr;

    lv_obj_t *msg_area_      = nullptr;
    lv_obj_t *neighbor_area_ = nullptr;
    lv_obj_t *status_lbl_    = nullptr;
    lv_obj_t *hint_lbl_      = nullptr;

    PageTimerLifecycle<lv_timer_t *> heartbeat_timer_;
    bool heartbeat_enabled_ = true;

private:
    static lv_obj_t *make_label(lv_obj_t *parent,
                                const char *text,
                                int x,
                                int y,
                                uint32_t color = 0xE6EDF3,
                                const lv_font_t *font = &lv_font_montserrat_12);
    static std::string current_time();

    void generate_node_id();
    void create_ui();
    bool ui_ready() const;
    void build_neighbor_list();
    void build_message_list();
    void add_message(const char *sender, const char *text);

    void show_input_overlay();
    void close_input_overlay();
    void update_input_display();
    void do_refresh();

    static void heartbeat_timer_cb(lv_timer_t *timer) noexcept;
    void on_heartbeat();

    void event_handler_init();
    void detach_ui_callbacks();
    static void owned_obj_delete_cb(lv_event_t *event) noexcept;
    static void static_lvgl_handler(lv_event_t *e) noexcept;
    void event_handler(lv_event_t *e);
    void handle_main_key(uint32_t key);
    void handle_input_key(uint32_t key, const key_item *item);
};
