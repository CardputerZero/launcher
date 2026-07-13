#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"

namespace setting {

void Bluetooth::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    Bluetooth *bt = &page->bluetooth_;
    MenuItem m;
    m.label = "Bluetooth";
    bt->named_only_ = UISetupPage::config_get_int("bt_named_only", 1) != 0;
    m.sub_items = {
        {"Power", true, false, [bt, page]() { bt->toggle_power(*page); }},
        {"Alias: CardputerZero", false, false, [bt, page]() { bt->enter_alias(*page); }},
        {"Discoverable", true, false, [bt, page]() { bt->toggle_discoverable(*page); }},
        {"Named Only", true, bt->named_only_, [bt, page]() { bt->toggle_named_only(*page); }},
        {"Connected", false, false, [bt, page]() { bt->enter_devices(*page); }},
        {"Scan", false, false, [bt, page]() { bt->enter_scan(*page); }},
    };
    m.on_enter = [bt, page]() { bt->refresh_status(*page); };
    menu.push_back(m);
}



void Bluetooth::enter_devices(UISetupPage &page)
{
    stop_scan_timer();
    list_mode_ = ListMode::Managed;
    page.view_state_ = UISetupPage::ViewState::BT_LIST;
    list_sel_ = 0;
    refresh_devices();
    build_list(page);
}

void Bluetooth::enter_alias(UISetupPage &page)
{
    stop_scan_timer();
    refresh_status(page);
    alias_input_ = alias_.empty() ? "CardputerZero" : alias_;
    page.view_state_ = UISetupPage::ViewState::BT_ALIAS;
    build_alias_view(page);
}

bool Bluetooth::alias_char_allowed(unsigned char ch)
{
    return std::isprint(ch) && ch != '\t' && ch != '\n' && ch != '\r';
}

std::string Bluetooth::alias_sanitized() const
{
    std::string out;
    out.reserve(alias_input_.size());
    for (unsigned char ch : alias_input_) {
        if (alias_char_allowed(ch))
            out.push_back(static_cast<char>(ch));
    }
    if (out.empty())
        out = "CardputerZero";
    return out.substr(0, CP0_BT_NAME_MAX - 1);
}

void Bluetooth::build_alias_view(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    alias_input_lbl_ = nullptr;
    alias_hint_lbl_ = nullptr;

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Bluetooth Name");
    lv_obj_set_pos(title, 10, 10);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, "Name:");
    lv_obj_set_pos(label, 10, 38);
    lv_obj_set_style_text_color(label, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);

    alias_input_lbl_ = lv_label_create(cont);
    lv_obj_set_pos(alias_input_lbl_, 64, 36);
    lv_obj_set_width(alias_input_lbl_, 236);
    lv_label_set_long_mode(alias_input_lbl_, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(alias_input_lbl_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(alias_input_lbl_, &lv_font_montserrat_14, LV_PART_MAIN);
    alias_update_display();

    alias_hint_lbl_ = lv_label_create(cont);
    lv_label_set_text(alias_hint_lbl_, "OK:set  BS:del  ESC:cancel");
    lv_obj_set_pos(alias_hint_lbl_, 10, 70);
    lv_obj_set_style_text_color(alias_hint_lbl_, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(alias_hint_lbl_, &lv_font_montserrat_10, LV_PART_MAIN);
}

void Bluetooth::alias_update_display()
{
    if (!alias_input_lbl_)
        return;
    std::string display = alias_input_ + "_";
    lv_label_set_text(alias_input_lbl_, display.c_str());
}

void Bluetooth::handle_alias_key(UISetupPage &page, uint32_t key)
{
    if (key == KEY_ESC || key == KEY_LEFT) {
        page.play_back();
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.build_sub_view();
        return;
    }
    if (key == KEY_ENTER || key == KEY_RIGHT) {
        std::string alias = alias_sanitized();
        if (alias_hint_lbl_) {
            lv_label_set_text(alias_hint_lbl_, "Setting alias...");
            lv_obj_set_style_text_color(alias_hint_lbl_, lv_color_hex(0xFFAA00), LV_PART_MAIN);
            lv_refr_now(NULL);
        }
        int ret = set_alias(alias);
        if (ret == 0) {
            alias_ = alias;
            refresh_status(page);
            page.view_state_ = UISetupPage::ViewState::SUB;
            page.build_sub_view();
        } else if (alias_hint_lbl_) {
            lv_label_set_text(alias_hint_lbl_, "Set failed");
            lv_obj_set_style_text_color(alias_hint_lbl_, lv_color_hex(0xFF4444), LV_PART_MAIN);
        }
        return;
    }
    if (key == KEY_BACKSPACE) {
        if (!alias_input_.empty())
            alias_input_.pop_back();
        alias_update_display();
        return;
    }
    if (page.cur_elm_ && page.cur_elm_->utf8[0] && alias_input_.size() < CP0_BT_NAME_MAX - 1) {
        const char *text = page.cur_elm_->utf8;
        while (*text && alias_input_.size() < CP0_BT_NAME_MAX - 1) {
            unsigned char ch = static_cast<unsigned char>(*text++);
            if (alias_char_allowed(ch))
                alias_input_ += static_cast<char>(ch);
        }
        alias_update_display();
    }
}

void Bluetooth::enter_scan(UISetupPage &page)
{
    list_mode_ = ListMode::Scan;
    page.view_state_ = UISetupPage::ViewState::BT_LIST;
    list_sel_ = 0;
    start_scan_timer(page);
}

void Bluetooth::build_list(UISetupPage &page)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    rebuild_rows();

    cp0_bt_status_t st = get_status();
    char title_buf[96];
    snprintf(title_buf, sizeof(title_buf), "%s: %s  %.24s",
             list_mode_ == ListMode::Scan ? "Scan" : "Connected",
             st.powered ? "On" : "Off", st.address[0] ? st.address : "--");
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, title_buf);
    lv_obj_set_pos(title, 8, 2);
    lv_obj_set_style_text_color(title, lv_color_hex(0x58A6FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
    UISetupPage::apply_overflow_handling(title, 8, UISetupPage::WIFI_TITLE_BOX_W, true);

    if (rows_.empty()) {
        lv_obj_t *empty = lv_label_create(cont);
        if (!st.powered)
            lv_label_set_text(empty, "Bluetooth is off. Enable Power first.");
        else if (list_mode_ == ListMode::Scan)
            lv_label_set_text(empty, "Scanning...");
        else
            lv_label_set_text(empty, "No connected devices.");
        lv_obj_set_pos(empty, 8, 50);
        lv_obj_set_width(empty, 300);
        lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(empty, lv_color_hex(0x666666), LV_PART_MAIN);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_12, LV_PART_MAIN);
    }

    constexpr int list_y = 22;
    constexpr int row_step = 20;
    constexpr int hint_y = UISetupPage::LIST_H - 14;
    constexpr int list_bottom_gap = 8;
    int visible = (hint_y - list_bottom_gap - list_y) / row_step;
    if (visible < 1) visible = 1;
    int offset = list_sel_ - visible / 2;
    if (offset < 0) offset = 0;
    if (offset > (int)rows_.size() - visible) offset = (int)rows_.size() - visible;
    if (offset < 0) offset = 0;

    for (int vi = 0; vi < visible && (vi + offset) < (int)rows_.size(); ++vi) {
        int row_index = vi + offset;
        const ListRow &row = rows_[row_index];
        int y = list_y + vi * row_step;

        if (row.is_header) {
            lv_obj_t *header = lv_label_create(cont);
            lv_label_set_text(header, row.title);
            lv_obj_set_pos(header, 8, y + 3);
            lv_obj_set_style_text_color(header, lv_color_hex(0x888888), LV_PART_MAIN);
            lv_obj_set_style_text_font(header, launcher_fonts().get("Montserrat-Bold.ttf", 10, LV_FREETYPE_FONT_STYLE_BOLD), LV_PART_MAIN);
            continue;
        }

        bool sel = (row_index == list_sel_);
        cp0_bt_device_t *dev = &devices_[row.device_index];
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

        uint32_t tc = dev->connected ? 0x58A6FF : (sel ? 0xFFFFFF : 0xCCCCCC);
        lv_obj_t *name = lv_label_create(cont);
        lv_label_set_text(name, dev->name[0] ? dev->name : dev->address);
        lv_obj_set_pos(name, 8, y + 1);
        lv_obj_set_width(name, 150);
        lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(name, lv_color_hex(tc), LV_PART_MAIN);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_12, LV_PART_MAIN);

        lv_obj_t *addr = lv_label_create(cont);
        lv_label_set_text(addr, dev->address);
        lv_obj_set_pos(addr, 8, y + 12);
        lv_obj_set_width(addr, 190);
        lv_label_set_long_mode(addr, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(addr, lv_color_hex(sel ? 0xBBBBBB : 0x777777), LV_PART_MAIN);
        lv_obj_set_style_text_font(addr, &lv_font_montserrat_10, LV_PART_MAIN);

        char state_buf[32];
        if (dev->connected)
            snprintf(state_buf, sizeof(state_buf), "Connected");
        else if (dev->paired)
            snprintf(state_buf, sizeof(state_buf), "Paired");
        else
            snprintf(state_buf, sizeof(state_buf), "%d", dev->rssi);
        lv_obj_t *state = lv_label_create(cont);
        lv_label_set_text(state, state_buf);
        lv_obj_set_pos(state, 226, y + 4);
        lv_obj_set_width(state, 88);
        lv_label_set_long_mode(state, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(state, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_color(state, lv_color_hex(tc), LV_PART_MAIN);
        lv_obj_set_style_text_font(state, &lv_font_montserrat_10, LV_PART_MAIN);
    }

    lv_obj_t *hint = lv_label_create(cont);
    lv_label_set_text(hint, list_mode_ == ListMode::Scan
                                 ? "OK:act R:restart ESC:back"
                                 : "OK:disconnect D:remove ESC:back");
    lv_obj_set_pos(hint, 8, hint_y);
    lv_obj_set_width(hint, 304);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN);
}

void Bluetooth::rebuild_rows()
{
    rows_.clear();
    if (list_mode_ == ListMode::Managed) {
        bool has_connected = false;
        for (int i = 0; i < device_count_; ++i) {
            if (should_hide_device(devices_[i]))
                continue;
            if (devices_[i].connected) {
                if (!has_connected) {
                    rows_.push_back({-1, "Connected Devices", true});
                    has_connected = true;
                }
                rows_.push_back({i, nullptr, false});
            }
        }
    } else {
        bool has_devices = false;
        for (int i = 0; i < device_count_; ++i) {
            if (should_hide_device(devices_[i]))
                continue;
            if (!has_devices) {
                rows_.push_back({-1, "Discovered Devices", true});
                has_devices = true;
            }
            rows_.push_back({i, nullptr, false});
        }
    }
    if (list_sel_ >= (int)rows_.size())
        list_sel_ = rows_.empty() ? 0 : (int)rows_.size() - 1;
    if (!rows_.empty() && rows_[list_sel_].is_header)
        select_next_device(1);
}

bool Bluetooth::should_hide_device(const cp0_bt_device_t &dev) const
{
    if (!named_only_)
        return false;
    if (!dev.name[0])
        return true;
    std::string name_hex = normalized_mac_text(dev.name);
    std::string addr_hex = normalized_mac_text(dev.address);
    return !name_hex.empty() && (name_hex == addr_hex || name_hex.size() == 12);
}

std::string Bluetooth::normalized_mac_text(const char *text)
{
    std::string out;
    if (!text)
        return out;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
        if (std::isxdigit(*p))
            out.push_back((char)std::tolower(*p));
        else if (*p != ':' && *p != '-' && *p != '_' && *p != ' ')
            return std::string();
    }
    return out;
}

int Bluetooth::selected_device_index() const
{
    if (list_sel_ < 0 || list_sel_ >= (int)rows_.size())
        return -1;
    return rows_[list_sel_].is_header ? -1 : rows_[list_sel_].device_index;
}

void Bluetooth::select_next_device(int direction)
{
    if (rows_.empty())
        return;
    int idx = list_sel_;
    for (int steps = 0; steps < (int)rows_.size(); ++steps) {
        idx += direction;
        if (idx < 0 || idx >= (int)rows_.size())
            return;
        if (!rows_[idx].is_header) {
            list_sel_ = idx;
            return;
        }
    }
}

void Bluetooth::show_action(UISetupPage &page, const char *msg, uint32_t color)
{
    lv_obj_t *cont = page.ui_obj_["list_cont"];
    lv_obj_clean(cont);
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, msg);
    lv_obj_set_pos(lbl, 8, 60);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_refr_now(NULL);
}

void Bluetooth::activate_selected(UISetupPage &page)
{
    if (action_busy_)
        return;
    int dev_index = selected_device_index();
    if (dev_index < 0)
        return;
    cp0_bt_device_t dev = devices_[dev_index];
    bool from_scan = list_mode_ == ListMode::Scan;
    if (from_scan)
        stop_scan_timer();
    action_busy_ = true;
    if (dev.connected)
        show_action(page, "Disconnecting...");
    else if (dev.paired)
        show_action(page, "Connecting...");
    else
        show_action(page, "Pairing...");

    struct BtActionResult {
        Bluetooth *bt;
        UISetupPage *page;
        int ret;
        bool from_scan;
    };

    std::thread([this, &page, dev, from_scan]() {
        int ret = -1;
        if (dev.connected) {
            ret = device_command("BtDisconnect", dev.address);
        } else if (dev.paired) {
            ret = device_command("BtConnect", dev.address);
        } else {
            ret = device_command("BtPair", dev.address);
            if (ret == 0)
                ret = device_command("BtConnect", dev.address);
        }

        BtActionResult *result = new BtActionResult{this, &page, ret, from_scan};
        lv_async_call([](void *user) {
            BtActionResult *result = static_cast<BtActionResult *>(user);
            Bluetooth *bt = result->bt;
            UISetupPage *page = result->page;
            if (bt && page) {
                bt->action_busy_ = false;
                if (result->ret != 0) {
                    bt->show_action(*page, "Bluetooth action failed", 0xFF4444);
                } else if (result->from_scan) {
                    bt->list_mode_ = ListMode::Managed;
                }
                bt->refresh_devices();
                if (page->view_state_ == UISetupPage::ViewState::BT_LIST)
                    bt->build_list(*page);
            }
            delete result;
        }, result);
    }).detach();
}

void Bluetooth::remove_selected(UISetupPage &page)
{
    int dev_index = selected_device_index();
    if (dev_index < 0)
        return;
    show_action(page, "Removing...");
    int ret = device_command("BtRemove", devices_[dev_index].address);
    if (ret != 0) {
        show_action(page, "Remove failed", 0xFF4444);
        cp0_signal_process_api({"DelayMs", "1200"}, nullptr);
    }
    refresh_devices();
    build_list(page);
}

void Bluetooth::handle_list_key(UISetupPage &page, uint32_t key)
{
    switch (key) {
    case KEY_UP:
        select_next_device(-1);
        build_list(page);
        break;
    case KEY_DOWN:
        select_next_device(1);
        build_list(page);
        break;
    case KEY_ENTER:
        activate_selected(page);
        break;
    case KEY_D:
        if (list_mode_ == ListMode::Managed)
            remove_selected(page);
        break;
    case KEY_R:
        if (list_mode_ == ListMode::Scan) {
            start_scan_timer(page);
        } else {
            refresh_devices();
            build_list(page);
        }
        break;
    case KEY_ESC:
    case KEY_LEFT:
        stop_scan_timer();
        page.view_state_ = UISetupPage::ViewState::SUB;
        page.build_sub_view();
        break;
    default:
        break;
    }
}

void Bluetooth::copy_string(char *dst, size_t dst_size, const std::string &src)
{
    if (!dst || dst_size == 0)
        return;
    std::snprintf(dst, dst_size, "%s", src.c_str());
}

std::vector<std::string> Bluetooth::split_char(const std::string &line, char delimiter)
{
    std::vector<std::string> cols;
    std::string item;
    std::istringstream row(line);
    while (std::getline(row, item, delimiter))
        cols.push_back(item);
    return cols;
}

bool Bluetooth::decode_status(const std::string &data, cp0_bt_status_t &st)
{
    auto cols = split_char(data, '\t');
    if (cols.size() < 3)
        return false;
    st.powered = std::atoi(cols[0].c_str());
    copy_string(st.address, sizeof(st.address), cols[1]);
    st.discoverable = std::atoi(cols[2].c_str());
    if (cols.size() >= 4)
        copy_string(st.alias, sizeof(st.alias), cols[3]);
    return true;
}

int Bluetooth::decode_devices(const std::string &data, cp0_bt_device_t *out, int max_devices)
{
    if (!out || max_devices <= 0)
        return 0;
    int count = 0;
    std::istringstream lines(data);
    std::string line;
    while (count < max_devices && std::getline(lines, line)) {
        if (line.empty())
            continue;
        auto cols = split_char(line, '\t');
        if (cols.size() < 4)
            continue;
        cp0_bt_device_t dev{};
        copy_string(dev.address, sizeof(dev.address), cols[0]);
        dev.paired = std::atoi(cols[1].c_str());
        dev.connected = std::atoi(cols[2].c_str());
        dev.rssi = std::atoi(cols[3].c_str());
        if (cols.size() >= 6)
            copy_string(dev.name, sizeof(dev.name), cols[5]);
        else if (cols.size() >= 4)
            copy_string(dev.name, sizeof(dev.name), cols[3]);
        out[count++] = dev;
    }
    return count;
}

int Bluetooth::api_int(std::list<std::string> args, int default_value)
{
    int ret = default_value;
    cp0_signal_bt_api(std::move(args), [&](int code, std::string) {
        ret = code;
    });
    return ret;
}

cp0_bt_status_t Bluetooth::get_status()
{
    cp0_bt_status_t st{};
    cp0_signal_bt_api({"BtStatus"}, [&](int code, std::string data) {
        if (code == 0)
            decode_status(data, st);
    });
    return st;
}

int Bluetooth::set_power(int on)
{
    return api_int({"BtPower", std::to_string(on)});
}

int Bluetooth::set_alias(const std::string &alias)
{
    return api_int({"BtAlias", alias});
}

int Bluetooth::set_discoverable(int on)
{
    return api_int({"BtDiscoverable", std::to_string(on)});
}

int Bluetooth::device_command(const char *cmd, const char *address)
{
    return api_int({cmd ? std::string(cmd) : std::string(),
                   address ? std::string(address) : std::string()});
}

int Bluetooth::device_list(const char *cmd, cp0_bt_device_t *out, int max_devices)
{
    int count = 0;
    cp0_signal_bt_api({cmd ? std::string(cmd) : std::string(), std::to_string(max_devices)},
                      [&](int code, std::string data) {
                          count = out && max_devices > 0 ? decode_devices(data, out, max_devices) : code;
                      });
    return count;
}

void Bluetooth::refresh_status(UISetupPage &page)
{
    cp0_bt_status_t st = get_status();
    for (auto &m : page.menu_items_) {
        if (m.label != "Bluetooth") continue;
        m.sub_items[0].toggle_state = st.powered != 0;
        discoverable_ = st.discoverable != 0;
        alias_ = st.alias[0] ? st.alias : "CardputerZero";
        m.sub_items[1].label = "Alias: " + alias_;
        m.sub_items[2].toggle_state = discoverable_;
        break;
    }
}

void Bluetooth::toggle_power(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Bluetooth") continue;
        bool on = m.sub_items[0].toggle_state;
        if (!on)
            stop_scan_timer();
        set_power(on ? 1 : 0);
        refresh_status(page);
        break;
    }
}

void Bluetooth::toggle_named_only(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Bluetooth") continue;
        named_only_ = m.sub_items[3].toggle_state;
        UISetupPage::config_set_int("bt_named_only", named_only_ ? 1 : 0);
        UISetupPage::config_save();
        break;
    }
    if (page.view_state_ == UISetupPage::ViewState::BT_LIST)
        build_list(page);
}

void Bluetooth::toggle_discoverable(UISetupPage &page)
{
    for (auto &m : page.menu_items_) {
        if (m.label != "Bluetooth") continue;
        discoverable_ = m.sub_items[2].toggle_state;
        if (set_discoverable(discoverable_ ? 1 : 0) != 0) {
            m.sub_items[2].toggle_state = !discoverable_;
            discoverable_ = m.sub_items[2].toggle_state;
        }
        break;
    }
}

void Bluetooth::start_scan_timer(UISetupPage &page)
{
    stop_scan_timer();
    discovery_active_ = api_int({"BtDiscoveryStart"}, 0) == 0;
    refresh_devices();
    build_list(page);
    if (!discovery_active_)
        return;
    scan_timer_ = lv_timer_create([](lv_timer_t *t) {
        UISetupPage *self = (UISetupPage *)lv_timer_get_user_data(t);
        if (!self || self->view_state_ != UISetupPage::ViewState::BT_LIST ||
            self->bluetooth_.list_mode_ != ListMode::Scan)
            return;
        self->bluetooth_.refresh_devices();
        self->bluetooth_.build_list(*self);
    }, 2500, &page);
}

void Bluetooth::stop_scan_timer()
{
    if (scan_timer_) {
        lv_timer_delete(scan_timer_);
        scan_timer_ = nullptr;
    }
    if (discovery_active_) {
        api_int({"BtDiscoveryStop"}, 0);
        discovery_active_ = false;
    }
}

void Bluetooth::refresh_devices()
{
    if (list_mode_ == ListMode::Managed)
        device_count_ = device_list("BtConnectedList", devices_, CP0_BT_DEVICE_MAX);
    else
        device_count_ = device_list("BtList", devices_, CP0_BT_DEVICE_MAX);
    if (device_count_ < 0)
        device_count_ = 0;
    if (device_count_ == 0)
        list_sel_ = 0;
}

void Bluetooth::do_scan(UISetupPage &page)
{
    enter_scan(page);
}

} // namespace setting
