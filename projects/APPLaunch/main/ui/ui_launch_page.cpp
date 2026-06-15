/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_launch_page.h"

#include "launch.h"
#include "hal_lvgl_bsp.h"
#include "lvgl/src/widgets/gif/lv_gif.h"
#include "sample_log.h"
#include "compat/input_keys.h"
#include <list>
#include <string>
#include <utility>

#include "animation/ui_launcher_animation.h"

#include <algorithm>
#include <unistd.h>

void UILaunchPage::rotate_carousel_left(size_t start, size_t end)
{
    auto &items = carousel_elements_;
    std::rotate(items.begin() + start, items.begin() + start + 1, items.begin() + end + 1);
}

void UILaunchPage::rotate_carousel_right(size_t start, size_t end)
{
    auto &items = carousel_elements_;
    std::rotate(items.begin() + start, items.begin() + end, items.begin() + end + 1);
}

// ============================================================
// switch panel style
// ============================================================

void UILaunchPage::switchpanleEnable(int obj_index, int enable)
{
    lv_obj_t *obj = carousel_elements_[obj_index];

    if (enable)
    {
        lv_obj_set_width(obj, 10);
        lv_obj_set_height(obj, 10);
        lv_obj_set_align(obj, LV_ALIGN_CENTER);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_bg_color(obj, lv_color_hex(0xCCCC33), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj, lv_color_hex(0xCCCC33), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else
    {
        lv_obj_set_width(obj, 5);
        lv_obj_set_height(obj, 5);
        lv_obj_set_align(obj, LV_ALIGN_CENTER);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_bg_color(obj, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj, lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}


void UILaunchPage::switchpanleEnableClick(int obj_index, int enable)
{
    lv_obj_t *obj = carousel_elements_[obj_index];

    if (enable)
    {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
    else
    {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
}

void UILaunchPage::set_panel_icon(lv_obj_t *panel, const char *src)
{
    if (!panel)
        return;

    const char *icon_src = src ? src : "";
    if (icon_src[0] == '\0') {
        SLOGW("[LAUNCHER] set panel icon with empty path");
    } else if (access(icon_src, R_OK) == 0) {
        SLOGI("[LAUNCHER] set panel icon: %s", icon_src);
    } else {
        SLOGW("[LAUNCHER] set panel icon missing/unreadable: %s", icon_src);
    }

    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *img = lv_obj_get_child(panel, 0);
    if (!img || !lv_obj_check_type(img, &lv_image_class)) {
        img = lv_image_create(panel);
        lv_obj_set_size(img, LV_PCT(100), LV_PCT(100));
        lv_obj_set_align(img, LV_ALIGN_CENTER);
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_STRETCH);
    }
    lv_image_set_src(img, icon_src);
}



namespace {

UILaunchPage *active_launch_page = nullptr;
lv_group_t *home_input_group = nullptr;
constexpr int kStartupSoundRetryMax = 10;
constexpr uint32_t kStartupSoundRetryMs = 500;

// ==================== standard layout for carousel slots ====================

struct CarouselSlot {
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t width;
    lv_coord_t height;
    bool hidden;
};

static const CarouselSlot CAROUSEL_SLOTS[] = {
    {-177, 4, 61, 61, true},
    {-99, -6, 80, 80, false},
    {0, -16, 100, 100, false},
    {99, -6, 80, 80, false},
    {177, 4, 61, 61, true},
    {-177, LABEL_Y_SIDE, 0, 0, true},
    {-99, LABEL_Y_SIDE, 0, 0, false},
    {0, LABEL_Y_CENTER, 0, 0, false},
    {99, LABEL_Y_SIDE, 0, 0, false},
    {177, LABEL_Y_SIDE, 0, 0, true},
};

// ============================================================
// audio
// ============================================================

static void audio_play_switch(void)
{
    cp0_signal_audio_api({"SystemSoundPlay", "1"}, nullptr);
}

static void audio_play_enter(void)
{
    cp0_signal_audio_api({"SystemSoundPlay", "2"}, nullptr);
}

// ============================================================
// Force the panel to the specified slot
// ============================================================

static void snap_panel_to_slot(lv_obj_t *panel, int slot)
{
    const CarouselSlot &layout = CAROUSEL_SLOTS[slot];
    lv_obj_set_x(panel, layout.x);
    lv_obj_set_y(panel, layout.y);
    lv_obj_set_width(panel, layout.width);
    lv_obj_set_height(panel, layout.height);

    if (layout.hidden)
    {
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
    }
}


// ============================================================
// Force the label to the specified slot
// ============================================================

static void snap_label_to_slot(lv_obj_t *label, int slot)
{
    const CarouselSlot &layout = CAROUSEL_SLOTS[slot];
    lv_obj_set_x(label, layout.x);
    lv_obj_set_y(label, layout.y);

    if (layout.hidden)
    {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
}

static uint32_t fzxc_to_arrow(uint32_t key)
{
    switch (key)
    {
    case KEY_F:
        return KEY_UP;

    case KEY_X:
        return KEY_DOWN;

    case KEY_Z:
        return KEY_LEFT;

    case KEY_C:
        return KEY_RIGHT;

    default:
        return key;
    }
}

static UILaunchPage *page_from_event(lv_event_t *event)
{
    return event ? static_cast<UILaunchPage *>(lv_event_get_user_data(event)) : nullptr;
}

} // namespace


LauncherFonts::~LauncherFonts()
{
    release();
}

lv_font_t *LauncherFonts::get(const char *ttf_name, uint16_t size, lv_freetype_font_style_t style)
{
    const std::string font_key = key(ttf_name, size, style);
    auto it = fonts_.find(font_key);
    if (it != fonts_.end()) {
        return it->second ? it->second : fallback(size);
    }

    lv_font_t *font = lv_freetype_font_create(cp0_file_path_c(ttf_name), LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                              size, style);
    fonts_[font_key] = font;
    return font ? font : fallback(size);
}

void LauncherFonts::release()
{
    for (auto &item : fonts_) {
        if (item.second) {
            lv_freetype_font_delete(item.second);
            item.second = nullptr;
        }
    }
    fonts_.clear();
}

lv_font_t *LauncherFonts::fallback(uint16_t size) const
{
    if (size >= 18) {
        return (lv_font_t *)&lv_font_montserrat_20;
    }
    if (size >= 14) {
        return (lv_font_t *)&lv_font_montserrat_14;
    }
    return (lv_font_t *)&lv_font_montserrat_12;
}

std::string LauncherFonts::key(const char *ttf_name, uint16_t size, lv_freetype_font_style_t style)
{
    return std::string(ttf_name ? ttf_name : "") + "#" + std::to_string(size) + "#" +
           std::to_string(static_cast<int>(style));
}

lv_group_t *UILaunchPage::home_input_group()
{
    if (active_launch_page)
        return active_launch_page->input_group();
    return ::home_input_group;
}

lv_obj_t *UILaunchPage::panel(size_t slot)
{
    return carousel_elements_[kCardFarLeft + slot];
}

lv_obj_t *UILaunchPage::label(size_t slot)
{
    return carousel_elements_[kTitleFarLeft + slot];
}

void UILaunchPage::bind_home_input_group()
{
    lv_indev_t *indev = lv_indev_get_next(nullptr);
    if (indev) {
        lv_indev_set_group(indev, home_input_group());
    }
}

void UILaunchPage::init_input_group()
{
    ::home_input_group = input_group();
    bind_home_input_group();
}

void UILaunchPage::show_home_screen()
{
    SLOGI("[HOME] show_home_screen() - loading launcher home screen");
    use_bold_home_title_font();
    lv_disp_load_scr(screen());
    UILaunchPage::bind_home_input_group();
}

void UILaunchPage::load_home_screen()
{
    show_home_screen();
    play_startup_sound_with_retry();
}

void UILaunchPage::play_startup_sound_with_retry()
{
    int play_result = -1;
    cp0_signal_audio_api({"SystemSoundPlay", "0"}, [&](int code, std::string) {
        play_result = code;
    });

    if (play_result == 0) {
        stop_startup_sound_timer();
        startup_sound_retry_count_ = 0;
        return;
    }

    if (startup_sound_timer_)
        return;

    startup_sound_retry_count_ = 0;
    startup_sound_timer_ = lv_timer_create(startup_sound_timer_cb, kStartupSoundRetryMs, this);
}

void UILaunchPage::stop_startup_sound_timer()
{
    if (startup_sound_timer_) {
        lv_timer_delete(startup_sound_timer_);
        startup_sound_timer_ = nullptr;
    }
}

void UILaunchPage::start_startup_gif()
{
    snprintf(startup_gif_path_.data(), startup_gif_path_.size(), "%s", cp0_file_path("logo_output.gif").c_str());
    startup_gif_done_ = false;
    startup_gif_ = lv_gif_create(nullptr);
    lv_gif_set_src(startup_gif_, startup_gif_path_.data());
    lv_obj_center(startup_gif_);
    lv_obj_add_event_cb(startup_gif_, on_startup_gif_event, LV_EVENT_ALL, this);
    lv_disp_load_scr(startup_gif_);
}

UILaunchPage::UILaunchPage(Launch *launch)
    : home_base(), launch_(launch)
{
    active_launch_page = this;
}

UILaunchPage::~UILaunchPage()
{
    stop_startup_sound_timer();
    if (green_bg_) {
        lv_obj_del(green_bg_);
        green_bg_ = nullptr;
    }
    if (active_launch_page == this)
        active_launch_page = nullptr;
}

void UILaunchPage::fill_right_entering_slot(lv_obj_t *panel, lv_obj_t *label)
{
    if (!launch_)
        return;

    launch_->select_next_app();
    if (const app *item = launch_->carousel_slot_app(kCardFarRight))
        update_carousel_item(panel, label, item->Name.c_str(), item->Icon.c_str());
}

void UILaunchPage::fill_left_entering_slot(lv_obj_t *panel, lv_obj_t *label)
{
    if (!launch_)
        return;

    launch_->select_previous_app();
    if (const app *item = launch_->carousel_slot_app(kCardFarLeft))
        update_carousel_item(panel, label, item->Name.c_str(), item->Icon.c_str());
}

void UILaunchPage::refresh_carousel()
{
    if (!launch_)
        return;

    for (size_t slot = 0; slot < 5; ++slot) {
        if (const app *item = launch_->carousel_slot_app(slot))
            update_carousel_slot(slot, item->Name.c_str(), item->Icon.c_str());
    }
}

void UILaunchPage::update_carousel_slot(size_t slot, const char *title, const char *icon)
{
    update_carousel_item(panel(slot), label(slot), title, icon);
}

void UILaunchPage::update_carousel_item(lv_obj_t *panel, lv_obj_t *label, const char *title, const char *icon)
{
    if (label)
        lv_label_set_text(label, title ? title : "");
    set_panel_icon(panel, icon);
}

void UILaunchPage::launch_selected_app()
{
    if (launch_)
        launch_->launch_app();
}

void UILaunchPage::finish_switch_animation()
{
    for (int i = 0; i < 5; i++)
    {
        snap_panel_to_slot(carousel_elements_[i], i);
    }

    for (int i = 5; i < 10; i++)
    {
        snap_label_to_slot(carousel_elements_[i], i);
    }

    is_animating_ = false;

    for (int i = 0; i < 5; i++) {
        uint32_t color = (i == 2) ? BORDER_COLOR_CENTER : BORDER_COLOR_SIDE;
        lv_obj_set_style_border_color(carousel_elements_[i], lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    for (int i = 5; i < 10; i++) {
        lv_obj_set_style_text_font(carousel_elements_[i], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

}

void UILaunchPage::switch_right()
{
    if (is_animating_)
        return;

    is_animating_ = true;

    lv_obj_clear_flag(carousel_elements_[0], LV_OBJ_FLAG_HIDDEN);

    launcher_home_animation::animate_right(carousel_elements_.data(), [this]() { finish_switch_animation(); });

    snap_panel_to_slot(carousel_elements_[4], 0);

    lv_obj_clear_flag(carousel_elements_[5], LV_OBJ_FLAG_HIDDEN);

    snap_label_to_slot(carousel_elements_[9], 5);

    fill_left_entering_slot(carousel_elements_[4], carousel_elements_[9]);

    switchpanleEnableClick(2, 0);
    rotate_carousel_right(0, 4);
    switchpanleEnableClick(2, 1);

    rotate_carousel_right(5, 9);

    switchpanleEnable(switch_current_pos_, 0);

    switch_current_pos_ = switch_current_pos_ == UILaunchPage::kPageDot0 ? UILaunchPage::kPageDot4 : switch_current_pos_ - 1;

    switchpanleEnable(switch_current_pos_, 1);
}

void UILaunchPage::switch_left()
{
    if (is_animating_)
        return;

    is_animating_ = true;

    lv_obj_clear_flag(carousel_elements_[4], LV_OBJ_FLAG_HIDDEN);

    launcher_home_animation::animate_left(carousel_elements_.data(), [this]() { finish_switch_animation(); });

    snap_panel_to_slot(carousel_elements_[0], 4);

    lv_obj_clear_flag(carousel_elements_[9], LV_OBJ_FLAG_HIDDEN);

    snap_label_to_slot(carousel_elements_[5], 9);

    fill_right_entering_slot(carousel_elements_[0], carousel_elements_[5]);

    switchpanleEnableClick(2, 0);
    rotate_carousel_left(0, 4);
    switchpanleEnableClick(2, 1);

    rotate_carousel_left(5, 9);

    switchpanleEnable(switch_current_pos_, 0);

    switch_current_pos_ = switch_current_pos_ == UILaunchPage::kPageDot4 ? UILaunchPage::kPageDot0 : switch_current_pos_ + 1;

    switchpanleEnable(switch_current_pos_, 1);
}

void UILaunchPage::handle_home_key(lv_event_t *event)
{
    if (!event)
        return;

    struct key_item *elm = static_cast<struct key_item *>(lv_event_get_param(event));
    if (!elm)
        return;

    uint32_t code = fzxc_to_arrow(elm->key_code);

    SLOGI("[LAUNCHER] main_key_switch raw=%u->code=%u state=%s sym=%s",
           elm->key_code,
           code,
           kbd_state_name(elm->key_state),
           elm->sym_name);

    if (elm->key_state)
    {
        switch (code)
        {
        case KEY_UP:
            break;

        case KEY_DOWN:
            break;

        case KEY_LEFT:
        {
            if (!lvping_lock_ && !is_animating_)
            {
                audio_play_switch();
                switch_right();
            }
        }
        break;

        case KEY_RIGHT:
        {
            if (!lvping_lock_ && !is_animating_)
            {
                audio_play_switch();
                switch_left();
            }
        }
        break;

        default:
            break;
        }
    }
    else if (code == KEY_ENTER)
    {
        audio_play_enter();
        launch_selected_app();
    }
    else if (code == KEY_F12)
    {
        if (lvping_lock_ == 0)
        {
            lvping_lock_ = 1;
            green_bg_ = lv_obj_create(lv_scr_act());
            lv_obj_set_size(green_bg_, 320, 170);
            lv_obj_align(green_bg_, LV_ALIGN_TOP_LEFT, 0, 0);

            lv_obj_set_style_bg_color(green_bg_, lv_color_hex(0x00FF00), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(green_bg_, LV_OPA_COVER, LV_PART_MAIN);

            lv_obj_set_style_border_width(green_bg_, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(green_bg_, 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(green_bg_, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(green_bg_, 0, LV_PART_MAIN);
        }
        else
        {
            lvping_lock_ = 0;
            if (green_bg_) {
                lv_obj_del(green_bg_);
                green_bg_ = nullptr;
            }
        }
    }
}

void UILaunchPage::handle_startup_gif_event(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_READY || startup_gif_done_)
        return;

    startup_gif_done_ = true;
    SLOGI("[GIF] first LV_EVENT_READY -> pause + home_screen_load()");
    if (startup_gif_)
        lv_gif_pause(startup_gif_);

    load_home_screen();
}

void UILaunchPage::on_left_arrow_clicked(lv_event_t *event)
{
    if (UILaunchPage *self = page_from_event(event))
        self->switch_right();
}

void UILaunchPage::on_right_arrow_clicked(lv_event_t *event)
{
    if (UILaunchPage *self = page_from_event(event))
        self->switch_left();
}

void UILaunchPage::on_app_clicked(lv_event_t *event)
{
    if (UILaunchPage *self = page_from_event(event))
        self->launch_selected_app();
}

void UILaunchPage::on_home_key(lv_event_t *event)
{
    if (UILaunchPage *self = page_from_event(event))
        self->handle_home_key(event);
}

void UILaunchPage::on_startup_gif_event(lv_event_t *event)
{
    if (UILaunchPage *self = page_from_event(event))
        self->handle_startup_gif_event(event);
}

void UILaunchPage::startup_sound_timer_cb(lv_timer_t *timer)
{
    UILaunchPage *self = static_cast<UILaunchPage *>(lv_timer_get_user_data(timer));
    if (!self)
        return;

    ++self->startup_sound_retry_count_;
    if (self->startup_sound_retry_count_ > kStartupSoundRetryMax) {
        self->stop_startup_sound_timer();
        return;
    }

    self->play_startup_sound_with_retry();
}

void UILaunchPage::create_screen()
{
    if (carousel_elements_[kCardCenter])
        return;

    create_app_container(content_container());

}


void UILaunchPage::create_app_container(lv_obj_t *parent)
{
    lv_obj_t *app_container = parent;
    if (!app_container)
        return;

    lv_obj_set_size(app_container, 320, 150);
    lv_obj_clear_flag(app_container, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

    carousel_elements_[kPageDot0] = lv_obj_create(app_container);
    lv_obj_set_width(carousel_elements_[kPageDot0], 5);
    lv_obj_set_height(carousel_elements_[kPageDot0], 5);
    lv_obj_set_x(carousel_elements_[kPageDot0], -20);
    lv_obj_set_y(carousel_elements_[kPageDot0], 70);
    lv_obj_set_align(carousel_elements_[kPageDot0], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements_[kPageDot0], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carousel_elements_[kPageDot0], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements_[kPageDot0], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(carousel_elements_[kPageDot0], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements_[kPageDot0], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements_[kPageDot0], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kPageDot1] = lv_obj_create(app_container);
    lv_obj_set_width(carousel_elements_[kPageDot1], 5);
    lv_obj_set_height(carousel_elements_[kPageDot1], 5);
    lv_obj_set_x(carousel_elements_[kPageDot1], -10);
    lv_obj_set_y(carousel_elements_[kPageDot1], 70);
    lv_obj_set_align(carousel_elements_[kPageDot1], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements_[kPageDot1], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carousel_elements_[kPageDot1], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements_[kPageDot1], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(carousel_elements_[kPageDot1], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements_[kPageDot1], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements_[kPageDot1], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kPageDot2] = lv_obj_create(app_container);
    lv_obj_set_width(carousel_elements_[kPageDot2], 10);
    lv_obj_set_height(carousel_elements_[kPageDot2], 10);
    lv_obj_set_x(carousel_elements_[kPageDot2], 0);
    lv_obj_set_y(carousel_elements_[kPageDot2], 70);
    lv_obj_set_align(carousel_elements_[kPageDot2], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements_[kPageDot2], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carousel_elements_[kPageDot2], lv_color_hex(0xCCCC33), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements_[kPageDot2], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(carousel_elements_[kPageDot2], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements_[kPageDot2], lv_color_hex(0xCCCC33), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements_[kPageDot2], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kPageDot3] = lv_obj_create(app_container);
    lv_obj_set_width(carousel_elements_[kPageDot3], 5);
    lv_obj_set_height(carousel_elements_[kPageDot3], 5);
    lv_obj_set_x(carousel_elements_[kPageDot3], 10);
    lv_obj_set_y(carousel_elements_[kPageDot3], 70);
    lv_obj_set_align(carousel_elements_[kPageDot3], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements_[kPageDot3], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carousel_elements_[kPageDot3], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements_[kPageDot3], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(carousel_elements_[kPageDot3], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements_[kPageDot3], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements_[kPageDot3], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kPageDot4] = lv_obj_create(app_container);
    lv_obj_set_width(carousel_elements_[kPageDot4], 5);
    lv_obj_set_height(carousel_elements_[kPageDot4], 5);
    lv_obj_set_x(carousel_elements_[kPageDot4], 20);
    lv_obj_set_y(carousel_elements_[kPageDot4], 70);
    lv_obj_set_align(carousel_elements_[kPageDot4], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements_[kPageDot4], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(carousel_elements_[kPageDot4], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements_[kPageDot4], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(carousel_elements_[kPageDot4], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements_[kPageDot4], lv_color_hex(0x4A4C4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements_[kPageDot4], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kTitleCenter] = lv_label_create(app_container);
    lv_obj_set_width(carousel_elements_[kTitleCenter], LV_SIZE_CONTENT);
    lv_obj_set_height(carousel_elements_[kTitleCenter], LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(carousel_elements_[kTitleCenter], 0);
    lv_obj_set_y(carousel_elements_[kTitleCenter], LABEL_Y_CENTER);
    lv_obj_set_align(carousel_elements_[kTitleCenter], LV_ALIGN_CENTER);
    lv_label_set_text(carousel_elements_[kTitleCenter], "CLI");
    lv_obj_set_style_text_font(carousel_elements_[kTitleCenter], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(carousel_elements_[kTitleCenter], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(carousel_elements_[kTitleCenter], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kTitleRight] = lv_label_create(app_container);
    lv_obj_set_width(carousel_elements_[kTitleRight], LV_SIZE_CONTENT);
    lv_obj_set_height(carousel_elements_[kTitleRight], LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(carousel_elements_[kTitleRight], 99);
    lv_obj_set_y(carousel_elements_[kTitleRight], LABEL_Y_SIDE);
    lv_obj_set_align(carousel_elements_[kTitleRight], LV_ALIGN_CENTER);
    lv_label_set_text(carousel_elements_[kTitleRight], "GAME");
    lv_obj_set_style_text_color(carousel_elements_[kTitleRight], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(carousel_elements_[kTitleRight], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(carousel_elements_[kTitleRight], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kTitleLeft] = lv_label_create(app_container);
    lv_obj_set_width(carousel_elements_[kTitleLeft], LV_SIZE_CONTENT);
    lv_obj_set_height(carousel_elements_[kTitleLeft], LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(carousel_elements_[kTitleLeft], -99);
    lv_obj_set_y(carousel_elements_[kTitleLeft], LABEL_Y_SIDE);
    lv_obj_set_align(carousel_elements_[kTitleLeft], LV_ALIGN_CENTER);
    lv_label_set_text(carousel_elements_[kTitleLeft], "STORE");
    lv_obj_set_style_text_color(carousel_elements_[kTitleLeft], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(carousel_elements_[kTitleLeft], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(carousel_elements_[kTitleLeft], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kCardLeft] = lv_obj_create(app_container);
    lv_obj_set_width(carousel_elements_[kCardLeft], 80);
    lv_obj_set_height(carousel_elements_[kCardLeft], 80);
    lv_obj_set_x(carousel_elements_[kCardLeft], -99);
    lv_obj_set_y(carousel_elements_[kCardLeft], -6);
    lv_obj_set_align(carousel_elements_[kCardLeft], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements_[kCardLeft], (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(carousel_elements_[kCardLeft], 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(carousel_elements_[kCardLeft], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements_[kCardLeft], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements_[kCardLeft], lv_color_hex(0x222222), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements_[kCardLeft], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kCardCenter] = lv_obj_create(app_container);
    lv_obj_set_width(carousel_elements_[kCardCenter], 100);
    lv_obj_set_height(carousel_elements_[kCardCenter], 100);
    lv_obj_set_x(carousel_elements_[kCardCenter], 0);
    lv_obj_set_y(carousel_elements_[kCardCenter], -16);
    lv_obj_set_align(carousel_elements_[kCardCenter], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements_[kCardCenter], LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(carousel_elements_[kCardCenter], 22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(carousel_elements_[kCardCenter], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements_[kCardCenter], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements_[kCardCenter], lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements_[kCardCenter], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(carousel_elements_[kCardCenter], 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kCardRight] = lv_obj_create(app_container);
    lv_obj_set_width(carousel_elements_[kCardRight], 80);
    lv_obj_set_height(carousel_elements_[kCardRight], 80);
    lv_obj_set_x(carousel_elements_[kCardRight], 99);
    lv_obj_set_y(carousel_elements_[kCardRight], -6);
    lv_obj_set_align(carousel_elements_[kCardRight], LV_ALIGN_CENTER);
    lv_obj_clear_flag(carousel_elements_[kCardRight], (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(carousel_elements_[kCardRight], 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(carousel_elements_[kCardRight], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements_[kCardRight], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements_[kCardRight], lv_color_hex(0x222222), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements_[kCardRight], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kCardFarRight] = lv_obj_create(app_container);
    lv_obj_set_width(carousel_elements_[kCardFarRight], 61);
    lv_obj_set_height(carousel_elements_[kCardFarRight], 61);
    lv_obj_set_x(carousel_elements_[kCardFarRight], 177);
    lv_obj_set_y(carousel_elements_[kCardFarRight], 4);
    lv_obj_set_align(carousel_elements_[kCardFarRight], LV_ALIGN_CENTER);
    lv_obj_add_flag(carousel_elements_[kCardFarRight], LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(carousel_elements_[kCardFarRight], (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(carousel_elements_[kCardFarRight], 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(carousel_elements_[kCardFarRight], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements_[kCardFarRight], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements_[kCardFarRight], lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements_[kCardFarRight], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    left_arrow_button_ = lv_btn_create(app_container);
    lv_obj_set_width(left_arrow_button_, 17);
    lv_obj_set_height(left_arrow_button_, 23);
    lv_obj_set_x(left_arrow_button_, -151);
    lv_obj_set_y(left_arrow_button_, -4);
    lv_obj_set_align(left_arrow_button_, LV_ALIGN_CENTER);
    lv_obj_add_flag(left_arrow_button_, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(left_arrow_button_, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(left_arrow_button_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(left_arrow_button_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(left_arrow_button_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(left_arrow_button_, cp0_file_path_c("carousel_left_arrow.png"), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(left_arrow_button_, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(left_arrow_button_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    right_arrow_button_ = lv_btn_create(app_container);
    lv_obj_set_width(right_arrow_button_, 17);
    lv_obj_set_height(right_arrow_button_, 23);
    lv_obj_set_x(right_arrow_button_, 150);
    lv_obj_set_y(right_arrow_button_, -4);
    lv_obj_set_align(right_arrow_button_, LV_ALIGN_CENTER);
    lv_obj_add_flag(right_arrow_button_, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(right_arrow_button_, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(right_arrow_button_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(right_arrow_button_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(right_arrow_button_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(right_arrow_button_, cp0_file_path_c("carousel_right_arrow.png"), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(right_arrow_button_, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(right_arrow_button_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kCardFarLeft] = lv_obj_create(app_container);
    lv_obj_set_width(carousel_elements_[kCardFarLeft], 61);
    lv_obj_set_height(carousel_elements_[kCardFarLeft], 61);
    lv_obj_set_x(carousel_elements_[kCardFarLeft], -177);
    lv_obj_set_y(carousel_elements_[kCardFarLeft], 4);
    lv_obj_set_align(carousel_elements_[kCardFarLeft], LV_ALIGN_CENTER);
    lv_obj_add_flag(carousel_elements_[kCardFarLeft], LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(carousel_elements_[kCardFarLeft], (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));      /// Flags
    lv_obj_set_style_radius(carousel_elements_[kCardFarLeft], 17, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(carousel_elements_[kCardFarLeft], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(carousel_elements_[kCardFarLeft], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(carousel_elements_[kCardFarLeft], lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(carousel_elements_[kCardFarLeft], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kTitleFarLeft] = lv_label_create(app_container);
    lv_obj_set_width(carousel_elements_[kTitleFarLeft], LV_SIZE_CONTENT);
    lv_obj_set_height(carousel_elements_[kTitleFarLeft], LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(carousel_elements_[kTitleFarLeft], -177);
    lv_obj_set_y(carousel_elements_[kTitleFarLeft], LABEL_Y_SIDE);
    lv_obj_set_align(carousel_elements_[kTitleFarLeft], LV_ALIGN_CENTER);
    lv_label_set_text(carousel_elements_[kTitleFarLeft], "one");
    lv_obj_add_flag(carousel_elements_[kTitleFarLeft], LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_set_style_text_font(carousel_elements_[kTitleFarLeft], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(carousel_elements_[kTitleFarLeft], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(carousel_elements_[kTitleFarLeft], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    carousel_elements_[kTitleFarRight] = lv_label_create(app_container);
    lv_obj_set_width(carousel_elements_[kTitleFarRight], LV_SIZE_CONTENT);
    lv_obj_set_height(carousel_elements_[kTitleFarRight], LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(carousel_elements_[kTitleFarRight], 177);
    lv_obj_set_y(carousel_elements_[kTitleFarRight], LABEL_Y_SIDE);
    lv_obj_set_align(carousel_elements_[kTitleFarRight], LV_ALIGN_CENTER);
    lv_label_set_text(carousel_elements_[kTitleFarRight], "three");
    lv_obj_add_flag(carousel_elements_[kTitleFarRight], LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_set_style_text_font(carousel_elements_[kTitleFarRight], launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(carousel_elements_[kTitleFarRight], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(carousel_elements_[kTitleFarRight], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(carousel_elements_[kCardLeft], on_app_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(carousel_elements_[kCardCenter], on_app_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(carousel_elements_[kCardRight], on_app_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(carousel_elements_[kCardFarRight], on_app_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(left_arrow_button_, on_left_arrow_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(right_arrow_button_, on_right_arrow_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(carousel_elements_[kCardFarLeft], on_app_clicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(screen(), on_home_key, (lv_event_code_t)LV_EVENT_KEYBOARD, this);


}
