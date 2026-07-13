#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"

namespace setting {

std::string SoundCard::trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool SoundCard::parse_limits(const std::string &line, int &mn, int &mx)
{
    size_t p = line.find("Limits:");
    if (p == std::string::npos) return false;
    std::string rest = line.substr(p + 7);
    for (const char *pfx : {"Playback ", "Capture "}) {
        if (rest.find(pfx) == 0) { rest = rest.substr(std::strlen(pfx)); break; }
    }
    int a = 0, b = 0;
    if (std::sscanf(rest.c_str(), " %d - %d", &a, &b) == 2) {
        mn = a; mx = b; return true;
    }
    return false;
}

int SoundCard::parse_current_val(const std::string &line)
{
    size_t p = line.find(": ");
    if (p == std::string::npos) return -1;
    int v = 0;
    if (std::sscanf(line.c_str() + p + 2, " %d", &v) == 1) return v;
    return -1;
}

std::string SoundCard::extract_value_str(const std::string &line)
{
    static const char *pfx[] = {
        "Mono:", "Front Left:", "Front Right:", "Rear Left:", "Rear Right:",
        "Center:", "LFE:", "Side Left:", "Side Right:", "Capture:", "Playback:",
        nullptr
    };
    for (int i = 0; pfx[i]; ++i) {
        size_t p = line.find(pfx[i]);
        if (p != std::string::npos) return trim(line.substr(p));
    }
    return trim(line);
}

bool SoundCard::is_value_line(const std::string &tl)
{
    static const char *pfx[] = {
        "Mono:", "Front Left:", "Front Right:", "Rear Left:", "Rear Right:",
        "Center:", "LFE:", "Side Left:", "Side Right:", "Capture:", "Playback:",
        nullptr
    };
    for (int i = 0; pfx[i]; ++i)
        if (tl.rfind(pfx[i], 0) == 0) return true;
    return false;
}

// ====================================================================
//  Helpers
// ====================================================================

void SoundCard::enter_cards(UISetupPage &page)
{
    cards_.clear();
    cp0_signal_soundcard_api({"ListCards"}, [this](int code, std::string data) {
        if (code != 0) return;
        std::istringstream lines(data);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.empty()) continue;
            size_t tab = line.find('\t');
            if (tab == std::string::npos) continue;
            SoundCard::Card c;
            c.index = std::atoi(line.substr(0, tab).c_str());
            c.name  = line.substr(tab + 1);
            cards_.push_back(std::move(c));
        }
    });
    card_sel_ = 0;
    page.view_state_ = UISetupPage::ViewState::SOUNDCARD_CARDS;
    page.transition_enter_level();
}

void SoundCard::enter_controls(UISetupPage &page)
{
    if (cards_.empty()) return;
    card_idx_ = cards_[card_sel_].index;
    controls_.clear();
    cp0_signal_soundcard_api({"ListControls", std::to_string(card_idx_)},
        [this](int code, std::string data) {
            if (code != 0) return;
            std::istringstream lines(data);
            std::string line;
            while (std::getline(lines, line)) {
                if (line.empty()) continue;
                std::vector<std::string> cols;
                std::string item;
                std::istringstream row(line);
                while (std::getline(row, item, '\t')) cols.push_back(item);
                if (cols.size() < 7) continue;
                SoundCard::Control c;
                c.name        = cols[0];
                c.type        = cols[1];
                c.min_val     = std::atoi(cols[2].c_str());
                c.max_val     = std::atoi(cols[3].c_str());
                c.step        = std::atoi(cols[4].c_str());
                c.current_str = cols[5];
                c.current_val = std::atoi(cols[6].c_str());
                controls_.push_back(std::move(c));
            }
        });
    ctrl_sel_ = 0;
    page.view_state_  = UISetupPage::ViewState::SOUNDCARD_CONTROLS;
    page.transition_enter_level();
}

void SoundCard::enter_detail(UISetupPage &page)
{
    if (controls_.empty()) return;
    const auto &ctrl = controls_[ctrl_sel_];
    detail_ = SoundCard::Control{};
    detail_.name = ctrl.name;
    cp0_signal_soundcard_api({"GetControlDetail", std::to_string(card_idx_), ctrl.name},
        [this, &ctrl](int code, std::string data) {
            if (code != 0) { detail_ = ctrl; return; }
            std::istringstream ss(data);
            std::string line;
            while (std::getline(ss, line)) {
                std::string tl = trim(line);
                if (tl.rfind("Capabilities:", 0) == 0)
                    detail_.type = (tl.find("enum") != std::string::npos) ? "ENUMERATED" : "INTEGER";
                else if (tl.rfind("Limits:", 0) == 0)
                    parse_limits(tl, detail_.min_val, detail_.max_val);
                else if (detail_.current_str.empty() && is_value_line(tl)) {
                    detail_.current_str = extract_value_str(tl);
                    int v = parse_current_val(tl);
                    if (v >= 0) detail_.current_val = v;
                }
            }
        });
    if (detail_.max_val == 0 && ctrl.max_val != 0) {
        detail_.min_val = ctrl.min_val;
        detail_.max_val = ctrl.max_val;
    }
    input_buf_.clear();
    input_lbl_  = nullptr;
    hint_lbl_  = nullptr;
    page.view_state_    = UISetupPage::ViewState::SOUNDCARD_DETAIL;
    page.transition_enter_level();
}

// ====================================================================
//  Build: card list view
// ====================================================================
void SoundCard::build_cards_view(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);

    // Title
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Sound Cards");
    UISetupPage::apply_fixed_label_box(title, UISetupPage::SC_MARGIN_X, 4, UISetupPage::SC_DETAIL_TEXT_W, false);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);

    if (cards_.empty()) {
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, "No ALSA cards found.");
        UISetupPage::apply_fixed_label_box(lbl, UISetupPage::SC_MARGIN_X, 40, UISetupPage::SC_DETAIL_TEXT_W, false);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "ESC: back");
        UISetupPage::apply_fixed_label_box(hint, UISetupPage::SC_MARGIN_X, UISetupPage::LIST_H - 14, UISetupPage::SC_BOTTOM_HINT_W, false);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
        return;
    }

    int visible = 5;
    int total   = (int)cards_.size();
    int offset  = card_sel_ - visible / 2;
    if (offset < 0) offset = 0;
    if (total > visible && offset > total - visible) offset = total - visible;

    for (int vi = 0; vi < visible && (vi + offset) < total; ++vi) {
        int ai  = vi + offset;
        bool sel = (ai == card_sel_);
        int  y   = 22 + vi * 22;

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

        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, cards_[ai].name.c_str());
        UISetupPage::apply_fixed_label_box(lbl, UISetupPage::SC_ROW_X, y + 2, UISetupPage::SC_CARD_NAME_W, sel);
        lv_obj_set_style_text_color(lbl, lv_color_hex(sel ? 0xFFFFFF : 0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    }

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "OK: open  ESC: back");
    UISetupPage::apply_fixed_label_box(hint, UISetupPage::SC_MARGIN_X, UISetupPage::LIST_H - 14, UISetupPage::SC_BOTTOM_HINT_W, false);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

// ====================================================================
//  Build: control list view
// ====================================================================
void SoundCard::build_controls_view(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);

    // Title: card name
    char title_buf[80];
    if (!cards_.empty() && card_sel_ < (int)cards_.size())
        std::snprintf(title_buf, sizeof(title_buf), "%s", cards_[card_sel_].name.c_str());
    else
        std::snprintf(title_buf, sizeof(title_buf), "Card %d", card_idx_);

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, title_buf);
    UISetupPage::apply_fixed_label_box(title, UISetupPage::SC_MARGIN_X, 4, UISetupPage::SC_DETAIL_TEXT_W, true);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);

    if (controls_.empty()) {
        lv_obj_t *lbl = lv_label_create(cont);
        lv_label_set_text(lbl, "No controls found.");
        UISetupPage::apply_fixed_label_box(lbl, UISetupPage::SC_MARGIN_X, 40, UISetupPage::SC_DETAIL_TEXT_W, false);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_t *hint = lv_label_create(cont);
        lv_label_set_text(hint, "ESC: back");
        UISetupPage::apply_fixed_label_box(hint, UISetupPage::SC_MARGIN_X, UISetupPage::LIST_H - 14, UISetupPage::SC_BOTTOM_HINT_W, false);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
        return;
    }

    int visible = 5;
    int total   = (int)controls_.size();
    int offset  = ctrl_sel_ - visible / 2;
    if (offset < 0) offset = 0;
    if (total > visible && offset > total - visible) offset = total - visible;

    for (int vi = 0; vi < visible && (vi + offset) < total; ++vi) {
        int ai  = vi + offset;
        bool sel = (ai == ctrl_sel_);
        int  y   = 20 + vi * 22;

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

        const auto &ctrl = controls_[ai];

        // Left: control name
        lv_obj_t *name_lbl = lv_label_create(cont);
        lv_label_set_text(name_lbl, ctrl.name.c_str());
        UISetupPage::apply_fixed_label_box(name_lbl, UISetupPage::SC_CTRL_NAME_X, y + 2, UISetupPage::SC_CTRL_NAME_W, sel);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(sel ? 0xFFFFFF : 0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, LV_PART_MAIN);

        // Right: current value summary
        if (!ctrl.current_str.empty()) {
            lv_obj_t *val_lbl = lv_label_create(cont);
            lv_label_set_text(val_lbl, ctrl.current_str.c_str());
            UISetupPage::apply_fixed_label_box(val_lbl, UISetupPage::SC_CTRL_VALUE_X, y + 2, UISetupPage::SC_CTRL_VALUE_W, sel);
            lv_obj_set_style_text_color(val_lbl, lv_color_hex(sel ? 0xAADDFF : 0x888888), LV_PART_MAIN);
            lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
        }
    }

    // Scroll arrows
    if (ctrl_sel_ > 0) {
        lv_obj_t *a = lv_img_create(cont);
        lv_img_set_src(a, page.img_arrow_up_.c_str());
        lv_obj_set_pos(a, UISetupPage::SCREEN_W / 2 - 8, 2);
    }
    if (ctrl_sel_ < total - 1) {
        lv_obj_t *a = lv_img_create(cont);
        lv_img_set_src(a, page.img_arrow_down_.c_str());
        lv_obj_set_pos(a, UISetupPage::SCREEN_W / 2 - 8, UISetupPage::LIST_H - 28);
    }

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "OK: edit  ESC: back");
    UISetupPage::apply_fixed_label_box(hint, UISetupPage::SC_MARGIN_X, UISetupPage::LIST_H - 14, UISetupPage::SC_BOTTOM_HINT_W, false);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

// ====================================================================
//  Build: detail / input view
// ====================================================================
void SoundCard::build_detail_view(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    input_lbl_ = nullptr;
    hint_lbl_ = nullptr;

    // Control name (header)
    lv_obj_t *name_lbl = lv_label_create(cont);
    lv_label_set_text(name_lbl, detail_.name.c_str());
    UISetupPage::apply_fixed_label_box(name_lbl, UISetupPage::SC_MARGIN_X, 4, UISetupPage::SC_DETAIL_TEXT_W, true);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(name_lbl, launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);

    // Info block: Limits + current value
    char info_buf[128];
    std::snprintf(info_buf, sizeof(info_buf),
                  "Limits: %d - %d", detail_.min_val, detail_.max_val);
    lv_obj_t *limits_lbl = lv_label_create(cont);
    lv_label_set_text(limits_lbl, info_buf);
    UISetupPage::apply_fixed_label_box(limits_lbl, UISetupPage::SC_MARGIN_X, 26, UISetupPage::SC_DETAIL_TEXT_W, false);
    lv_obj_set_style_text_color(limits_lbl, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(limits_lbl, &lv_font_montserrat_12, LV_PART_MAIN);

    if (!detail_.current_str.empty()) {
        lv_obj_t *cur_lbl = lv_label_create(cont);
        lv_label_set_text(cur_lbl, detail_.current_str.c_str());
        UISetupPage::apply_fixed_label_box(cur_lbl, UISetupPage::SC_MARGIN_X, 44, UISetupPage::SC_DETAIL_TEXT_W, true);
        lv_obj_set_style_text_color(cur_lbl, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(cur_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    }

    // Separator line
    lv_obj_t *sep = lv_obj_create(cont);
    lv_obj_set_size(sep, UISetupPage::SCREEN_W - 16, 1);
    lv_obj_set_pos(sep, 8, 64);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sep, 255, LV_PART_MAIN);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    // "New value:" label
    lv_obj_t *new_lbl = lv_label_create(cont);
    lv_label_set_text(new_lbl, "New value:");
    UISetupPage::apply_fixed_label_box(new_lbl, UISetupPage::SC_MARGIN_X, 72, UISetupPage::SC_INPUT_X - UISetupPage::SC_MARGIN_X - 4, false);
    lv_obj_set_style_text_color(new_lbl, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(new_lbl, &lv_font_montserrat_12, LV_PART_MAIN);

    // Input field (digits + cursor)
    cursor_vis_ = true;
    input_lbl_ = lv_label_create(cont);
    input_update_display();
    UISetupPage::apply_fixed_label_box(input_lbl_, UISetupPage::SC_INPUT_X, 70, UISetupPage::SC_INPUT_W, false);
    lv_obj_set_style_text_color(input_lbl_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(input_lbl_, &lv_font_montserrat_14, LV_PART_MAIN);

    // Blinking cursor timer (500 ms period)
    cursor_timer_ = lv_timer_create([](lv_timer_t *timer) {
        auto *self = static_cast<SoundCard *>(lv_timer_get_user_data(timer));
        self->cursor_vis_ = !self->cursor_vis_;
        self->input_update_display();
    }, 500, this);

    // Range hint below input
    char range_buf[64];
    std::snprintf(range_buf, sizeof(range_buf), "Range: %d ~ %d",
                  detail_.min_val, detail_.max_val);
    hint_lbl_ = lv_label_create(cont);
    lv_label_set_text(hint_lbl_, range_buf);
    UISetupPage::apply_fixed_label_box(hint_lbl_, UISetupPage::SC_MARGIN_X, 92, UISetupPage::SC_DETAIL_TEXT_W, false);
    lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint_lbl_, &lv_font_montserrat_10, LV_PART_MAIN);

    // Bottom hint
    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, "0-9:type  BS:del  OK:apply  ESC:back");
    UISetupPage::apply_fixed_label_box(hint, UISetupPage::SC_MARGIN_X, UISetupPage::LIST_H - 14, UISetupPage::SC_BOTTOM_HINT_W, true);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

void SoundCard::input_update_display()
{
    if (!input_lbl_) return;
    std::string disp = input_buf_ + (cursor_vis_ ? "_" : " ");
    lv_label_set_text(input_lbl_, disp.c_str());
}

void SoundCard::cursor_stop()
{
    if (cursor_timer_) {
        lv_timer_del(cursor_timer_);
        cursor_timer_ = nullptr;
    }
    cursor_vis_ = true;
}

// Apply the typed value via cp0_signal_soundcard_api
void SoundCard::apply_value(UISetupPage &page)
{
    if (input_buf_.empty()) return;

    int new_val = std::atoi(input_buf_.c_str());
    // Clamp to declared limits when they are known
    if (detail_.max_val > detail_.min_val) {
        if (new_val < detail_.min_val) new_val = detail_.min_val;
        if (new_val > detail_.max_val) new_val = detail_.max_val;
    }

    // Visual feedback while applying
    if (hint_lbl_) {
        lv_label_set_text(hint_lbl_, "Applying...");
        lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0xFFAA00), LV_PART_MAIN);
        lv_refr_now(NULL);
    }

    int rc = -1;
    cp0_signal_soundcard_api(
        {"SetControl", std::to_string(card_idx_), detail_.name, std::to_string(new_val)},
        [&rc](int code, std::string) { rc = code; });

    if (hint_lbl_) {
        if (rc == 0) {
            lv_label_set_text(hint_lbl_, "Applied OK");
            lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0x33CC33), LV_PART_MAIN);
        } else {
            lv_label_set_text(hint_lbl_, "Error (check amixer)");
            lv_obj_set_style_text_color(hint_lbl_, lv_color_hex(0xFF4444), LV_PART_MAIN);
        }
        lv_refr_now(NULL);
    }

    // Refresh the control list entry with the new value
    if (rc == 0 && ctrl_sel_ < (int)controls_.size()) {
        char val_str[32];
        std::snprintf(val_str, sizeof(val_str), "%d", new_val);
        controls_[ctrl_sel_].current_val = new_val;
        controls_[ctrl_sel_].current_str = val_str;
    }

    // Go back to control list after a short pause
    cursor_stop();
    input_lbl_  = nullptr;
    hint_lbl_  = nullptr;
    page.view_state_ = UISetupPage::ViewState::SOUNDCARD_CONTROLS;
    lv_timer_t *t = lv_timer_create([](lv_timer_t *timer) {
        auto *self = static_cast<UISetupPage *>(lv_timer_get_user_data(timer));
        lv_timer_del(timer);
        self->transition_back_level();
    }, 900, &page);
    (void)t;
}

// ====================================================================
//  Key handlers
// ====================================================================
void SoundCard::handle_cards_key(UISetupPage &page, uint32_t key)
{
    int total = (int)cards_.size();
    switch (key) {
    case KEY_UP:
        if (card_sel_ > 0) { --card_sel_; build_cards_view(page); }
        break;
    case KEY_DOWN:
        if (card_sel_ < total - 1) { ++card_sel_; build_cards_view(page); }
        break;
    case KEY_ENTER:
    case KEY_RIGHT:
        if (total > 0) { page.play_enter(); enter_controls(page); }
        break;
    case KEY_ESC:
    case KEY_LEFT:
        page.play_back();
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.transition_back_level();
        break;
    default:
        break;
    }
}

void SoundCard::handle_controls_key(UISetupPage &page, uint32_t key)
{
    int total = (int)controls_.size();
    switch (key) {
    case KEY_UP:
        if (ctrl_sel_ > 0) { --ctrl_sel_; build_controls_view(page); }
        break;
    case KEY_DOWN:
        if (ctrl_sel_ < total - 1) { ++ctrl_sel_; build_controls_view(page); }
        break;
    case KEY_ENTER:
    case KEY_RIGHT:
        if (total > 0) { page.play_enter(); enter_detail(page); }
        break;
    case KEY_ESC:
    case KEY_LEFT:
        page.play_back();
        page.view_state_ = UISetupPage::ViewState::SOUNDCARD_CARDS;
        page.transition_back_level();
        break;
    default:
        break;
    }
}

void SoundCard::handle_detail_key(UISetupPage &page, uint32_t key)
{
    // Digit keys: accumulate input
    if (key == KEY_0 || (key >= KEY_1 && key <= KEY_9)) {
        // KEY_1..KEY_9 map to '1'..'9', KEY_0 maps to '0'
        // Linux input key codes: KEY_1=2..KEY_9=10, KEY_0=11
        int digit = -1;
        if (key == KEY_0)         digit = 0;
        else if (key >= KEY_1 && key <= KEY_9) digit = (int)(key - KEY_1 + 1);
        if (digit >= 0 && input_buf_.size() < 8) {
            input_buf_ += (char)('0' + digit);
            input_update_display();
        }
        return;
    }

    switch (key) {
    case KEY_BACKSPACE:
        if (!input_buf_.empty()) {
            input_buf_.pop_back();
            input_update_display();
        }
        break;
    case KEY_ENTER:
    case KEY_RIGHT:
        apply_value(page);
        break;
    case KEY_ESC:
    case KEY_LEFT:
        cursor_stop();
        page.play_back();
        page.view_state_ = UISetupPage::ViewState::SOUNDCARD_CONTROLS;
        page.transition_back_level();
        break;
    default:
        // Also accept typed digit characters forwarded via page.cur_elm_->utf8
        if (page.cur_elm_ && page.cur_elm_->utf8[0] >= '0' && page.cur_elm_->utf8[0] <= '9') {
            if (input_buf_.size() < 8) {
                input_buf_ += page.cur_elm_->utf8[0];
                input_update_display();
            }
        }
        break;
    }
}


void SoundCard::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    MenuItem m;
    m.label = "SoundCard";
    m.sub_items = {{"Open Mixer", false, false, [page]() { page->soundcard_.enter_cards(*page); }}};
    menu.push_back(m);
}

void build_menu(UISetupPage &page)
{
    page.menu_items_.clear();
    Launcher::append(page, page.menu_items_);
    Boot::append(page, page.menu_items_);
    Screen::append(page, page.menu_items_);
    WiFi::append(page, page.menu_items_);
    Speaker::append(page, page.menu_items_);
    Camera::append(page, page.menu_items_);
    Info::append(page, page.menu_items_);
    About::append(page, page.menu_items_);
    Help::append(page, page.menu_items_);
    ExtPort::append(page, page.menu_items_);
    Developer::append(page, page.menu_items_);
    RTC::append(page, page.menu_items_);
    Bluetooth::append(page, page.menu_items_);
    Ethernet::append(page, page.menu_items_);
    Account::append(page, page.menu_items_);
    Update::append(page, page.menu_items_);
    SoundCard::append(page, page.menu_items_);
}

} // namespace setting
