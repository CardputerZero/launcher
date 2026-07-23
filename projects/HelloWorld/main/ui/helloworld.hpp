/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "ui_app_page.hpp"

#include <array>

// The launcher-facing class name is retained for AppPage compatibility.
class UIIpPanelPage : public AppPage
{
public:
    UIIpPanelPage();
    ~UIIpPanelPage() override = default;

private:
    void create_ui();
    void update_ui();
    void activate(int index);
    void handle_key(lv_event_t *event);

    static void button_event_cb(lv_event_t *event);
    static void key_event_cb(lv_event_t *event);

    bool dark_mode_ = false;
    bool bold_text_ = false;
    bool info_visible_ = false;
    bool counter_page_ = false;
    int counter_ = 0;
    int selected_button_ = 0;

    lv_obj_t *background_ = nullptr;
    lv_obj_t *content_ = nullptr;
    lv_obj_t *navigation_ = nullptr;
    lv_obj_t *hello_group_ = nullptr;
    lv_obj_t *greeting_label_ = nullptr;
    lv_obj_t *info_label_ = nullptr;
    lv_obj_t *counter_label_ = nullptr;
    std::array<lv_obj_t *, 5> buttons_{};
    std::array<lv_obj_t *, 5> button_labels_{};
};
