/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "ui_app_page.hpp"

#include <functional>
#include <memory>
#include <utility>

struct Cp0LvglRunOptions
{
    std::function<void()> after_lvgl_init;
    std::function<void()> after_resource_init;
    std::function<bool()> setup;
    std::function<bool()> should_quit;
    std::function<void()> teardown;
};

struct Cp0LvglAppHooks
{
    std::function<void()> after_lvgl_init;
    std::function<void()> after_resource_init;
    std::function<void(AppPageRoot &)> after_page_init;
    std::function<bool()> should_quit;
};

int cp0_lvgl_run(Cp0LvglRunOptions options = {});
void cp0_lvgl_wake();

template <class PageT>
int cp0_lvgl_run_app(Cp0LvglAppHooks hooks = {})
{
    std::unique_ptr<PageT> page;
    Cp0LvglRunOptions options;
    options.after_lvgl_init = std::move(hooks.after_lvgl_init);
    options.after_resource_init = std::move(hooks.after_resource_init);
    options.should_quit = std::move(hooks.should_quit);
    options.setup = [&]() {
        page = cp0_lvgl_start_app<PageT>();
        if (!page || !page->screen()) return false;
        if (hooks.after_page_init)
            hooks.after_page_init(*page);
        return true;
    };
    options.teardown = [&]() { page.reset(); };
    return cp0_lvgl_run(std::move(options));
}
