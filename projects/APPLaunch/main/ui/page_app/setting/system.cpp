#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"

namespace setting {

Developer::Developer() : async_state_(std::make_shared<AsyncState>()) {}

Developer::~Developer()
{
    async_state_->alive = false;
    if (async_state_->request_id) cp0_sudo_cancel(async_state_->request_id);
    close_status();
}

void Developer::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Developer";
    bool adb_en = UISetupPage::config_get_int("adb_debug", 0) != 0;
    m.sub_items = {{"ADB", true, adb_en, [page]() { page->developer_.toggle_adb(*page); }}};
    m.on_enter = [page]() { page->developer_.refresh_adb_status(*page); };
    menu.push_back(m);
}

void Developer::toggle_adb(UISetupPage &page)
{
    int idx = page.find_menu("Developer");
    if (idx < 0) return;
    if (async_state_->request_id) {
        page.menu_items_[idx].sub_items[0].toggle_state =
            !page.menu_items_[idx].sub_items[0].toggle_state;
        return;
    }
    bool want_on = page.menu_items_[idx].sub_items[0].toggle_state;
    const bool previous = !want_on;
    show_status(want_on ? "Enabling ADB" : "Disabling ADB", "Please wait...", Modal::BUSY);
    struct Context {
        Developer *developer;
        UISetupPage *page;
        std::weak_ptr<AsyncState> state;
        bool desired;
        bool previous;
    };
    auto *ctx = new (std::nothrow) Context{this, &page, async_state_, want_on, previous};
    if (!ctx) {
        update_toggle(page, previous, false);
        show_status("ADB update failed", "Out of memory", Modal::ERROR);
        return;
    }
    const char *argv[] = {kAdbHelper, want_on ? "enable" : "disable", nullptr};
    uint64_t request_id = 0;
    int rc = cp0_sudo_run_argv_async_ex(argv, CP0_SUDO_CALLBACK_LVGL, nullptr,
        [](cp0_sudo_result_t result, int exit_code, void *user) {
            std::unique_ptr<Context> ctx(static_cast<Context *>(user));
            auto state = ctx->state.lock();
            if (!state || !state->alive) return;
            state->request_id = 0;
            if (adb_toggle_succeeded(static_cast<int>(result), exit_code)) {
                ctx->developer->update_toggle(*ctx->page, ctx->desired, true);
                ctx->developer->close_status();
                if (adb_reboot_required(static_cast<int>(result), exit_code))
                    ctx->developer->enter_usb_guide(*ctx->page, ctx->desired);
                else
                    ctx->page->build_sub_view();
                return;
            }
            ctx->developer->update_toggle(*ctx->page, ctx->previous, false);
            if (result == CP0_SUDO_RESULT_CANCELLED) {
                ctx->developer->close_status();
                ctx->page->build_sub_view();
            } else {
                ctx->developer->show_result_error(result, exit_code);
            }
        }, ctx, 60000, 300000, &request_id);
    if (rc != 0) {
        delete ctx;
        update_toggle(page, previous, false);
        show_status("ADB update failed", "Unable to start request", Modal::ERROR);
        return;
    }
    async_state_->request_id = request_id;
}

void Developer::refresh_adb_status(UISetupPage &page)
{
    int idx = page.find_menu("Developer");
    if (idx < 0) return;
    int rc = -1;
    std::string out;
    cp0_signal_process_api({"CaptureArgv", kAdbHelper, "status"},
                           [&](int code, std::string data) { rc = code; out = std::move(data); });
    if (rc != 0) return;
    AdbStatus status = parse_adb_status(out.c_str());
    if (status.valid) update_toggle(page, status.active || status.enabled, true);
}

void Developer::update_toggle(UISetupPage &page, bool enabled, bool save)
{
    int idx = page.find_menu("Developer");
    if (idx < 0) return;
    page.menu_items_[idx].sub_items[0].toggle_state = enabled;
    if (save) {
        UISetupPage::config_set_int("adb_debug", enabled ? 1 : 0);
        UISetupPage::config_save();
    }
}

void Developer::show_result_error(cp0_sudo_result_t result, int exit_code)
{
    const char *reason = "Command failed";
    switch (classify_privileged_result(static_cast<int>(result))) {
    case PrivilegedResultKind::AUTH_FAILED: reason = "Authentication failed"; break;
    case PrivilegedResultKind::CANCELLED: reason = "Request cancelled"; break;
    case PrivilegedResultKind::TIMED_OUT: reason = "Request timed out"; break;
    case PrivilegedResultKind::EXEC_FAILED:
        reason = exit_code ? "Command returned an error" : "Unable to start command";
        break;
    default: break;
    }
    show_status("ADB update failed", reason, Modal::ERROR);
}

void Developer::show_status(const char *title_text, const char *detail_text, Modal modal)
{
    close_status();
    modal_ = modal;
    status_overlay_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(status_overlay_);
    lv_obj_set_size(status_overlay_, UISetupPage::SCREEN_W, UISetupPage::SCREEN_H + 20);
    lv_obj_set_style_bg_color(status_overlay_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(status_overlay_, LV_OPA_60, 0);
    lv_obj_clear_flag(status_overlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *box = guide_chip(status_overlay_, 35, 69, 250, 82, 0x1A1A2E,
                               modal == Modal::ERROR ? 0xCC5555 : 0x3A5A8A, 6, 1);
    lv_obj_t *title = guide_label(box, 10, 10, title_text, 0xFFFFFF,
        launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD));
    lv_obj_set_width(title, 230);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *detail = guide_label(box, 10, 39, detail_text, 0xAAAAAA, &lv_font_montserrat_10);
    lv_obj_set_width(detail, 230);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    if (modal == Modal::ERROR) {
        lv_obj_t *hint = guide_label(box, 10, 64, "OK / ESC: close", 0x777777,
                                     &lv_font_montserrat_10);
        lv_obj_set_width(hint, 230);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_move_foreground(status_overlay_);
}

void Developer::close_status()
{
    if (status_overlay_) {
        lv_obj_del(status_overlay_);
        status_overlay_ = nullptr;
    }
    modal_ = Modal::NONE;
}

void Developer::handle_status_key(UISetupPage &page, uint32_t key)
{
    if (modal_ == Modal::BUSY) return;
    if (key == KEY_ENTER || key == KEY_ESC || key == KEY_LEFT) {
        close_status();
        page.build_sub_view();
    }
}

void Developer::enter_usb_guide(UISetupPage &page, bool enabling)
{
    usb_guide_enabling_ = enabling;
    build_usb_guide_view(page);
}

lv_obj_t *Developer::guide_chip(lv_obj_t *parent, int x, int y, int w, int h,
                                uint32_t bg, uint32_t border, int radius, int border_w)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(o, lv_color_hex(border), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, border_w, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

lv_obj_t *Developer::guide_label(lv_obj_t *parent, int x, int y, const char *txt,
                                 uint32_t color, const lv_font_t *font)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    return l;
}

void Developer::build_usb_guide_view(UISetupPage &page)
{
    page.view_state_ = UISetupPage::ViewState::USB_GUIDE;
    usb_guide_knob_ = nullptr;
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    const bool en = usb_guide_enabling_;
    const uint32_t C_GREEN = 0x46DC87, C_YEL = 0xF0C850, C_RED = 0xEB5F5F;
    const uint32_t C_WHITE = 0xECECEC, C_GREY = 0x9A9AA0;
    const lv_font_t *f_title = launcher_fonts().get("Montserrat-Bold.ttf", 13, LV_FREETYPE_FONT_STYLE_BOLD);
    const lv_font_t *f_msg = &lv_font_montserrat_10;

    guide_label(cont, 8, 2, en ? "Enable ADB - switch USB to device" : "Disable ADB - switch USB to HUB",
                C_WHITE, f_title ? f_title : &lv_font_montserrat_12);
    guide_chip(cont, 86, 24, 146, 50, 0x282A30, 0x5A5C64, 6, 2);
    guide_label(cont, 120, 28, "CardputerZero", C_GREY, f_msg);
    guide_chip(cont, 218, 30, 12, 12, 0x101012, 0x5A5C64, 3, 2);
    guide_chip(cont, 228, 32, 22, 8, 0xCDCDD2, 0xCDCDD2, 2, 0);
    guide_chip(cont, 250, 34, 60, 4, 0x6A6C72, 0x6A6C72, 2, 0);
    guide_label(cont, 232, 42, "USB-C", C_GREEN, f_msg);
    guide_chip(cont, 24, 28, 32, 44, 0x1A1A1C, 0x5A5C64, 6, 2);
    guide_chip(cont, 33, 33, 14, 34, 0x0E0E10, 0x0E0E10, 4, 0);
    guide_label(cont, 26, 14, "HUB", en ? C_RED : C_GREEN, f_msg);
    guide_label(cont, 28, 72, "USB", en ? C_GREEN : C_GREY, f_msg);
    const int thumb_top = 34, thumb_bot = 54;
    usb_guide_knob_ = guide_chip(cont, 32, en ? thumb_top : thumb_bot, 16, 10, C_GREEN, 0x2A6F49, 3, 1);

    int y = 80;
    if (en) {
        guide_label(cont, 8, y,      "1  Slide LEFT switch  HUB -> USB", C_WHITE, f_msg);
        guide_label(cont, 8, y + 15, "2  USB hub & peripherals turn OFF", C_YEL, f_msg);
        guide_label(cont, 8, y + 30, "3  Cable -> top-right USB-C port", C_GREEN, f_msg);
    } else {
        guide_label(cont, 8, y,      "1  Slide LEFT switch  USB -> HUB", C_WHITE, f_msg);
        guide_label(cont, 8, y + 15, "2  USB hub & peripherals come back", C_GREEN, f_msg);
        guide_label(cont, 8, y + 30, "3  Reboot to apply the change", C_YEL, f_msg);
    }
    guide_label(cont, 8, UISetupPage::LIST_H - 16, "OK: reboot now     ESC: later", C_GREY, &lv_font_montserrat_10);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, usb_guide_knob_);
    lv_anim_set_values(&a, en ? thumb_top : thumb_bot, en ? thumb_bot : thumb_top);
    lv_anim_set_time(&a, 650);
    lv_anim_set_playback_time(&a, 650);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, [](void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); });
    lv_anim_start(&a);
}

void Developer::stop_usb_guide_anims()
{
    if (usb_guide_knob_) lv_anim_del(usb_guide_knob_, nullptr);
    usb_guide_knob_ = nullptr;
}

void Developer::handle_usb_guide_key(UISetupPage &page, uint32_t key)
{
    switch (key) {
    case KEY_ENTER:
    case KEY_RIGHT: {
        stop_usb_guide_anims();
        lv_obj_t *cont = page.ui_obj_["list_cont"];
        lv_obj_clean(cont);
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, "Rebooting...");
        lv_obj_center(lbl);
        cp0_system_reboot();
        break;
    }
    case KEY_ESC:
    case KEY_LEFT:
        stop_usb_guide_anims();
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.build_sub_view();
        break;
    default:
        break;
    }
}

void About::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "About";
    m.sub_items = {
        {"CardputerZero", false, false, nullptr},
        {"LVGL 9.x", false, false, nullptr},
        {"", false, false, nullptr},
        {"", false, false, nullptr},
    };
    m.on_enter = [page]() { About::refresh_info(*page); };
    menu.push_back(m);
}

void About::refresh_info(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "About") continue;
        m.sub_items[0].label = "M5CardputerZero";
        m.sub_items[1].label = "LVGL: 9.x";
        char buf[64];
        snprintf(buf, sizeof(buf), "Build: %s", __DATE__);
        m.sub_items[2].label = buf;
        snprintf(buf, sizeof(buf), "Commit: %s", LAUNCHER_GIT_COMMIT);
        m.sub_items[3].label = buf;
        break;
    }
}

void Help::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Help";
    m.sub_items = {{"View Help", false, false, [page]() { Help::enter_page(*page); }}};
    menu.push_back(m);
}

void Help::enter_page(UISetupPage &page)
{
    page.view_state_ = UISetupPage::ViewState::WIFI_LIST;
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);

    int y = 4;
    auto add_line = [&](const char *text, uint32_t color, const lv_font_t *font) {
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, text);
        lv_obj_set_pos(lbl, 8, y);
        lv_obj_set_width(lbl, 300);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
        lv_obj_update_layout(lbl);
        y += lv_obj_get_height(lbl) + 3;
    };

    add_line("Help", 0x58A6FF, launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD));
    add_line("Screenshot: Ctrl+Alt+S", 0xCCCCCC, &lv_font_montserrat_12);
    add_line("  Saved to ~/Screenshots", 0x888888, &lv_font_montserrat_10);
    add_line("Home: Hold ESC 3s", 0xCCCCCC, &lv_font_montserrat_12);
    add_line("Navigate: Arrow keys / OK / ESC", 0xCCCCCC, &lv_font_montserrat_12);
    add_line("WiFi: Setting > WiFi > Scan", 0xCCCCCC, &lv_font_montserrat_12);

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "ESC: back");
    lv_obj_set_pos(hint, 8, UISetupPage::LIST_H - 14);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

void ExtPort::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "ExtPort";
    bool usb_en = UISetupPage::config_get_int("extport_usb", 1) != 0;
    bool vout_en = UISetupPage::config_get_int("extport_5vout", 1) != 0;
    m.sub_items = {
        {"GROVE5V", true, usb_en, [page]() {
            bool en = page->menu_items_[page->selected_idx_].sub_items[0].toggle_state;
            UISetupPage::config_set_int("extport_usb", en ? 1 : 0);
            UISetupPage::gpio_set("GROVE5V", en ? 1 : 0);
            UISetupPage::config_save();
        }},
        {"EXT5V", true, vout_en, [page]() {
            bool en = page->menu_items_[page->selected_idx_].sub_items[1].toggle_state;
            UISetupPage::config_set_int("extport_5vout", en ? 1 : 0);
            UISetupPage::gpio_set("EXT5V", en ? 1 : 0);
            UISetupPage::config_save();
        }},
    };
    menu.push_back(m);
}

void Ethernet::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Ethernet";
    m.sub_items = {
        {"IP: --", false, false, nullptr},
        {"Gateway: --", false, false, nullptr},
        {"MAC: --", false, false, nullptr},
    };
    m.on_enter = [page]() { Ethernet::refresh_info(*page); };
    menu.push_back(m);
}

void Ethernet::refresh_info(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Ethernet") continue;
        cp0_eth_info_t info;
        cp0_network_default_info_read(&info);
        m.sub_items[0].label = std::string("IP: ") + info.ipv4;
        m.sub_items[1].label = std::string("GW: ") + info.gateway;
        m.sub_items[2].label = std::string("MAC: ") + info.mac;
        break;
    }
}

void Account::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Account";
    m.sub_items = {
        {"Username", false, false, nullptr},
        {"Password", false, false, nullptr},
        {"Hostname", false, false, nullptr},
    };
    m.on_enter = [page]() { Account::refresh_info(*page); };
    menu.push_back(m);
}

void Account::refresh_info(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Account") continue;
        cp0_account_info_t info;
        cp0_account_info_read(&info);
        m.sub_items[0].label = std::string("User: ") + info.user;
        m.sub_items[1].label = "Password: ****";
        m.sub_items[2].label = std::string("Host: ") + info.hostname;
        break;
    }
}

void Update::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Update";
    m.sub_items = {
        {"Check System", false, false, []() { Update::check_system_update(); }},
        {"Update Launcher", false, false, []() { Update::update_launcher(); }},
        {"Version: --", false, false, nullptr},
    };
    m.on_enter = [page]() { Update::refresh_version_info(*page); };
    menu.push_back(m);
}

void Update::refresh_version_info(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Update") continue;
        m.sub_items[2].label = std::string("Version: ") + LAUNCHER_GIT_COMMIT;
        break;
    }
}

void Update::check_system_update()
{
    cp0_system_apt_update_background();
}

void Update::update_launcher()
{
    cp0_system_update_launcher_background();
}

} // namespace setting
