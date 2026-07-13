#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"

namespace setting {

void Info::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "Info";
    m.sub_items = {
        {"Battery: --%", false, false, nullptr},
        {"Temp: --C", false, false, nullptr},
        {"Current: --mA", false, false, nullptr},
        {"Voltage: --V", false, false, nullptr},
        {"BQ Calibrate", false, false, [page]() { page->info_.enter_bq_calibrate(*page); }},
    };
    m.on_enter = [page]() { page->info_.refresh_values(*page); page->info_.start_timer(*page); };
    menu.push_back(m);
}

void Info::refresh_values(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Info") continue;
        cp0_battery_info_t bat{};
        cp0_signal_bq27220_api({"Read"}, [&](int code, std::string data) {
            cp0_battery_info_t parsed{};
            if (code == 0 && std::sscanf(data.c_str(), "%d,%d,%d,%d,%d,%d,%d,%d,%d",
                    &parsed.voltage_mv, &parsed.current_ma, &parsed.temperature_c10, &parsed.soc,
                    &parsed.remain_mah, &parsed.full_mah, &parsed.flags, &parsed.avg_current_ma,
                    &parsed.valid) == 9)
                bat = parsed;
        });
        char buf[64];
        snprintf(buf, sizeof(buf), "Battery: %d%%", bat.valid ? bat.soc : 0);
        m.sub_items[0].label = buf;
        snprintf(buf, sizeof(buf), "Temp: %.1fC", bat.valid ? bat.temperature_c10 / 10.0 : 0.0);
        m.sub_items[1].label = buf;
        if (bat.valid && bat.current_ma != INT32_MIN)
            snprintf(buf, sizeof(buf), "Current: %dmA", bat.current_ma);
        else
            snprintf(buf, sizeof(buf), "Current: --mA");
        m.sub_items[2].label = buf;
        snprintf(buf, sizeof(buf), "Voltage: %.2fV", bat.valid ? bat.voltage_mv / 1000.0 : 0.0);
        m.sub_items[3].label = buf;
        break;
    }
    if (page.view_state_ == UISetupPage::ViewState::SUB) refresh_visible_labels(page);
}

void Info::reset_visible_labels()
{
    for (int i = 0; i < 4; ++i) {
        sub_labels_[i] = nullptr;
        sub_label_focused_[i] = false;
        visible_text_[i].clear();
    }
}

void Info::track_visible_label(int index, lv_obj_t *label, bool focused, const std::string &text)
{
    if (index < 0 || index >= 4)
        return;

    sub_labels_[index] = label;
    sub_label_focused_[index] = focused;
    visible_text_[index] = text;
}

void Info::refresh_visible_labels(UISetupPage &page)
{
    if (page.selected_idx_ < 0 || page.selected_idx_ >= (int)page.menu_items_.size())
        return;

    MenuItem &item = page.menu_items_[page.selected_idx_];
    if (item.label != "Info")
        return;

    for (int i = 0; i < 4 && i < (int)item.sub_items.size(); ++i) {
        lv_obj_t *lbl = sub_labels_[i];
        if (!lbl)
            continue;

        const char *new_text = item.sub_items[i].label.c_str();
        if (visible_text_[i] == new_text)
            continue;

        lv_label_set_text(lbl, new_text);
        visible_text_[i] = new_text;
        page.apply_overflow_centered(lbl, UISetupPage::SUB_CENTER_X,
                                     sub_label_focused_[i] ? 80 : UISetupPage::VALUE_BOX_W,
                                     sub_label_focused_[i]);
    }
}

void Info::start_timer(UISetupPage &page)
{
    stop_timer();
    timer_ = lv_timer_create([](lv_timer_t *t) {
        UISetupPage *self = (UISetupPage *)lv_timer_get_user_data(t);
        if (self && self->view_state_ == UISetupPage::ViewState::SUB)
            self->info_.refresh_values(*self);
    }, 1000, &page);
}

void Info::stop_timer()
{
    if (timer_) { lv_timer_delete(timer_); timer_ = nullptr; }
}

void Info::enter_bq_calibrate(UISetupPage &page)
{
    page.val_title_ = "BQ Calib";
    page.val_options_ = {"Enter CAL", "CC Offset", "Board Offset", "Exit CAL"};
    page.val_sel_idx_ = 0;
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void Info::apply_bq_calibrate(UISetupPage &page)
{
    cp0_signal_bq27220_api({"Calibrate", std::to_string(page.val_sel_idx_)}, nullptr);
}

void RTC::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "RTC";
    m.sub_items = {
        {"NTP", true, true, [page]() { page->rtc_.toggle_ntp(*page); }},
        {"Year", false, false, [page]() { page->rtc_.enter_adjust(*page, 0); }},
        {"Month", false, false, [page]() { page->rtc_.enter_adjust(*page, 1); }},
        {"Day", false, false, [page]() { page->rtc_.enter_adjust(*page, 2); }},
        {"Hour", false, false, [page]() { page->rtc_.enter_adjust(*page, 3); }},
        {"Minute", false, false, [page]() { page->rtc_.enter_adjust(*page, 4); }},
        {"Second", false, false, [page]() { page->rtc_.enter_adjust(*page, 5); }},
    };
    m.on_enter = [page]() { page->rtc_.refresh_values(*page); };
    menu.push_back(m);
}

RTC::RTC() : async_state_(std::make_shared<AsyncState>()) {}

RTC::~RTC()
{
    async_state_->alive = false;
    if (async_state_->request_id)
        cp0_signal_sudo_cancel(async_state_->request_id, nullptr);
}

int RTC::days_in_month(int year, int month)
{
    return cp0_testable::days_in_month(year, month);
}

void RTC::update_labels(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "RTC") continue;
        m.sub_items[0].toggle_state = ntp_on_;
        char buf[32];
        const char *names[] = {"Year", "Month", "Day", "Hour", "Minute", "Second"};
        for (int i = 0; i < 6; ++i) {
            snprintf(buf, sizeof(buf), "%s: %d", names[i], values_[i]);
            m.sub_items[i + 1].label = buf;
        }
        break;
    }
}

void RTC::refresh_values(UISetupPage &page)
{
    int ntp = -1;
    cp0_signal_osinfo_api({"NtpGet"}, [&](int code, std::string) { ntp = code; });
    ntp_available_ = ntp >= 0;
    if (ntp_available_)
        ntp_on_ = ntp == 1;
    cp0_signal_osinfo_api({"LocalTime"}, [&](int code, std::string data) {
        int parsed[6]{};
        if (code == 0 && std::sscanf(data.c_str(), "%d,%d,%d,%d,%d,%d", &parsed[0], &parsed[1],
                &parsed[2], &parsed[3], &parsed[4], &parsed[5]) == 6)
            std::copy(std::begin(parsed), std::end(parsed), std::begin(values_));
    });
    dirty_ = false;
    update_labels(page);
}

void RTC::toggle_ntp(UISetupPage &page)
{
    NtpToggleEligibility eligibility = ntp_toggle_eligibility(
        async_state_->request_id != 0, dirty_, ntp_available_);
    if (eligibility == NtpToggleEligibility::IN_FLIGHT)
        return;
    if (eligibility == NtpToggleEligibility::DIRTY) {
        update_labels(page);
        show_status("NTP unchanged", "Save or discard time edits first", Modal::ERROR);
        return;
    }
    if (eligibility == NtpToggleEligibility::UNAVAILABLE) {
        update_labels(page);
        show_status("NTP unavailable", "Unable to read NTP status", Modal::ERROR);
        return;
    }
    bool desired = ntp_on_;
    for (auto &m : page.menu_items_)
        if (m.label == "RTC") { desired = m.sub_items[0].toggle_state; break; }
    ntp_previous_ = ntp_on_;
    show_status("Updating NTP", "Please wait...", Modal::BUSY);
    struct Context { RTC *rtc; UISetupPage *page; std::weak_ptr<AsyncState> state; };
    auto *ctx = new (std::nothrow) Context{this, &page, async_state_};
    if (!ctx) {
        ntp_on_ = ntp_rollback_value(ntp_previous_);
        update_labels(page);
        show_status("NTP failed", "Out of memory", Modal::ERROR);
        return;
    }
    uint64_t request_id = 0;
    int rc = -1;
    cp0_signal_system_admin_async({"NtpSet", desired ? "1" : "0"}, 60000, 30000,
        [ctx](int result_code, int exit_code) {
            std::unique_ptr<Context> owned(ctx);
            cp0_sudo_result_t result = static_cast<cp0_sudo_result_t>(result_code);
            auto state = ctx->state.lock();
            if (!state || !state->alive) return;
            ctx->rtc->finish_request();
            if (result != CP0_SUDO_RESULT_SUCCESS) {
                ctx->rtc->ntp_on_ = ntp_rollback_value(ctx->rtc->ntp_previous_);
                ctx->rtc->update_labels(*ctx->page);
                if (result == CP0_SUDO_RESULT_CANCELLED) {
                    ctx->rtc->close_write_confirm();
                    ctx->page->build_sub_view();
                    return;
                }
                ctx->rtc->show_result_error(result, exit_code, "NTP update");
                return;
            }
            int actual = -1;
            cp0_signal_osinfo_api({"NtpGet"}, [&](int code, std::string) { actual = code; });
            if (actual < 0) {
                ctx->rtc->ntp_available_ = false;
                ctx->rtc->show_status("NTP unavailable", "Unable to read NTP status", Modal::ERROR);
            } else {
                ctx->rtc->ntp_available_ = true;
                ctx->rtc->ntp_on_ = actual == 1;
                ctx->rtc->close_write_confirm();
            }
            ctx->rtc->update_labels(*ctx->page);
            ctx->page->build_sub_view();
        }, [&](int code, uint64_t id) { rc = code; request_id = id; });
    if (rc != 0) {
        delete ctx;
        ntp_on_ = ntp_rollback_value(ntp_previous_);
        update_labels(page);
        show_status("NTP failed", "Unable to start request", Modal::ERROR);
    }
    else async_state_->request_id = request_id;
}

void RTC::enter_adjust(UISetupPage &page, int field)
{
    if (ntp_on_)
        return;
    field_ = field;
    const char *names[] = {"Year", "Month", "Day", "Hour", "Minute", "Second"};
    page.val_title_ = names[field];
    int cur = values_[field];
    int mins[] = {2000, 1, 1, 0, 0, 0};
    int maxs[] = {2099, 12, days_in_month(values_[0], values_[1]), 23, 59, 59};

    page.val_options_.clear();
    for (int v = mins[field]; v <= maxs[field]; ++v) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", v);
        page.val_options_.push_back(buf);
    }
    page.val_sel_idx_ = cur - mins[field];
    page.view_state_ = UISetupPage::ViewState::VALUE_SELECT;
    page.transition_enter_level();
}

void RTC::apply_value(UISetupPage &page)
{
    int new_val = atoi(page.val_options_[page.val_sel_idx_].c_str());
    values_[field_] = new_val;
    if ((field_ == 0 || field_ == 1) && values_[2] > days_in_month(values_[0], values_[1]))
        values_[2] = days_in_month(values_[0], values_[1]);
    dirty_ = true;
    update_labels(page);
}

void RTC::commit_to_hardware(UISetupPage &page)
{
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             values_[0], values_[1], values_[2], values_[3], values_[4], values_[5]);
    show_status("Saving date & time", "Please wait...", Modal::BUSY);
    struct Context { RTC *rtc; UISetupPage *page; std::weak_ptr<AsyncState> state; };
    auto *ctx = new (std::nothrow) Context{this, &page, async_state_};
    if (!ctx) { show_status("Save failed", "Out of memory", Modal::ERROR); return; }
    uint64_t request_id = 0;
    int rc = -1;
    cp0_signal_system_admin_async({"TimeSet", timestamp}, 60000, 30000,
        [ctx](int result_code, int exit_code) {
            std::unique_ptr<Context> owned(ctx);
            cp0_sudo_result_t result = static_cast<cp0_sudo_result_t>(result_code);
            auto state = ctx->state.lock();
            if (!state || !state->alive) return;
            ctx->rtc->finish_request();
            if (result == CP0_SUDO_RESULT_SUCCESS) {
                ctx->rtc->dirty_ = false;
                ctx->rtc->close_write_confirm();
                ctx->page->update_datetime_status();
                ctx->page->view_state_ = UISetupPage::ViewState::MAIN;
                ctx->page->build_main_view();
            } else {
                ctx->rtc->dirty_ = true;
                if (result == CP0_SUDO_RESULT_CANCELLED)
                    ctx->rtc->close_write_confirm();
                else
                    ctx->rtc->show_result_error(result, exit_code, "Date/time save");
            }
        }, [&](int code, uint64_t id) { rc = code; request_id = id; });
    if (rc != 0) { delete ctx; show_status("Save failed", "Unable to start request", Modal::ERROR); }
    else async_state_->request_id = request_id;
}

void RTC::finish_request()
{
    async_state_->request_id = 0;
}

void RTC::show_result_error(cp0_sudo_result_t result, int exit_code, const char *operation)
{
    const char *reason = "Command failed";
    switch (classify_privileged_result(static_cast<int>(result))) {
    case PrivilegedResultKind::AUTH_FAILED: reason = "Authentication failed"; break;
    case PrivilegedResultKind::CANCELLED: reason = "Request cancelled"; break;
    case PrivilegedResultKind::TIMED_OUT: reason = "Request timed out"; break;
    case PrivilegedResultKind::EXEC_FAILED: reason = exit_code ? "Command returned an error" : "Unable to start command"; break;
    default: break;
    }
    char title[64];
    snprintf(title, sizeof(title), "%s failed", operation);
    show_status(title, reason, Modal::ERROR);
}

void RTC::show_status(const char *title_text, const char *detail_text, Modal modal)
{
    close_write_confirm();
    modal_ = modal;
    confirm_overlay_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(confirm_overlay_);
    lv_obj_set_size(confirm_overlay_, UISetupPage::SCREEN_W, UISetupPage::SCREEN_H + 20);
    lv_obj_set_pos(confirm_overlay_, 0, 0);
    lv_obj_set_style_bg_color(confirm_overlay_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(confirm_overlay_, LV_OPA_60, 0);
    lv_obj_clear_flag(confirm_overlay_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *box = lv_obj_create(confirm_overlay_);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 250, 82);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(modal == Modal::ERROR ? 0xCC5555 : 0x3A5A8A), 0);
    lv_obj_set_style_border_width(box, 1, 0);

    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, title_text);
    lv_obj_set_width(title, 230);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *detail = lv_label_create(box);
    lv_label_set_text(detail, detail_text);
    lv_obj_set_width(detail, 230);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(detail, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_10, 0);
    lv_obj_align(detail, LV_ALIGN_CENTER, 0, 7);
    if (modal == Modal::ERROR) {
        lv_obj_t *hint = lv_label_create(box);
        lv_label_set_text(hint, "OK / ESC: close");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x777777), 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);
    }
    lv_obj_move_foreground(confirm_overlay_);
}

void RTC::show_write_confirm(UISetupPage &page)
{
    if (confirm_overlay_)
        return;

    modal_ = Modal::CONFIRM;
    confirm_sel_ = 1;
    lv_obj_t *layer = lv_layer_top();

    confirm_overlay_ = lv_obj_create(layer);
    lv_obj_remove_style_all(confirm_overlay_);
    lv_obj_set_size(confirm_overlay_, UISetupPage::SCREEN_W, UISetupPage::SCREEN_H + 20);
    lv_obj_set_pos(confirm_overlay_, 0, 0);
    lv_obj_set_style_bg_color(confirm_overlay_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(confirm_overlay_, LV_OPA_60, 0);
    lv_obj_clear_flag(confirm_overlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(confirm_overlay_, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *box = lv_obj_create(confirm_overlay_);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 230, 86);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x3A5A8A), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, "Write hardware RTC?");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *hint = lv_label_create(box);
    lv_label_set_text(hint, "OK:confirm  ESC:no");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -7);

    confirm_yes_lbl_ = lv_label_create(box);
    lv_label_set_text(confirm_yes_lbl_, "Yes");
    lv_obj_set_style_text_font(confirm_yes_lbl_, launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), 0);
    lv_obj_align(confirm_yes_lbl_, LV_ALIGN_CENTER, -42, 8);

    confirm_no_lbl_ = lv_label_create(box);
    lv_label_set_text(confirm_no_lbl_, "No");
    lv_obj_set_style_text_font(confirm_no_lbl_, launcher_fonts().get("Montserrat-Bold.ttf", 16, LV_FREETYPE_FONT_STYLE_BOLD), 0);
    lv_obj_align(confirm_no_lbl_, LV_ALIGN_CENTER, 42, 8);

    update_write_confirm_buttons();
    lv_obj_move_foreground(confirm_overlay_);
    lv_refr_now(NULL);
}

void RTC::close_write_confirm()
{
    if (confirm_overlay_) {
        lv_obj_del(confirm_overlay_);
        confirm_overlay_ = nullptr;
    }
    confirm_yes_lbl_ = nullptr;
    confirm_no_lbl_ = nullptr;
    modal_ = Modal::NONE;
}

void RTC::update_write_confirm_buttons()
{
    if (!confirm_yes_lbl_ || !confirm_no_lbl_)
        return;
    lv_obj_set_style_text_color(confirm_yes_lbl_,
                                lv_color_hex(confirm_sel_ == 0 ? 0x00CC66 : 0x888888), 0);
    lv_obj_set_style_text_color(confirm_no_lbl_,
                                lv_color_hex(confirm_sel_ == 1 ? 0x00CC66 : 0x888888), 0);
}

void RTC::handle_write_confirm_key(UISetupPage &page, uint32_t key)
{
    if (modal_ == Modal::BUSY)
        return;
    if (modal_ == Modal::ERROR) {
        if (key == KEY_ENTER || key == KEY_ESC) {
            page.play_back();
            close_write_confirm();
            if (page.view_state_ == UISetupPage::ViewState::SUB)
                page.build_sub_view();
        }
        return;
    }
    switch (key) {
    case KEY_LEFT:
    case KEY_UP:
        confirm_sel_ = 0;
        update_write_confirm_buttons();
        break;
    case KEY_RIGHT:
    case KEY_DOWN:
        confirm_sel_ = 1;
        update_write_confirm_buttons();
        break;
    case KEY_ENTER:
        page.play_enter();
        close_write_confirm();
        if (confirm_sel_ == 0) {
            commit_to_hardware(page);
        } else {
            refresh_values(page);
            page.view_state_ = UISetupPage::ViewState::MAIN;
            page.build_main_view();
        }
        break;
    case KEY_ESC:
        page.play_back();
        close_write_confirm();
        refresh_values(page);
        dirty_ = false;
        page.view_state_ = UISetupPage::ViewState::MAIN;
        page.build_main_view();
        break;
    default:
        break;
    }
}


} // namespace setting
