/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "home_icon_buffer_pool.hpp"
#include "launcher_ui_app_page.hpp"
#include "model/launcher_grid_model.hpp"
#include "model/launcher_navigation_model.hpp"
#include "model/launcher_startup_contract.hpp"
#include <atomic>
#include <array>
#include <memory>
#include <string>
#include <vector>

class Launch;

// Home screen: a 3x2 grid showing six apps per page.  Arrow keys move the
// selection inside the grid (up/down swap rows); stepping past the last
// column flips to the next page with a slide animation, and the app list
// wraps around at both ends.
class UILaunchPage : public home_base
{
public:
    explicit UILaunchPage(Launch *launch);
    ~UILaunchPage();

    void show_home_screen();
    void load_home_screen();
    void start_startup_gif();
    void create_screen();
    enum LauncherGridElement : size_t {
        kCard0 = 0,
        kCard1,
        kCard2,
        kCard3,
        kCard4,
        kCard5,
        kTitleOffset,
        kTitle0 = kTitleOffset,
        kTitle1,
        kTitle2,
        kTitle3,
        kTitle4,
        kTitle5,
        kLauncherGridElementCount,
    };

    void init_input_group();
    static void bind_home_input_group();
    static lv_group_t *home_input_group();
    lv_obj_t *panel(size_t cell);
    lv_obj_t *label(size_t cell);

    void refresh_grid();
    void reload_home_icons(const std::vector<std::string> &icon_paths);
    void update_grid_cell(size_t cell, const char *title, const std::string &icon);
    void update_grid_item(lv_obj_t *panel, lv_obj_t *label, const char *title,
                          const std::string &icon);
    void launch_selected_app();
    void move_grid_next();
    void move_grid_previous();
    void move_grid_up();
    void move_grid_down();
    void select_grid_cell(size_t cell);
    bool navigation_ready() const { return !page_flip_active_; }

protected:
    std::array<lv_obj_t *, kLauncherGridElementCount> grid_elements_ = {};

private:
    struct StartupTimerContext
    {
        UILaunchPage *page = nullptr;
        LauncherStartupGeneration::Token generation = 0;
    };

    struct StartupSoundAsyncState
    {
        std::atomic<bool> alive{true};
        std::atomic<bool> pending{false};
    };

    struct StartupSoundResultContext
    {
        UILaunchPage *page = nullptr;
        std::weak_ptr<StartupSoundAsyncState> state;
        int code = -1;
    };

    void create_app_container(lv_obj_t *parent);
    bool begin_grid_move();
    void finish_grid_move();
    void start_page_flip(bool forward);
    void finish_page_flip_animation();
    void cancel_page_flip_animation();
    void apply_grid_highlight();
    void handle_home_key(lv_event_t *event);
    void handle_startup_gif_event(lv_event_t *event);
    void play_startup_sound_with_retry();
    void warm_up_audio_mixer();
    void handle_startup_sound_result(int code);
    void stop_startup_presentation();
    void stop_startup_sound_timer();
    void stop_startup_delay();
    void finish_startup_delay();
    void release_startup_screen(lv_obj_t *&screen);

    void set_grid_clickable(bool clickable);
    void set_panel_icon(lv_obj_t *panel, const std::string &src);
    static void on_left_arrow_clicked(lv_event_t *event) noexcept;
    static void on_right_arrow_clicked(lv_event_t *event) noexcept;
    static void on_app_clicked(lv_event_t *event) noexcept;
    static void on_home_key(lv_event_t *event) noexcept;
    static void on_startup_gif_event(lv_event_t *event) noexcept;
    static void on_owned_obj_deleted(lv_event_t *event) noexcept;
    static void on_root_deleted(lv_event_t *event) noexcept;
    static void startup_sound_timer_cb(lv_timer_t *timer) noexcept;
    static void startup_sound_result_async(void *user_data) noexcept;
    static void startup_delay_timer_cb(lv_timer_t *timer) noexcept;

    Launch *launch_ = nullptr;
    lv_obj_t *startup_gif_ = nullptr;
    lv_obj_t *left_arrow_button_ = nullptr;
    lv_obj_t *right_arrow_button_ = nullptr;
    lv_obj_t *grid_container_ = nullptr;
    lv_timer_t *startup_sound_timer_ = nullptr;
    lv_timer_t *startup_delay_timer_ = nullptr;
    StartupTimerContext *startup_sound_context_ = nullptr;
    StartupTimerContext *startup_delay_context_ = nullptr;
    lv_obj_t *startup_logo_scr_ = nullptr;
    std::array<char, 256> startup_gif_path_ = {};
    std::array<char, 256> startup_logo_path_ = {};
    bool startup_gif_done_ = false;
    int startup_sound_retry_count_ = 0;
    LauncherGridModel grid_;
    LauncherStartupDelayModel startup_delay_model_;
    LauncherStartupGeneration startup_sound_generation_;
    LauncherStartupGeneration startup_delay_generation_;
    HomeIconBufferPool home_icon_pool_;
    std::shared_ptr<StartupSoundAsyncState> startup_sound_state_ =
        std::make_shared<StartupSoundAsyncState>();
    std::shared_ptr<bool> grid_alive_ = std::make_shared<bool>(true);
    bool home_key_registered_ = false;
    bool page_flip_active_ = false;
};
