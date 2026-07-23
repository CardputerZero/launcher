/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#define APP_PAGE_IMPLEMENTATION_UNIT
#include "ui_app_file.hpp"

#include "input_keys.h"
#include "sample_log.h"

#include <utility>

UIFilePage::UIFilePage() : AppPage()
{
    try {
        set_page_title("FILES");
        if (create_ui()) event_handler_init();
    } catch (...) {
        detach_owned_obj_callbacks();
    }
}

UIFilePage::~UIFilePage()
{
    detach_owned_obj_callbacks();
    if (root_screen_)
        lv_obj_remove_event_cb_with_user_data(
            root_screen_, UIFilePage::static_lvgl_handler, this);
}

void UIFilePage::read_directory()
{
    int result_code = -1;
    std::string data;
    cp0_signal_filesystem_api(
        {"DirListDetail", model_.current_path()},
        [&](int code, std::string result) {
            result_code = code;
            data = std::move(result);
        });
    if (!model_.apply_listing(result_code, data))
        SLOGI("[File] directory list failed: %s", model_.current_path().c_str());
}

void UIFilePage::navigate_into()
{
    if (!model_.enter_selected()) return;
    read_directory();
    build_list_rows();
}

void UIFilePage::navigate_parent()
{
    if (!model_.navigate_parent()) return;
    read_directory();
    build_list_rows();
}

void UIFilePage::event_handler_init()
{
    if (!root_screen_) return;
    lv_obj_add_event_cb(root_screen_, UIFilePage::static_lvgl_handler, LV_EVENT_ALL, this);
}

void UIFilePage::static_lvgl_handler(lv_event_t *event) noexcept
{
    try {
        if (!event) return;
        UIFilePage *page = static_cast<UIFilePage *>(lv_event_get_user_data(event));
        if (page) page->event_handler(event);
    } catch (...) {
    }
}

void UIFilePage::event_handler(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_DELETE &&
        lv_event_get_target(event) == lv_event_get_current_target(event)) {
        view_.clear();
        return;
    }
    if (!launcher_ui::events::is_key_released(event)) return;

    const uint32_t key = launcher_ui::events::keyboard_key(event);
    switch (key) {
    case KEY_UP:
        if (model_.select_previous()) build_list_rows();
        break;
    case KEY_DOWN:
        if (model_.select_next()) build_list_rows();
        break;
    case KEY_RIGHT:
    case KEY_ENTER:
        navigate_into();
        break;
    case KEY_LEFT:
        navigate_parent();
        break;
    case KEY_ESC:
        if (model_.current_path() != "/")
            navigate_parent();
        else if (navigate_home)
            navigate_home();
        break;
    default:
        break;
    }
}

void UIFilePage::static_owned_obj_delete_cb(lv_event_t *event) noexcept
{
    if (!event || !file_browser_owned_delete_callback_allowed(
            lv_event_get_target(event), lv_event_get_current_target(event))) return;
    auto *page = static_cast<UIFilePage *>(lv_event_get_user_data(event));
    auto *deleted = static_cast<lv_obj_t *>(lv_event_get_target(event));
    if (!page) return;
    page->view_.erased(deleted);
}

void UIFilePage::detach_owned_obj_callbacks()
{
    for (void *handle : {view_.path_label(), view_.list_container(), view_.background()})
        if (handle)
            lv_obj_remove_event_cb_with_user_data(static_cast<lv_obj_t *>(handle),
                                                  static_owned_obj_delete_cb, this);
    view_.clear();
}
