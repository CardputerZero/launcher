/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "model/launcher_media_model.hpp"
#include "model/launcher_media_osd_contract.hpp"
#include "lvgl/lvgl.h"

class LauncherMediaOsd
{
public:
    void show_level(const char *title, const char *icon, int percent) noexcept;
    void show_mute(bool muted) noexcept;
    void shutdown();

private:
    bool ensure_created() noexcept;
    void show() noexcept;
    void stop_hide_timer();
    static void hide_timer_cb(lv_timer_t *timer) noexcept;
    static void container_delete_cb(lv_event_t *event) noexcept;
    static void child_delete_cb(lv_event_t *event) noexcept;

    lv_obj_t *container_ = nullptr;
    lv_obj_t *icon_ = nullptr;
    lv_obj_t *title_ = nullptr;
    lv_obj_t *value_ = nullptr;
    lv_obj_t *bar_ = nullptr;
    lv_timer_t *hide_timer_ = nullptr;
    LauncherMediaOsdModel model_;
};

LauncherMediaOsd &launcher_media_osd();
