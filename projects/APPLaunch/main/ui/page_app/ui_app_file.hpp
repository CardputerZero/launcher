/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "../launcher_ui_app_page.hpp"
#include "../model/file_browser_model.hpp"
#include "../model/file_browser_view_lifecycle.hpp"

#include <string>

class UIFilePage : public AppPage
{
public:
    UIFilePage();
    ~UIFilePage();

private:
    static constexpr int ITEM_H = 30;
    static constexpr int TITLE_H = 22;
    static constexpr int LIST_H = 128;

    void read_directory();
    bool create_ui();
    bool build_list_rows();
    bool create_row(lv_obj_t *parent, int visual_row, int index, bool selected);
    void navigate_into();
    void navigate_parent();
    void event_handler_init();
    static void static_lvgl_handler(lv_event_t *event) noexcept;
    static void static_owned_obj_delete_cb(lv_event_t *event) noexcept;
    void event_handler(lv_event_t *event);
    void detach_owned_obj_callbacks();

    FileBrowserViewLifecycle view_;
    FileBrowserModel model_;
};
