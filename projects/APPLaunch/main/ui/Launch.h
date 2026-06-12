/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl/lvgl.h"
#include "cp0_lvgl_app.h"

#include <functional>
#include <list>
#include <memory>
#include <string>

class Launch;
class UILaunchPage;

template <class PageT>
struct page_t
{
    using type = PageT;
};

template <class PageT>
inline constexpr page_t<PageT> page_v{};

struct app
{
    std::string Name;
    std::string Icon;
    std::string Exec;
    std::function<void(Launch *)> launch;

    app(std::string name, std::string icon, std::string exec, bool terminal);
    app(std::string name, std::string icon, std::string exec, bool terminal, bool sysplause);
    app(std::string name, std::string icon, std::string exec, bool terminal, bool sysplause, bool run_as_root);

    template <class PageT>
    app(std::string name, std::string icon, page_t<PageT> tag);
};

class Launch
{
public:
    Launch();
    ~Launch();

    void bind_ui();
    void set_launch_page(std::shared_ptr<UILaunchPage> launch_page);
    void update_left_slot(lv_obj_t *panel, lv_obj_t *label);
    void update_right_slot(lv_obj_t *panel, lv_obj_t *label);
    void launch_app();

private:
    friend struct app;

    void go_back_home();
    void launch_Exec_in_terminal(const std::string &exec, bool sysplause = true);
    void launch_Exec(const std::string &exec, bool keep_root = false);
    void applications_load();
    void inotify_init_watch();
    void refresh_ui_panels();
    void applications_reload();

    static void lv_go_back_home(void *arg);
    static void app_dir_watch_cb(lv_timer_t *timer);

    std::shared_ptr<UILaunchPage> launch_page_;
    int current_app = 2;
    cp0_watcher_t dir_watcher = NULL;
    lv_timer_t *watch_timer = nullptr;
    int fixed_count = 0;
    std::list<app> app_list;
    std::shared_ptr<void> app_Page;
    std::shared_ptr<void> home_Page;
};
