#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"

namespace setting {

void WiFi::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "WiFi";
    m.sub_items = {
        {"Power", true, false, [page]() { page->wifi_.toggle_enable(*page); }},
        {"Scan", false, false, [page]() { page->wifi_.enter_scan(*page); }},
    };
    m.on_enter = [page]() { page->wifi_.refresh_radio(*page); };
    menu.push_back(m);
}

void WiFi::do_scan()
{
    ap_count_ = launcher_wifi::scan(aps_, CP0_WIFI_AP_MAX);
}

void WiFi::enter_scan(UISetupPage &page)
{
    do_scan();
    list_sel_ = 0;
    page.view_state_ = UISetupPage::ViewState::WIFI_LIST;
    build_list(page);
}

void WiFi::refresh_radio(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "WiFi") continue;
        m.sub_items[0].toggle_state = launcher_wifi::radio_enabled() != 0;
        break;
    }
}

void WiFi::toggle_enable(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "WiFi") continue;
        bool on = m.sub_items[0].toggle_state;
        launcher_wifi::radio_set_enabled(on);
        m.sub_items[0].toggle_state = launcher_wifi::radio_enabled() != 0;
        break;
    }
}

void WiFi::build_list(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);

    lv_obj_t *title = lv_label_create(cont);
    {
        cp0_wifi_status_t ws = launcher_wifi::get_status();
        static char title_buf[128];
        if (ws.connected)
            snprintf(title_buf, sizeof(title_buf), "Connected WiFi: %.64s  %.40s", ws.ssid, ws.ip);
        else
            snprintf(title_buf, sizeof(title_buf), "WiFi: Not connected");
        lv_label_set_text(title, title_buf);
    }
    lv_obj_set_pos(title, 8, 2);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
    UISetupPage::apply_overflow_handling(title, 8, UISetupPage::WIFI_TITLE_BOX_W, true);

    lv_obj_t *h1 = lv_label_create(cont);
    lv_label_set_text(h1, "SSID");
    lv_obj_set_pos(h1, 8, 18);
    lv_obj_set_style_text_color(h1, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(h1, &lv_font_montserrat_10, LV_PART_MAIN);

    lv_obj_t *h2 = lv_label_create(cont);
    lv_label_set_text(h2, "Security");
    lv_obj_set_pos(h2, 180, 18);
    lv_obj_set_style_text_color(h2, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(h2, &lv_font_montserrat_10, LV_PART_MAIN);

    lv_obj_t *h3 = lv_label_create(cont);
    lv_label_set_text(h3, "Signal");
    lv_obj_set_pos(h3, 270, 18);
    lv_obj_set_style_text_color(h3, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(h3, &lv_font_montserrat_10, LV_PART_MAIN);

    if (ap_count_ == 0) {
        lv_obj_t *empty = lv_label_create(cont);
        lv_label_set_text(empty, "No networks found. Press R to rescan.");
        lv_obj_set_pos(empty, 8, 50);
        lv_obj_set_style_text_color(empty, lv_color_hex(0x666666), LV_PART_MAIN);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, LV_PART_MAIN);
        return;
    }

    int visible = 5;
    int offset = list_sel_ - visible / 2;
    if (offset < 0) offset = 0;
    if (offset > ap_count_ - visible) offset = ap_count_ - visible;
    if (offset < 0) offset = 0;

    for (int vi = 0; vi < visible && (vi + offset) < ap_count_; ++vi) {
        int ai = vi + offset;
        bool sel = (ai == list_sel_);
        cp0_wifi_ap_t *ap = &aps_[ai];
        int y = 30 + vi * 22;

        if (sel) {
            lv_obj_t *bg = lv_obj_create(cont);
            lv_obj_set_size(bg, UISetupPage::SCREEN_W - 8, 20);
            lv_obj_set_pos(bg, 4, y);
            lv_obj_set_style_radius(bg, 2, LV_PART_MAIN);
            lv_obj_set_style_bg_color(bg, lv_color_hex(0x1F3A5F), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN);
            lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN);
            lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        }

        uint32_t tc = sel ? 0xFFFFFF : 0xCCCCCC;
        if (ap->in_use) tc = 0x58A6FF;

        lv_obj_t *ssid_lbl = lv_label_create(cont);
        static char ssid_buf[CP0_WIFI_SSID_MAX + 4];
        if (has_saved_profile(ap->ssid))
            snprintf(ssid_buf, sizeof(ssid_buf), "%s *", ap->ssid);
        else
            snprintf(ssid_buf, sizeof(ssid_buf), "%s", ap->ssid);
        lv_label_set_text(ssid_lbl, ssid_buf);
        lv_obj_set_pos(ssid_lbl, 8, y + 2);
        lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(tc), LV_PART_MAIN);
        lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_width(ssid_lbl, 165);
        lv_label_set_long_mode(ssid_lbl, LV_LABEL_LONG_CLIP);

        lv_obj_t *sec = lv_label_create(cont);
        lv_label_set_text(sec, ap->security[0] ? ap->security : "Open");
        lv_obj_set_pos(sec, 180, y + 2);
        lv_obj_set_style_text_color(sec, lv_color_hex(tc), LV_PART_MAIN);
        lv_obj_set_style_text_font(sec, &lv_font_montserrat_10, LV_PART_MAIN);

        char sig_buf[16];
        snprintf(sig_buf, sizeof(sig_buf), "%d%%", ap->signal);
        lv_obj_t *sig = lv_label_create(cont);
        lv_label_set_text(sig, sig_buf);
        lv_obj_set_pos(sig, 275, y + 2);
        lv_obj_set_style_text_color(sig, lv_color_hex(tc), LV_PART_MAIN);
        lv_obj_set_style_text_font(sig, &lv_font_montserrat_10, LV_PART_MAIN);
    }

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "OK:connect  R:rescan  D:forget  ESC:back");
    lv_obj_set_pos(hint, 8, UISetupPage::LIST_H - 14);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

void WiFi::handle_list_key(UISetupPage &page, uint32_t key)
{
    switch (key) {
    case KEY_UP:
        if (list_sel_ > 0) { --list_sel_; build_list(page); }
        break;
    case KEY_DOWN:
        if (list_sel_ < ap_count_ - 1) { ++list_sel_; build_list(page); }
        break;
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
    case KEY_LEFT:
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.build_sub_view();
        break;
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
        ret = launcher_wifi::connect(ap->ssid, NULL);
    } else if (has_saved_profile(ap->ssid)) {
        show_connecting(page, ap->ssid);
        ret = launcher_wifi::connect(ap->ssid, NULL);
        if (ret != 0) {
            needs_password = true;
            pw_ssid_ = ap->ssid;
            pw_buf_.clear();
            show_pw_input(page);
        }
    } else {
        needs_password = true;
        pw_ssid_ = ap->ssid;
        pw_buf_.clear();
        show_pw_input(page);
    }
    if (!needs_password) {
        if (ret != 0)
            show_error(page, "Connection failed");
        do_scan();
        build_list(page);
    }
}

void WiFi::show_connecting(UISetupPage &page, const char *ssid)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    static char msg[128];
    snprintf(msg, sizeof(msg), "Connecting to %s...", ssid);
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, msg);
    lv_obj_set_pos(lbl, 8, 60);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_refr_now(NULL);
}

void WiFi::show_error(UISetupPage &page, const char *msg)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, msg);
    lv_obj_set_pos(lbl, 8, 60);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_refr_now(NULL);
    usleep(2000000);
}

void WiFi::forget_selected(UISetupPage &page)
{
    if (list_sel_ < 0 || list_sel_ >= ap_count_) return;
    cp0_wifi_ap_t *ap = &aps_[list_sel_];

    if (!has_saved_profile(ap->ssid)) {
        show_error(page, "No saved password for this network");
        do_scan();
        build_list(page);
        return;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Forget '%s'?", ap->ssid);
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, msg);
    lv_obj_set_pos(lbl, 8, 50);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFAA00), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "OK:confirm  ESC:cancel");
    lv_obj_set_pos(hint, 8, 75);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_refr_now(NULL);

    pw_ssid_ = ap->ssid;
    page.view_state_ = UISetupPage::ViewState::WIFI_PW;
    launcher_wifi::profile_forget(ap->ssid);
    if (ap->in_use)
        launcher_wifi::profile_disconnect_active();

    lv_obj_clean(cont);
    lbl = lv_label_create(cont);
    snprintf(msg, sizeof(msg), "Forgot '%s'", ap->ssid);
    lv_label_set_text(lbl, msg);
    lv_obj_set_pos(lbl, 8, 60);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x33CC33), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_refr_now(NULL);
    usleep(1500000);

    page.view_state_ = UISetupPage::ViewState::WIFI_LIST;
    do_scan();
    build_list(page);
}

bool WiFi::has_saved_profile(const char *ssid)
{
    return launcher_wifi::profile_exists(ssid) != 0;
}

void WiFi::show_pw_input(UISetupPage &page)
{
    page.view_state_ = UISetupPage::ViewState::WIFI_PW;
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);

    lv_obj_t *title = lv_label_create(cont);
    char buf[128];
    snprintf(buf, sizeof(buf), "Connect: %s", pw_ssid_.c_str());
    lv_label_set_text(title, buf);
    lv_obj_set_pos(title, 10, 10);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_t *pw_label = lv_label_create(cont);
    lv_label_set_text(pw_label, "Password:");
    lv_obj_set_pos(pw_label, 10, 35);
    lv_obj_set_style_text_color(pw_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(pw_label, &lv_font_montserrat_12, LV_PART_MAIN);

    pw_input_lbl_ = lv_label_create(cont);
    lv_label_set_text(pw_input_lbl_, "_");
    lv_obj_set_pos(pw_input_lbl_, 90, 35);
    lv_obj_set_width(pw_input_lbl_, 200);
    lv_label_set_long_mode(pw_input_lbl_, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(pw_input_lbl_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(pw_input_lbl_, &lv_font_montserrat_14, LV_PART_MAIN);

    pw_hint_lbl_ = lv_label_create(cont);
    lv_label_set_text(pw_hint_lbl_, "Type pw, OK:connect, ESC:cancel");
    lv_obj_set_pos(pw_hint_lbl_, 10, 65);
    lv_obj_set_style_text_color(pw_hint_lbl_, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(pw_hint_lbl_, &lv_font_montserrat_10, LV_PART_MAIN);
}

void WiFi::handle_pw_key(UISetupPage &page, uint32_t key)
{
    if (key == KEY_ESC) {
        page.view_state_ = UISetupPage::ViewState::WIFI_LIST;
        page.rebuild_view();
        return;
    }
    if (key == KEY_ENTER) {
        if (pw_hint_lbl_) lv_label_set_text(pw_hint_lbl_, "Connecting...");
        lv_refr_now(NULL);
        int ret = launcher_wifi::connect(pw_ssid_.c_str(), pw_buf_.c_str());
        if (ret != 0) {
            launcher_wifi::profile_forget(pw_ssid_.c_str());
            if (pw_hint_lbl_) {
                lv_label_set_text(pw_hint_lbl_, "Failed! Wrong password? Try again.");
                lv_obj_set_style_text_color(pw_hint_lbl_, lv_color_hex(0xFF4444), LV_PART_MAIN);
            }
            pw_buf_.clear();
            pw_update_display();
            return;
        }
        page.view_state_ = UISetupPage::ViewState::WIFI_LIST;
        do_scan();
        page.rebuild_view();
        return;
    }
    if (key == KEY_BACKSPACE) {
        if (!pw_buf_.empty()) pw_buf_.pop_back();
        pw_update_display();
        return;
    }
    if (page.cur_elm_ && page.cur_elm_->utf8[0]) {
        pw_buf_ += page.cur_elm_->utf8;
        pw_update_display();
    }
}

void WiFi::pw_update_display()
{
    if (!pw_input_lbl_) return;
    std::string display = pw_buf_ + "_";
    lv_label_set_text(pw_input_lbl_, display.c_str());
}

} // namespace setting
