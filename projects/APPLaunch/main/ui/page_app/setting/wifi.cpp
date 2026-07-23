#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"
#include "setup_page_access.hpp"

namespace setting {

WiFi::~WiFi()
{
    stop_connection_failure_feedback();
    clear_password_view();
    password_model_.reset();
    forget_ssid_.clear();
    forget_active_ = false;
}

void WiFi::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    WiFi *controller = this;
    MenuItem m;
    m.label = "WiFi";
    m.sub_items = {
        {"Power", true, false, [controller, page]() { controller->toggle_enable(*page); }},
        {"Scan", false, false, [controller, page]() { controller->enter_scan(*page); }},
    };
    m.on_enter = [controller, page]() { controller->refresh_radio(*page); };
    menu.push_back(m);
}

void WiFi::do_scan()
{
    ap_count_ = cp0_wifi_scan(aps_, CP0_WIFI_AP_MAX);
}

void WiFi::enter_scan(UISetupPage &page)
{
    stop_connection_failure_feedback();
    clear_password_view();
    do_scan();
    list_sel_ = 0;
    SetupPageAccess(page).set_view(SetupViewState::WIFI_LIST);
    build_list(page);
}

void WiFi::refresh_radio(UISetupPage &page)
{
    MenuItem *menu = SetupPageAccess(page).find_menu("WiFi");
    if (menu && !menu->sub_items.empty())
        menu->sub_items[0].toggle_state = cp0_wifi_radio_enabled() != 0;
}

void WiFi::toggle_enable(UISetupPage &page)
{
    MenuItem *menu = SetupPageAccess(page).find_menu("WiFi");
    if (menu && !menu->sub_items.empty()) {
        bool on = menu->sub_items[0].toggle_state;
        cp0_wifi_radio_set_enabled(on);
        menu->sub_items[0].toggle_state = cp0_wifi_radio_enabled() != 0;
    }
}

void WiFi::handle_list_key(UISetupPage &page, uint32_t key)
{
    if (feedback_model_.pending() && key != KEY_ESC && key != KEY_LEFT)
        return;
    switch (key) {
    case KEY_UP: {
        if (list_sel_ > 0) {
            const int previous = list_sel_--;
            if (!build_list(page)) list_sel_ = previous;
        }
        break;
    }
    case KEY_DOWN: {
        if (list_sel_ < ap_count_ - 1) {
            const int previous = list_sel_++;
            if (!build_list(page)) list_sel_ = previous;
        }
        break;
    }
    case KEY_ENTER:
        if (ap_count_ > 0) try_connect(page, list_sel_);
        break;
    case KEY_R:
        do_scan();
        list_sel_ = 0;
        build_list(page);
        break;
    case KEY_D:
        if (ap_count_ > 0) forget_selected(page);
        break;
    case KEY_ESC:
    case KEY_LEFT: {
        stop_connection_failure_feedback();
        SetupPageAccess access(page);
        access.set_view(SetupViewState::SUB);
        access.build_sub_view();
        break;
    }
    default:
        break;
    }
}

void WiFi::try_connect(UISetupPage &page, int idx)
{
    if (idx < 0 || idx >= ap_count_) return;
    cp0_wifi_ap_t *ap = &aps_[idx];
    if (ap->in_use) return;

    bool needs_password = false;
    int ret = -1;
    if (strcmp(ap->security, "Open") == 0 || ap->security[0] == 0) {
        show_connecting(page, ap->ssid);
        ret = cp0_wifi_connect(ap->ssid, NULL);
    } else if (has_saved_profile(ap->ssid)) {
        show_connecting(page, ap->ssid);
        ret = cp0_wifi_connect(ap->ssid, NULL);
        if (ret != 0) {
            needs_password = true;
            password_model_.begin(ap->ssid);
            if (!show_pw_input(page)) password_model_.reset();
        }
    } else {
        needs_password = true;
        password_model_.begin(ap->ssid);
        if (!show_pw_input(page)) password_model_.reset();
    }
    if (!needs_password) {
        if (ret != 0) {
            show_error(page, "Connection failed");
            start_connection_failure_feedback(page);
            return;
        }
        do_scan();
        build_list(page);
    }
}

void WiFi::start_connection_failure_feedback(UISetupPage &page)
{
    stop_connection_failure_feedback();
    feedback_token_ = feedback_model_.begin();
    if (!feedback_token_) return;
    feedback_page_ = &page;
    feedback_timer_ = lv_timer_create(connection_feedback_timer_cb, 2000, this);
    if (feedback_timer_) {
        lv_timer_set_repeat_count(feedback_timer_, 1);
        if (page.screen())
            lv_obj_add_event_cb(
                page.screen(), feedback_screen_delete_cb, LV_EVENT_DELETE, this);
        return;
    }

    feedback_page_ = nullptr;
    feedback_model_.complete(feedback_token_);
    feedback_token_ = 0;
    do_scan();
    if (SetupPageAccess(page).is_view(SetupViewState::WIFI_LIST)) build_list(page);
}

void WiFi::stop_connection_failure_feedback()
{
    if (feedback_page_ && feedback_page_->screen())
        lv_obj_remove_event_cb_with_user_data(
            feedback_page_->screen(), feedback_screen_delete_cb, this);
    if (feedback_timer_) {
        lv_timer_delete(feedback_timer_);
        feedback_timer_ = nullptr;
    }
    feedback_page_ = nullptr;
    feedback_model_.cancel(feedback_token_);
    feedback_token_ = 0;
}

void WiFi::connection_feedback_timer_cb(lv_timer_t *timer) noexcept
{
    auto *self = static_cast<WiFi *>(lv_timer_get_user_data(timer));
    if (!self || !setup_wifi_feedback_timer_callback_ready(
            timer, self->feedback_timer_, self->feedback_token_)) return;
    const auto token = self->feedback_token_;
    try {
        UISetupPage *page = self->feedback_page_;
        if (page && page->screen())
            lv_obj_remove_event_cb_with_user_data(
                page->screen(), feedback_screen_delete_cb, self);
        self->feedback_timer_ = nullptr;
        self->feedback_page_ = nullptr;
        self->feedback_token_ = 0;
        if (!self->feedback_model_.complete(token) || !page) return;
        if (!SetupPageAccess(*page).is_view(SetupViewState::WIFI_LIST)) return;
        self->do_scan();
        self->build_list(*page);
    } catch (...) {
        self->feedback_timer_ = nullptr;
        self->feedback_page_ = nullptr;
        self->feedback_token_ = 0;
        self->feedback_model_.cancel(token);
    }
}

void WiFi::feedback_screen_delete_cb(lv_event_t *event) noexcept
{
    if (!event) return;
    auto *self = static_cast<WiFi *>(lv_event_get_user_data(event));
    if (!self) return;
    lv_obj_t *tracked_screen = self->feedback_page_
        ? self->feedback_page_->screen() : nullptr;
    if (!setup_wifi_feedback_screen_delete_allowed(
            lv_event_get_target(event), lv_event_get_current_target(event),
            tracked_screen)) return;

    lv_timer_t *timer = self->feedback_timer_;
    self->feedback_page_ = nullptr;
    self->feedback_timer_ = nullptr;
    self->feedback_model_.cancel(self->feedback_token_);
    self->feedback_token_ = 0;
    if (timer) {
        try {
            lv_timer_delete(timer);
        } catch (...) {
        }
    }
}

void WiFi::forget_selected(UISetupPage &page)
{
    if (list_sel_ < 0 || list_sel_ >= ap_count_) return;
    cp0_wifi_ap_t *ap = &aps_[list_sel_];

    if (!has_saved_profile(ap->ssid)) {
        show_error(page, "No saved password for this network");
        return;
    }

    const std::string ssid = ap->ssid;
    const bool active = ap->in_use;
    if (!show_forget_confirmation(page, ssid)) return;

    forget_ssid_ = ssid;
    forget_active_ = active;
    SetupPageAccess(page).set_view(SetupViewState::WIFI_FORGET_CONFIRM);
}

void WiFi::handle_forget_key(UISetupPage &page, uint32_t key)
{
    if (key != KEY_ENTER && key != KEY_ESC && key != KEY_LEFT) return;

    if (key == KEY_ENTER && !forget_ssid_.empty()) {
        cp0_wifi_profile_forget(forget_ssid_.c_str());
        if (forget_active_) cp0_wifi_disconnect_active();
    }
    forget_ssid_.clear();
    forget_active_ = false;
    SetupPageAccess access(page);
    access.set_view(SetupViewState::WIFI_LIST);
    do_scan();
    access.rebuild_view();
}

bool WiFi::has_saved_profile(const char *ssid)
{
    return cp0_wifi_profile_exists(ssid) != 0;
}

void WiFi::handle_pw_key(UISetupPage &page, uint32_t key)
{
    if (key == KEY_ESC) {
        clear_password_view();
        password_model_.reset();
        SetupPageAccess access(page);
        access.set_view(SetupViewState::WIFI_LIST);
        access.rebuild_view();
        return;
    }
    if (key == KEY_ENTER) {
        if (!password_model_.can_submit()) {
            if (pw_hint_lbl_) lv_label_set_text(pw_hint_lbl_, "Password required");
            return;
        }
        if (pw_hint_lbl_) lv_label_set_text(pw_hint_lbl_, "Connecting...");
        lv_refr_now(NULL);
        int ret = cp0_wifi_connect(
            password_model_.ssid().c_str(), password_model_.password().c_str());
        if (ret != 0) {
            cp0_wifi_profile_forget(password_model_.ssid().c_str());
            if (pw_hint_lbl_) {
                lv_label_set_text(pw_hint_lbl_, "Failed! Wrong password? Try again.");
                lv_obj_set_style_text_color(pw_hint_lbl_, lv_color_hex(0xFF4444), LV_PART_MAIN);
            }
            password_model_.clear_password();
            pw_update_display();
            return;
        }
        clear_password_view();
        password_model_.reset();
        SetupPageAccess access(page);
        access.set_view(SetupViewState::WIFI_LIST);
        do_scan();
        access.rebuild_view();
        return;
    }
    if (key == KEY_BACKSPACE) {
        password_model_.erase_last();
        pw_update_display();
        return;
    }
    const std::string_view input = SetupPageAccess(page).current_utf8();
    if (!input.empty()) {
        if (!password_model_.append(std::string(input))) {
            if (pw_hint_lbl_) lv_label_set_text(pw_hint_lbl_, "Password too long");
            return;
        }
        pw_update_display();
    }
}

void WiFi::clear_password_view()
{
    if (pw_input_lbl_)
        lv_obj_remove_event_cb_with_user_data(
            pw_input_lbl_, password_label_delete_cb, this);
    if (pw_hint_lbl_)
        lv_obj_remove_event_cb_with_user_data(
            pw_hint_lbl_, password_label_delete_cb, this);
    pw_input_lbl_ = nullptr;
    pw_hint_lbl_ = nullptr;
}

void WiFi::password_label_delete_cb(lv_event_t *event) noexcept
{
    try {
        if (!event) return;
        auto *self = static_cast<WiFi *>(lv_event_get_user_data(event));
        auto *deleted = static_cast<lv_obj_t *>(lv_event_get_target(event));
        auto *current = static_cast<lv_obj_t *>(lv_event_get_current_target(event));
        if (!self || !deleted || deleted != current) return;
        if (setup_wifi_owned_label_delete_matches(
                deleted, current, self->pw_input_lbl_))
            self->pw_input_lbl_ = nullptr;
        if (setup_wifi_owned_label_delete_matches(
                deleted, current, self->pw_hint_lbl_))
            self->pw_hint_lbl_ = nullptr;
    } catch (...) {
    }
}

} // namespace setting
