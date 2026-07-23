#define APP_PAGE_IMPLEMENTATION_UNIT
#include "ui_app_mesh.hpp"

#include "input_keys.h"
#include "../keyboard_text_input.hpp"

#include <cstdio>
#include <cstring>

UIMeshPage::UIMeshPage() : AppPage()
{
    set_page_title("MESH");
    generate_node_id();
    create_ui();
    const auto bg = ui_obj_.find("bg");
    if (!ui_ready()) {
        if (bg != ui_obj_.end() && bg->second) lv_obj_delete(bg->second);
        return;
    }
    event_handler_init();
    heartbeat_timer_.start(
        [this] { return lv_timer_create(heartbeat_timer_cb, 8000, this); },
        [](lv_timer_t *timer) { lv_timer_delete(timer); });
}

UIMeshPage::~UIMeshPage()
{
    heartbeat_timer_.stop();
    if (root_screen_)
        lv_obj_remove_event_cb_with_user_data(
            root_screen_, UIMeshPage::static_lvgl_handler, this);
    detach_ui_callbacks();
}

void UIMeshPage::generate_node_id()
{
    uint32_t r = 0;
    cp0_signal_osinfo_api({"RandomU32"}, [&](int code, std::string data) {
        uint32_t parsed = 0;
        if (code == 0 && mesh_parse_random_u32(data, parsed)) r = parsed;
    });
    model_.initialize(r);
}

std::string UIMeshPage::current_time()
{
    int hour = 0;
    int minute = 0;
    int second = 0;
    bool valid = false;
    cp0_signal_osinfo_api({"LocalTime"}, [&](int code, std::string data) {
        valid = code == 0 && mesh_parse_local_time(data, hour, minute, second);
    });
    if (!valid) return "--:--:--";
    char timestamp[16];
    snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d", hour, minute, second);
    return timestamp;
}

void UIMeshPage::show_input_overlay()
{
    const auto bg_entry = ui_obj_.find("bg");
    if (bg_entry == ui_obj_.end() || !bg_entry->second || input_overlay_) return;
    lv_obj_t *bg = bg_entry->second;
    input_overlay_ = lv_obj_create(bg);
    if (!input_overlay_) return;
    lv_obj_add_event_cb(input_overlay_, owned_obj_delete_cb, LV_EVENT_DELETE, this);
    view_state_ = ViewState::INPUT;
    msg_input_buf_.clear();
    lv_obj_set_size(input_overlay_, 280, 50);
    lv_obj_set_pos(input_overlay_, 20, 50);
    lv_obj_set_style_radius(input_overlay_, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(input_overlay_, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(input_overlay_, 240, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(input_overlay_, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(input_overlay_, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(input_overlay_, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(input_overlay_, LV_OBJ_FLAG_SCROLLABLE);
    make_label(input_overlay_, "Send MESH Message:", 0, 0, 0x58A6FF, &lv_font_montserrat_10);
    msg_input_lbl_ = make_label(input_overlay_, "_", 0, 14, 0xFFFFFF, &lv_font_montserrat_12);
    if (!msg_input_lbl_) {
        close_input_overlay();
        return;
    }
    lv_obj_add_event_cb(msg_input_lbl_, owned_obj_delete_cb, LV_EVENT_DELETE, this);
    lv_obj_set_width(msg_input_lbl_, 260);
    lv_label_set_long_mode(msg_input_lbl_, LV_LABEL_LONG_CLIP);
    make_label(input_overlay_, "OK:send  ESC:cancel", 0, 30, 0x888888, &lv_font_montserrat_10);
}

void UIMeshPage::close_input_overlay()
{
    if (input_overlay_) {
        lv_obj_del(input_overlay_);
        input_overlay_ = nullptr;
    }
    msg_input_lbl_ = nullptr;
    view_state_ = ViewState::MAIN;
}

void UIMeshPage::update_input_display()
{
    if (!msg_input_lbl_) return;
    std::string display = msg_input_buf_ + "_";
    lv_label_set_text(msg_input_lbl_, display.c_str());
}

void UIMeshPage::do_refresh()
{
    model_.refresh(current_time());
    build_neighbor_list();
    build_message_list();
}

void UIMeshPage::heartbeat_timer_cb(lv_timer_t *timer) noexcept
{
    UIMeshPage *self = static_cast<UIMeshPage *>(lv_timer_get_user_data(timer));
    if (!self) return;
    try {
        if (mesh_heartbeat_callback_allowed(
                self->heartbeat_timer_.current(timer), self->heartbeat_enabled_))
            self->on_heartbeat();
    } catch (...) {
        self->heartbeat_enabled_ = false;
    }
}

void UIMeshPage::on_heartbeat()
{
    model_.heartbeat(current_time());
    build_neighbor_list();
    build_message_list();
}

void UIMeshPage::event_handler_init()
{
    if (!root_screen_) return;
    lv_obj_add_event_cb(root_screen_, UIMeshPage::static_lvgl_handler, LV_EVENT_ALL, this);
}

bool UIMeshPage::ui_ready() const
{
    const auto bg = ui_obj_.find("bg");
    return bg != ui_obj_.end() && mesh_page_ui_ready(
        bg->second, neighbor_area_, msg_area_, status_lbl_, hint_lbl_);
}

void UIMeshPage::detach_ui_callbacks()
{
    lv_obj_t *objects[] = {msg_input_lbl_, input_overlay_, neighbor_area_, msg_area_,
                           status_lbl_, hint_lbl_};
    for (lv_obj_t *object : objects)
        if (object)
            lv_obj_remove_event_cb_with_user_data(object, owned_obj_delete_cb, this);
    const auto bg = ui_obj_.find("bg");
    if (bg != ui_obj_.end() && bg->second)
        lv_obj_remove_event_cb_with_user_data(bg->second, owned_obj_delete_cb, this);
}

void UIMeshPage::owned_obj_delete_cb(lv_event_t *event) noexcept
{
    if (!event || !mesh_owned_delete_callback_allowed(
            lv_event_get_target(event), lv_event_get_current_target(event))) return;
    auto *self = static_cast<UIMeshPage *>(lv_event_get_user_data(event));
    if (!self) return;
    try {
        auto *deleted = static_cast<lv_obj_t *>(lv_event_get_target(event));

        if (self->input_overlay_ == deleted) {
            self->input_overlay_ = nullptr;
            self->msg_input_lbl_ = nullptr;
            self->msg_input_buf_.clear();
            self->view_state_ = ViewState::MAIN;
        }
        if (self->msg_input_lbl_ == deleted) self->msg_input_lbl_ = nullptr;
        if (self->status_lbl_ == deleted) self->status_lbl_ = nullptr;
        if (self->hint_lbl_ == deleted) self->hint_lbl_ = nullptr;
        if (self->neighbor_area_ == deleted) {
            self->neighbor_area_ = nullptr;
            self->heartbeat_timer_.stop();
        }
        if (self->msg_area_ == deleted) {
            self->msg_area_ = nullptr;
            self->heartbeat_timer_.stop();
        }
        const auto bg = self->ui_obj_.find("bg");
        if (bg != self->ui_obj_.end() && bg->second == deleted) {
            self->heartbeat_timer_.stop();
            self->input_overlay_ = nullptr;
            self->msg_input_lbl_ = nullptr;
            self->msg_area_ = nullptr;
            self->neighbor_area_ = nullptr;
            self->status_lbl_ = nullptr;
            self->hint_lbl_ = nullptr;
            self->ui_obj_.clear();
            self->msg_input_buf_.clear();
            self->view_state_ = ViewState::MAIN;
        }
        if (!self->ui_ready()) self->heartbeat_timer_.stop();
    } catch (...) {
        self->heartbeat_enabled_ = false;
    }
}

void UIMeshPage::static_lvgl_handler(lv_event_t *e) noexcept
{
    UIMeshPage *self = nullptr;
    try {
        if (!e) return;
        self = static_cast<UIMeshPage *>(lv_event_get_user_data(e));
        if (!self || !mesh_root_event_callback_allowed(
                lv_event_get_current_target(e), self->root_screen_))
            return;
        self->event_handler(e);
    } catch (...) {
        if (self) self->heartbeat_enabled_ = false;
    }
}

void UIMeshPage::event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_DELETE &&
        lv_event_get_target(e) == lv_event_get_current_target(e)) {
        heartbeat_timer_.stop();
        input_overlay_ = nullptr;
        msg_input_lbl_ = nullptr;
        msg_area_ = nullptr;
        neighbor_area_ = nullptr;
        return;
    }
    if (!launcher_ui::events::is_key_released(e)) return;
    uint32_t key = launcher_ui::events::keyboard_key(e);
    switch (view_state_) {
    case ViewState::MAIN: handle_main_key(key); break;
    case ViewState::INPUT: handle_input_key(key, launcher_ui::events::keyboard_item(e)); break;
    }
}

void UIMeshPage::handle_main_key(uint32_t key)
{
    switch (key) {
    case KEY_S: show_input_overlay(); break;
    case KEY_R: do_refresh(); break;
    case KEY_UP:
        if (msg_area_) lv_obj_scroll_by(msg_area_, 0, 20, LV_ANIM_ON);
        break;
    case KEY_DOWN:
        if (msg_area_) lv_obj_scroll_by(msg_area_, 0, -20, LV_ANIM_ON);
        break;
    case KEY_ESC:
        heartbeat_timer_.stop();
        if (navigate_home) navigate_home();
        break;
    default: break;
    }
}

void UIMeshPage::handle_input_key(uint32_t key, const struct key_item *elm)
{
    if (key == KEY_ESC) {
        close_input_overlay();
        return;
    }
    if (key == KEY_ENTER) {
        if (!msg_input_buf_.empty()) add_message("ME", msg_input_buf_.c_str());
        close_input_overlay();
        return;
    }
    if (key == KEY_BACKSPACE) {
        launcher_ui::text_input::erase_last_codepoint(msg_input_buf_);
        update_input_display();
        return;
    }
    if (launcher_ui::text_input::append_limited(
            msg_input_buf_, launcher_ui::text_input::key_text(key, elm), 40)) {
        update_input_display();
    }
}
