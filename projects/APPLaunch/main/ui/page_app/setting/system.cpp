#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"

namespace setting {

Developer::Developer() : async_state_(std::make_shared<AsyncState>()) {}

Developer::~Developer()
{
    async_state_->alive = false;
    if (async_state_->request_id) cp0_signal_sudo_cancel(async_state_->request_id, nullptr);
    close_status();
}

void Developer::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Developer";
    bool adb_en = UISetupPage::config_get_int("adb_debug", 0) != 0;
    m.sub_items = {
        {"ADB", true, adb_en, [page]() { page->developer_.toggle_adb(*page); }},
        {"Pair host key", false, false, [page]() { page->developer_.enter_pair_view(*page); }},
        {"Authorizations: 0", false, false, nullptr},
        {"Manage authorized hosts", false, false, [page]() { page->developer_.enter_revoke_view(*page); }},
        {"Clear authorizations", false, false, [page]() {
            page->enter_confirm_action("Clear ADB authorizations?", [page]() {
                page->developer_.clear_authorizations(*page);
            });
        }},
    };
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
    if (want_on) {
        refresh_adb_status(page);
        if (page.menu_items_[idx].sub_items[2].label == "Authorizations: 0") {
            page.menu_items_[idx].sub_items[0].toggle_state = false;
            enter_pair_view(page, true);
            return;
        }
    }
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
    uint64_t request_id = 0;
    int rc = -1;
    cp0_signal_system_admin_async({"AdbSet", want_on ? "1" : "0"}, 60000, 300000,
        [ctx](int result_code, int exit_code) {
            std::unique_ptr<Context> owned(ctx);
            cp0_sudo_result_t result = static_cast<cp0_sudo_result_t>(result_code);
            auto state = ctx->state.lock();
            if (!state || !state->alive) return;
            state->request_id = 0;
            if (adb_toggle_succeeded(result, exit_code)) {
                ctx->developer->update_toggle(*ctx->page, ctx->desired, true);
                ctx->developer->close_status();
                if (adb_reboot_required(result, exit_code))
                    ctx->developer->enter_usb_guide(*ctx->page, ctx->desired);
                else
                    ctx->page->build_sub_view();
                return;
            }
            if (!ctx->developer->refresh_adb_status(*ctx->page))
                ctx->developer->update_toggle(*ctx->page, ctx->previous, false);
            if (result == CP0_SUDO_RESULT_CANCELLED) {
                ctx->developer->close_status();
                ctx->page->build_sub_view();
            } else {
                ctx->developer->show_result_error(result, exit_code);
            }
        }, [&](int code, uint64_t id) { rc = code; request_id = id; });
    if (rc != 0) {
        delete ctx;
        update_toggle(page, previous, false);
        show_status("ADB update failed", "Unable to start request", Modal::ERROR);
        return;
    }
    async_state_->request_id = request_id;
}

bool Developer::refresh_adb_status(UISetupPage &page)
{
    int idx = page.find_menu("Developer");
    if (idx < 0) return false;
    int rc = -1;
    std::string out;
    cp0_signal_process_api({"AdbStatus"},
                           [&](int code, std::string data) { rc = code; out = std::move(data); });
    if (rc != 0) return false;
    AdbStatus status = parse_adb_status(out.c_str());
    if (!status.valid) return false;
    update_toggle(page, adb_state_after_failure(status, false), true);
    char count[32];
    snprintf(count, sizeof(count), "Authorizations: %d", status.authorizations);
    page.menu_items_[idx].sub_items[2].label = count;
    authorizations_ = parse_adb_authorizations(out.c_str());
    if (authorization_selected_ >= static_cast<int>(authorizations_.size()))
        authorization_selected_ = std::max(0, static_cast<int>(authorizations_.size()) - 1);
    return true;
}

void Developer::enter_pair_view(UISetupPage &page, bool enable_after_pair)
{
    enable_after_pair_ = enable_after_pair;
    pair_key_.clear();
    page.view_state_ = UISetupPage::ViewState::ADB_PAIR;
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);

    guide_label(cont, 8, 5, "Pair ADB host", 0xFFFFFF,
                launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD));
    guide_label(cont, 8, 28, "Paste the host's adbkey.pub:", 0xAAAAAA, &lv_font_montserrat_10);
    pair_input_label_ = guide_label(cont, 8, 48, "_", 0xFFFFFF, &lv_font_montserrat_10);
    lv_obj_set_width(pair_input_label_, 304);
    lv_label_set_long_mode(pair_input_label_, LV_LABEL_LONG_CLIP);
    pair_hint_label_ = guide_label(cont, 8, 82, "OK: authorize   ESC: cancel", 0x777777,
                                   &lv_font_montserrat_10);
}

void Developer::enter_revoke_view(UISetupPage &page)
{
    refresh_adb_status(page);
    authorization_selected_ = 0;
    page.view_state_ = UISetupPage::ViewState::ADB_AUTHORIZATIONS;
    build_authorizations_view(page);
}

void Developer::build_authorizations_view(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    guide_label(cont, 8, 3, "Authorized ADB hosts", 0xFFFFFF,
                launcher_fonts().get("Montserrat-Bold.ttf", 13, LV_FREETYPE_FONT_STYLE_BOLD));
    if (authorizations_.empty()) {
        guide_label(cont, 8, 48, "No authorized hosts", 0x888888, &lv_font_montserrat_12);
    } else {
        const int first = std::max(0, authorization_selected_ - 1);
        const int last = std::min(static_cast<int>(authorizations_.size()), first + 3);
        for (int i = first; i < last; ++i) {
            const int y = 27 + (i - first) * 28;
            if (i == authorization_selected_)
                guide_chip(cont, 4, y - 2, 312, 25, 0x2A2A2A, 0x444444, 2, 0);
            std::string label = authorizations_[i].label;
            if (label.size() > 25) label.resize(25);
            guide_label(cont, 10, y, label.c_str(), i == authorization_selected_ ? 0xFFFFFF : 0xAAAAAA,
                        &lv_font_montserrat_10);
            std::string short_id = authorizations_[i].fingerprint.substr(0, 12);
            guide_label(cont, 218, y, short_id.c_str(), 0x666666, &lv_font_montserrat_10);
        }
    }
    guide_label(cont, 8, UISetupPage::LIST_H - 15,
                authorizations_.empty() ? "ESC: back" : "OK: revoke   ESC: back",
                0x777777, &lv_font_montserrat_10);
}

void Developer::handle_authorizations_key(UISetupPage &page, uint32_t key)
{
    if (key == KEY_ESC || key == KEY_LEFT) {
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.build_sub_view();
        return;
    }
    if (key == KEY_UP && authorization_selected_ > 0) {
        --authorization_selected_;
        build_authorizations_view(page);
    } else if (key == KEY_DOWN && authorization_selected_ + 1 < static_cast<int>(authorizations_.size())) {
        ++authorization_selected_;
        build_authorizations_view(page);
    } else if ((key == KEY_ENTER || key == KEY_RIGHT) && !authorizations_.empty()) {
        const std::string fingerprint = authorizations_[authorization_selected_].fingerprint;
        UISetupPage *page_ptr = &page;
        page.enter_confirm_action("Revoke selected ADB host?", [this, page_ptr, fingerprint]() {
            run_admin_action(*page_ptr, {"AdbRevoke", fingerprint}, "Revoking ADB host",
                             "Removing this key...", false);
        });
    }
}

void Developer::handle_pair_key(UISetupPage &page, uint32_t key)
{
    if (key == KEY_ESC || key == KEY_LEFT) {
        pair_key_.clear();
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.build_sub_view();
        return;
    }
    if (key == KEY_ENTER) {
        submit_pairing(page);
        return;
    }
    if (key == KEY_BACKSPACE) {
        if (!pair_key_.empty()) pair_key_.pop_back();
    } else if (page.cur_elm_ && page.cur_elm_->utf8[0] && pair_key_.size() < 2048) {
        pair_key_ += page.cur_elm_->utf8;
    }
    if (pair_input_label_) {
        std::string shown = pair_key_.size() > 40 ? "..." + pair_key_.substr(pair_key_.size() - 40) : pair_key_;
        shown += "_";
        lv_label_set_text(pair_input_label_, shown.c_str());
    }
}

void Developer::submit_pairing(UISetupPage &page)
{
    if (!adb_public_key_valid(pair_key_)) {
        if (pair_hint_label_) {
            lv_label_set_text(pair_hint_label_, "Invalid adbkey.pub public key");
            lv_obj_set_style_text_color(pair_hint_label_, lv_color_hex(0xEB5F5F), 0);
        }
        return;
    }
    run_admin_action(page, {"AdbAuthorize", pair_key_}, "Pairing ADB host",
                     "Authorizing this key...", enable_after_pair_);
}

void Developer::clear_authorizations(UISetupPage &page)
{
    run_admin_action(page, {"AdbClearAuthorizations"}, "Clearing ADB keys",
                     "Revoking every host...", false);
}

void Developer::run_admin_action(UISetupPage &page, std::list<std::string> args,
                                 const char *title, const char *detail, bool enable_after)
{
    show_status(title, detail, Modal::BUSY);
    struct Context { Developer *self; UISetupPage *page; std::weak_ptr<AsyncState> state; bool enable; };
    auto *ctx = new (std::nothrow) Context{this, &page, async_state_, enable_after};
    if (!ctx) { show_status("ADB authorization failed", "Out of memory", Modal::ERROR); return; }
    uint64_t request_id = 0;
    int rc = -1;
    cp0_signal_system_admin_async(std::move(args), 60000, 30000,
        [ctx](int result_code, int exit_code) {
            std::unique_ptr<Context> owned(ctx);
            auto state = ctx->state.lock();
            if (!state || !state->alive) return;
            state->request_id = 0;
            auto result = static_cast<cp0_sudo_result_t>(result_code);
            if (result != CP0_SUDO_RESULT_SUCCESS) {
                ctx->self->show_result_error(result, exit_code);
                return;
            }
            ctx->self->pair_key_.clear();
            ctx->self->close_status();
            ctx->page->view_state_ = UISetupPage::ViewState::SUB;
            ctx->self->refresh_adb_status(*ctx->page);
            ctx->page->build_sub_view();
            if (ctx->enable) {
                int idx = ctx->page->find_menu("Developer");
                if (idx >= 0) ctx->page->menu_items_[idx].sub_items[0].toggle_state = true;
                ctx->self->toggle_adb(*ctx->page);
            }
        }, [&](int code, uint64_t id) { rc = code; request_id = id; });
    if (rc != 0) {
        delete ctx;
        show_status("ADB authorization failed", "Unable to start request", Modal::ERROR);
        return;
    }
    async_state_->request_id = request_id;
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
        reason = exit_code < 0 ? "Unable to start command" : "Command returned an error";
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
        cp0_signal_process_api({"Reboot"}, nullptr);
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
        std::string data;
        cp0_signal_osinfo_api({"NetworkDefaultInfoRead"},
                              [&](int code, std::string value) { if (code == 0) data = std::move(value); });
        std::istringstream lines(data);
        std::string ip, gateway, mac;
        std::getline(lines, ip); std::getline(lines, gateway); std::getline(lines, mac);
        m.sub_items[0].label = "IP: " + ip;
        m.sub_items[1].label = "GW: " + gateway;
        m.sub_items[2].label = "MAC: " + mac;
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
        std::string data;
        cp0_signal_osinfo_api({"AccountInfoRead"},
                              [&](int code, std::string value) { if (code == 0) data = std::move(value); });
        std::istringstream lines(data);
        std::string user, hostname;
        std::getline(lines, user); std::getline(lines, hostname);
        m.sub_items[0].label = "User: " + user;
        m.sub_items[1].label = "Password: ****";
        m.sub_items[2].label = "Host: " + hostname;
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
    cp0_signal_osinfo_api({"AptUpdateBackground"}, nullptr);
}

void Update::update_launcher()
{
    cp0_signal_osinfo_api({"UpdateLauncherBackground"}, nullptr);
}

} // namespace setting
