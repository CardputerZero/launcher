#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"
#include "setup_page_access.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>

namespace setting {

struct WiFi::ScanState {
    std::mutex mutex;
    std::condition_variable wake;
    WiFi *owner = nullptr;
    UISetupPage *page = nullptr;
    bool stop = false;
    bool requested = false;
};

struct WiFi::ScanResult {
    std::shared_ptr<WiFi::ScanState> state;
    UISetupPage *page = nullptr;
    cp0_wifi_ap_t aps[CP0_WIFI_AP_MAX]{};
    int count = 0;
};

namespace {

constexpr auto kWifiScanPeriod = std::chrono::seconds(8);

} // namespace

WiFi::~WiFi()
{
    shutdown();
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

void WiFi::enter_scan(UISetupPage &page)
{
    stop_connection_failure_feedback();
    clear_password_view();
    SetupPageAccess(page).set_view(SetupViewState::WIFI_LIST);
    refresh_list_status();
    start_scan(page);
    build_list(page);
}

void WiFi::start_scan(UISetupPage &page)
{
    stop_scan();
    auto state = std::make_shared<ScanState>();
    state->owner = this;
    state->page = &page;
    scan_state_ = state;
    list_view_model_.begin_scan();
    scan_threads_.emplace_back([state] {
        for (;;) {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (state->stop) break;
                state->requested = false;
            }

            auto result = std::make_unique<ScanResult>();
            result->state = state;
            result->count = cp0_wifi_scan(result->aps, CP0_WIFI_AP_MAX);

            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (state->stop) break;
                result->page = state->page;
            }
            ScanResult *raw = result.release();
            if (lv_async_call(scan_result_cb, raw) != LV_RESULT_OK)
                delete raw;

            std::unique_lock<std::mutex> lock(state->mutex);
            state->wake.wait_for(lock, kWifiScanPeriod, [&] {
                return state->stop || state->requested;
            });
            if (state->stop) break;
        }
    });
}

void WiFi::stop_scan()
{
    auto state = scan_state_;
    if (state) {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->stop = true;
            state->owner = nullptr;
            state->page = nullptr;
        }
        state->wake.notify_all();
    }
    scan_state_.reset();
    list_view_model_.cancel_scan();
}

void WiFi::shutdown()
{
    stop_scan();
    for (auto &thread : scan_threads_) {
        if (thread.joinable()) thread.join();
    }
    scan_threads_.clear();
}

void WiFi::request_scan()
{
    auto state = scan_state_;
    if (!state) return;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->stop) return;
        state->requested = true;
    }
    list_view_model_.begin_scan();
    state->wake.notify_all();
}

void WiFi::scan_result_cb(void *user) noexcept
{
    std::unique_ptr<ScanResult> result(static_cast<ScanResult *>(user));
    if (!result) return;
    try {
        WiFi *owner = nullptr;
        {
            std::lock_guard<std::mutex> lock(result->state->mutex);
            if (result->state->stop || result->state->page != result->page) return;
            owner = result->state->owner;
        }
        if (!owner || !result->page ||
            !SetupPageAccess(*result->page).is_view(SetupViewState::WIFI_LIST)) return;
        owner->apply_scan_result(*result->page, result->aps, result->count);
    } catch (...) {
    }
}

void WiFi::apply_scan_result(UISetupPage &page, const cp0_wifi_ap_t *aps, int count)
{
    std::vector<SetupWifiAccessPoint> access_points;
    const int safe_count = std::clamp(count, 0, CP0_WIFI_AP_MAX);
    access_points.reserve(static_cast<std::size_t>(safe_count));
    for (int index = 0; aps && index < safe_count; ++index) {
        const auto &source = aps[index];
        access_points.push_back({source.ssid, source.security, source.signal,
                                 source.in_use != 0, source.saved != 0});
    }
    list_view_model_.apply_scan(std::move(access_points));
    refresh_list_status();
    build_list(page);
}

void WiFi::refresh_list_status()
{
    cp0_wifi_status_t status{};
    cp0_wifi_status_read(&status);
    list_view_model_.set_status({status.connected != 0, status.ssid, status.ip});
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
    case KEY_UP:
        if (list_view_model_.move_selection(-1)) build_list(page);
        break;
    case KEY_DOWN:
        if (list_view_model_.move_selection(1)) build_list(page);
        break;
    case KEY_ENTER:
        if (list_view_model_.selected()) try_connect(page, list_view_model_.selected_index());
        break;
    case KEY_R:
        request_scan();
        build_list(page);
        break;
    case KEY_D:
        if (list_view_model_.selected()) forget_selected(page);
        break;
    case KEY_ESC:
    case KEY_LEFT: {
        stop_connection_failure_feedback();
        stop_scan();
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
    const SetupWifiAccessPoint *selected = list_view_model_.at(idx);
    if (!selected || selected->in_use) return;
    const SetupWifiAccessPoint ap = *selected;
    stop_scan();

    bool needs_password = false;
    int ret = -1;
    if (ap.security == "Open" || ap.security.empty()) {
        show_connecting(page, ap.ssid.c_str());
        ret = cp0_wifi_connect(ap.ssid.c_str(), NULL);
    } else if (ap.saved) {
        show_connecting(page, ap.ssid.c_str());
        ret = cp0_wifi_connect(ap.ssid.c_str(), NULL);
        if (ret != 0) {
            needs_password = true;
            password_model_.begin(ap.ssid);
            if (!password_view_.show(page, password_model_.ssid())) {
                password_model_.reset();
                start_scan(page);
                build_list(page);
            }
        }
    } else {
        needs_password = true;
        password_model_.begin(ap.ssid);
        if (!password_view_.show(page, password_model_.ssid())) {
            password_model_.reset();
            start_scan(page);
            build_list(page);
        }
    }
    if (!needs_password) {
        if (ret != 0) {
            show_error(page, "Connection failed");
            start_connection_failure_feedback(page);
            return;
        }
        start_scan(page);
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
    if (SetupPageAccess(page).is_view(SetupViewState::WIFI_LIST)) {
        start_scan(page);
        build_list(page);
    }
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
        self->start_scan(*page);
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
    const SetupWifiAccessPoint *selected = list_view_model_.selected();
    if (!selected) return;
    const SetupWifiAccessPoint ap = *selected;

    if (!ap.saved) {
        stop_scan();
        show_error(page, "No saved password for this network");
        start_connection_failure_feedback(page);
        return;
    }

    const std::string ssid = ap.ssid;
    const bool active = ap.in_use;
    stop_scan();
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
    start_scan(page);
    access.rebuild_view();
}

void WiFi::handle_pw_key(UISetupPage &page, uint32_t key)
{
    if (key == KEY_ESC) {
        clear_password_view();
        password_model_.reset();
        SetupPageAccess access(page);
        access.set_view(SetupViewState::WIFI_LIST);
        start_scan(page);
        access.rebuild_view();
        return;
    }
    if (key == KEY_ENTER) {
        if (!password_model_.can_submit()) {
            password_view_.set_hint("Password required");
            return;
        }
        password_view_.set_hint("Connecting...");
        lv_refr_now(NULL);
        int ret = cp0_wifi_connect(
            password_model_.ssid().c_str(), password_model_.password().c_str());
        if (ret != 0) {
            cp0_wifi_profile_forget(password_model_.ssid().c_str());
            password_view_.set_hint("Failed! Wrong password? Try again.", 0xFF4444);
            password_model_.clear_password();
            password_view_.update_password(password_model_.masked_display());
            return;
        }
        clear_password_view();
        password_model_.reset();
        SetupPageAccess access(page);
        access.set_view(SetupViewState::WIFI_LIST);
        start_scan(page);
        access.rebuild_view();
        return;
    }
    if (key == KEY_BACKSPACE) {
        password_model_.erase_last();
        password_view_.update_password(password_model_.masked_display());
        return;
    }
    const std::string_view input = SetupPageAccess(page).current_utf8();
    if (!input.empty()) {
        if (!password_model_.append(std::string(input))) {
            password_view_.set_hint("Password too long");
            return;
        }
        password_view_.update_password(password_model_.masked_display());
    }
}

void WiFi::clear_password_view()
{
    password_view_.unmount();
}

} // namespace setting
