/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl/lvgl.h"
#include "model/esc_hold_lifecycle_model.hpp"

struct key_item;

class EscHoldHintController
{
public:
    using ForceHomeCallback = void (*)(void *user_data);
    using HomeHoldCallback = void (*)(void *user_data);

    bool handle(const key_item *item);
    void clear_hint_ownership();
    void set_return_home_enabled(bool enabled);
    void set_force_home_callback(ForceHomeCallback callback, void *user_data);
    /* Fired when ESC is held for the same 3s on the launcher home screen
     * (i.e. while return-home is disabled). */
    void set_home_hold_callback(HomeHoldCallback callback, void *user_data);
    void shutdown();

private:
    void poll(lv_timer_t *timer);
    bool home_hold_available() const;
    static void poll_timer_cb(lv_timer_t *timer) noexcept;

    lv_timer_t *poll_timer_ = nullptr;
    EscHoldLifecycleModel model_;
    ForceHomeCallback force_home_callback_ = nullptr;
    void *force_home_user_data_ = nullptr;
    HomeHoldCallback home_hold_callback_ = nullptr;
    void *home_hold_user_data_ = nullptr;
    bool return_home_enabled_ = false;
    bool hold_is_home_ = false;
};

EscHoldHintController &esc_hold_hint_controller();
