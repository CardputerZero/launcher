#pragma once

#include "menu_types.hpp"
#include "../../model/setup_wifi_model.hpp"

#include "cp0_lvgl_app.h"
#include <lvgl.h>

#include <cstdint>
#include <string>
#include <vector>

class UISetupPage;

namespace setting {

class WiFi
{
public:
    ~WiFi();
    void append(UISetupPage &page, std::vector<MenuItem> &menu);
    void enter_scan(UISetupPage &page);
    bool build_list(UISetupPage &page);
    void handle_list_key(UISetupPage &page, uint32_t key);
    void refresh_radio(UISetupPage &page);
    void toggle_enable(UISetupPage &page);
    void try_connect(UISetupPage &page, int index);
    bool show_connecting(UISetupPage &page, const char *ssid);
    bool show_error(UISetupPage &page, const char *message);
    bool show_forget_confirmation(UISetupPage &page, const std::string &ssid);
    void forget_selected(UISetupPage &page);
    void handle_forget_key(UISetupPage &page, uint32_t key);
    bool has_saved_profile(const char *ssid);
    bool show_pw_input(UISetupPage &page);
    void handle_pw_key(UISetupPage &page, uint32_t key);
    void pw_update_display();
    void do_scan();

private:
    void clear_password_view();
    void start_connection_failure_feedback(UISetupPage &page);
    void stop_connection_failure_feedback();
    static void password_label_delete_cb(lv_event_t *event) noexcept;
    static void connection_feedback_timer_cb(lv_timer_t *timer) noexcept;
    static void feedback_screen_delete_cb(lv_event_t *event) noexcept;

    cp0_wifi_ap_t aps_[CP0_WIFI_AP_MAX];
    int ap_count_ = 0;
    int list_sel_ = 0;
    SetupWifiPasswordModel password_model_;
    SetupWifiFeedbackModel feedback_model_;
    SetupWifiFeedbackModel::Token feedback_token_ = 0;
    lv_timer_t *feedback_timer_ = nullptr;
    UISetupPage *feedback_page_ = nullptr;
    lv_obj_t *pw_input_lbl_ = nullptr;
    lv_obj_t *pw_hint_lbl_ = nullptr;
    std::string forget_ssid_;
    bool forget_active_ = false;
};

} // namespace setting
