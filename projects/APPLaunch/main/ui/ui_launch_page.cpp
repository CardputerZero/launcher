/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_launch_page.h"

#include "launch.h"

#include "animation/launcher_grid_layout.h"
#include "animation/ui_launcher_animation.h"
#include "ui.h"

#include <algorithm>

void UILaunchPage::move_grid_next()
{
    if (!begin_grid_move()) return;
    if (grid_.move_row_right()) finish_grid_move();
}

void UILaunchPage::move_grid_previous()
{
    if (!begin_grid_move()) return;
    if (grid_.move_row_left()) finish_grid_move();
}

void UILaunchPage::move_grid_down()
{
    if (!begin_grid_move()) return;
    if (grid_.move_down()) finish_grid_move();
}

void UILaunchPage::move_grid_up()
{
    if (!begin_grid_move()) return;
    if (grid_.move_up()) finish_grid_move();
}

namespace {

UILaunchPage *active_launch_page = nullptr;
lv_group_t *home_input_group = nullptr;
// Grid cells never move: the layout is static, and page flips slide the whole
// container instead of repositioning individual cells.
static_assert(UILaunchPage::kLauncherGridElementCount ==
              launcher_grid_layout::kCells * 2);

} // namespace


lv_group_t *UILaunchPage::home_input_group()
{
    if (active_launch_page)
        return active_launch_page->input_group();
    return ::home_input_group;
}

lv_obj_t *UILaunchPage::panel(size_t cell)
{
    return grid_elements_[cell];
}

lv_obj_t *UILaunchPage::label(size_t cell)
{
    return grid_elements_[kTitleOffset + cell];
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

UILaunchPage::UILaunchPage(Launch *launch)
    : home_base(), launch_(launch)
{
    active_launch_page = this;
    if (root_screen_)
        lv_obj_add_event_cb(root_screen_, UILaunchPage::on_root_deleted,
                            LV_EVENT_DELETE, this);
}

UILaunchPage::~UILaunchPage()
{
    startup_sound_state_->alive.store(false);
    *grid_alive_ = false;
    if (grid_container_) {
        lv_obj_remove_event_cb_with_user_data(
            grid_container_, UILaunchPage::on_owned_obj_deleted, this);
        for (lv_obj_t *element : grid_elements_) {
            if (element)
                lv_obj_remove_event_cb_with_user_data(
                    element, UILaunchPage::on_owned_obj_deleted, this);
        }
        lv_obj_delete(grid_container_);
        grid_container_ = nullptr;
        grid_elements_.fill(nullptr);
    }
    if (home_key_registered_ && root_screen_) {
        lv_obj_remove_event_cb_with_user_data(
            root_screen_, UILaunchPage::on_home_key, this);
        home_key_registered_ = false;
    }
    if (root_screen_)
        lv_obj_remove_event_cb_with_user_data(
            root_screen_, UILaunchPage::on_root_deleted, this);
    stop_startup_delay();
    stop_startup_sound_timer();
    release_startup_screen(startup_logo_scr_);
    release_startup_screen(startup_gif_);
    if (active_launch_page == this)
        active_launch_page = nullptr;
    if (::home_input_group == input_group())
        ::home_input_group = nullptr;
}

void UILaunchPage::release_startup_screen(lv_obj_t *&screen)
{
    if (!screen) return;

    lv_display_t *display = lv_display_get_default();
    auto display_references = [display](lv_obj_t *candidate) {
        return display && candidate &&
            (lv_display_get_screen_active(display) == candidate ||
             lv_display_get_screen_loading(display) == candidate ||
             lv_display_get_screen_prev(display) == candidate);
    };
    auto detach_from_page = [this](lv_obj_t *object) {
        lv_obj_remove_event_cb_with_user_data(
            object, UILaunchPage::on_owned_obj_deleted, this);
        lv_obj_remove_event_cb_with_user_data(
            object, UILaunchPage::on_startup_gif_event, this);
    };

    const bool display_references_screen = display_references(screen);
    if (display_references_screen) {
        if (root_screen_ && lv_display_get_screen_active(display) != root_screen_) {
            lv_screen_load(root_screen_);
        } else {
            detach_from_page(screen);
            screen = nullptr;
            return;
        }
        if (display_references(screen)) {
            detach_from_page(screen);
            screen = nullptr;
            return;
        }
    }

    if (screen) lv_obj_delete(screen);
    screen = nullptr;
}

void UILaunchPage::on_root_deleted(lv_event_t *event) noexcept
{
    try {
        auto *self = static_cast<UILaunchPage *>(lv_event_get_user_data(event));
        if (!self || lv_event_get_target(event) != lv_event_get_current_target(event) ||
            lv_event_get_target(event) != self->root_screen_)
            return;
        self->stop_startup_delay();
        self->stop_startup_sound_timer();
        self->startup_sound_state_->alive.store(false);
        *self->grid_alive_ = false;
        self->grid_elements_.fill(nullptr);
        self->grid_container_ = nullptr;
        self->home_key_registered_ = false;
    } catch (...) {
    }
}

void UILaunchPage::on_owned_obj_deleted(lv_event_t *event) noexcept
{
    try {
        auto *self = static_cast<UILaunchPage *>(lv_event_get_user_data(event));
        lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(event));
        if (!self || !target || target != lv_event_get_current_target(event)) return;
        const bool recover_gif = launcher_startup_should_recover_deleted_gif(
            self->startup_gif_ == target, self->startup_gif_done_);
        if (self->startup_logo_scr_ == target) self->startup_logo_scr_ = nullptr;
        if (self->startup_gif_ == target) self->startup_gif_ = nullptr;
        bool grid_object_deleted = self->grid_container_ == target;
        for (lv_obj_t *&element : self->grid_elements_) {
            if (element == target) {
                element = nullptr;
                grid_object_deleted = true;
            }
        }
        if (self->grid_container_ == target) {
            self->grid_container_ = nullptr;
            self->grid_elements_.fill(nullptr);
        }
        if (grid_object_deleted) {
            *self->grid_alive_ = false;
            self->cancel_page_flip_animation();
        }
        if (recover_gif) {
            self->startup_gif_done_ = true;
            self->load_home_screen();
        }
    } catch (...) {
    }
}

void UILaunchPage::refresh_grid()
{
    if (!launch_)
        return;

    grid_.set_app_count(launch_->app_count());
    grid_.set_index(launch_->current_app_index());

    const size_t page_base = grid_.page() * LauncherGridModel::kCellsPerPage;
    for (size_t cell = 0; cell < launcher_grid_layout::kCells; ++cell) {
        if (const app *item = launch_->app_at_index(static_cast<int>(page_base + cell)))
            update_grid_cell(cell, item->Name.c_str(), item->Icon);
        else
            update_grid_cell(cell, "", {});
    }
    apply_grid_highlight();
}

void UILaunchPage::reload_home_icons(const std::vector<std::string> &icon_paths)
{
    HomeIconBufferPool replacement;
    replacement.rebuild(icon_paths);
    home_icon_pool_.swap(replacement);

    // Keep the old pool alive until every image widget no longer references it.
    for (size_t cell = 0; cell < launcher_grid_layout::kCells; ++cell)
        set_panel_icon(panel(cell), {});
    refresh_grid();
}

void UILaunchPage::launch_selected_app()
{
    if (launch_)
        launch_->launch_app();
}

void UILaunchPage::select_grid_cell(size_t cell)
{
    if (!launch_) return;
    grid_.set_app_count(launch_->app_count());
    grid_.set_index(launch_->current_app_index());
    const size_t target = grid_.page() * LauncherGridModel::kCellsPerPage + cell;
    grid_.set_index(target);
    launch_->select_app_relative(static_cast<int>(target) -
                                 static_cast<int>(launch_->current_app_index()));
    apply_grid_highlight();
}

void UILaunchPage::apply_grid_highlight()
{
    const size_t page_base = grid_.page() * LauncherGridModel::kCellsPerPage;
    for (size_t cell = 0; cell < launcher_grid_layout::kCells; ++cell) {
        const bool selected = page_base + cell == grid_.index();
        lv_obj_t *card = grid_elements_[cell];
        if (card) {
            const auto selector = LV_PART_MAIN | LV_STATE_DEFAULT;
            const uint32_t border = selected ? launcher_grid_layout::kSelectedBorderColor
                                             : BORDER_COLOR_SIDE;
            lv_obj_set_style_border_color(card, lv_color_hex(border), selector);
            lv_obj_set_style_bg_color(
                card, lv_color_hex(launcher_grid_layout::kSelectedBgColor), selector);
            lv_obj_set_style_bg_opa(card, selected ? launcher_grid_layout::kSelectedBgOpa
                                                   : LV_OPA_TRANSP,
                                    selector);
            /* Dim the icon art so the selected tile reads as the lit one. */
            if (lv_obj_t *clip = lv_obj_get_child(card, 0)) {
                if (lv_obj_t *image = lv_obj_get_child(clip, 0))
                    lv_obj_set_style_opa(image, selected ? LV_OPA_COVER
                                                         : launcher_grid_layout::kUnselectedIconOpa,
                                         selector);
            }
        }
        if (label(cell)) {
            launcher_home_animation::apply_title_overflow(label(cell), selected);
            lv_obj_set_style_text_color(
                label(cell),
                lv_color_hex(selected ? 0xFFFFFF
                                      : launcher_grid_layout::kUnselectedTitleColor),
                LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

bool UILaunchPage::begin_grid_move()
{
    if (page_flip_active_) return false;
    if (!launch_) return false;
    if (std::any_of(grid_elements_.begin(), grid_elements_.end(),
                    [](lv_obj_t *element) { return element == nullptr; }))
        return false;

    grid_.set_app_count(launch_->app_count());
    grid_.set_index(launch_->current_app_index());
    return true;
}

void UILaunchPage::finish_grid_move()
{
    // The model already sits on the new index; sync Launch through a signed
    // delta (select_app_relative wraps, so any magnitude works).
    launch_->select_app_relative(static_cast<int>(grid_.index()) -
                                 static_cast<int>(launch_->current_app_index()));
    if (grid_.crossed_page())
        start_page_flip(grid_.moved_forward());
    else
        apply_grid_highlight();
}

void UILaunchPage::start_page_flip(bool forward)
{
    if (!grid_container_) return;
    page_flip_active_ = true;
    set_grid_clickable(false);

    refresh_grid();

    std::weak_ptr<bool> alive = grid_alive_;
    launcher_home_animation::animate_page_slide(
        grid_container_, forward,
        [this, alive]() {
            auto state = alive.lock();
            if (state && *state) finish_page_flip_animation();
        },
        [alive]() {
            auto state = alive.lock();
            return state && *state;
        });
}

void UILaunchPage::finish_page_flip_animation()
{
    if (grid_container_)
        lv_obj_set_x(grid_container_, 0);
    set_grid_clickable(true);
    page_flip_active_ = false;
    apply_grid_highlight();
}

void UILaunchPage::cancel_page_flip_animation()
{
    if (!page_flip_active_) return;
    page_flip_active_ = false;
    if (grid_container_)
        lv_obj_set_x(grid_container_, 0);
}

void UILaunchPage::set_grid_clickable(bool clickable)
{
    for (size_t cell = 0; cell < launcher_grid_layout::kCells; ++cell) {
        if (!grid_elements_[cell]) continue;
        if (clickable)
            lv_obj_add_flag(grid_elements_[cell], LV_OBJ_FLAG_CLICKABLE);
        else
            lv_obj_clear_flag(grid_elements_[cell], LV_OBJ_FLAG_CLICKABLE);
    }
}
