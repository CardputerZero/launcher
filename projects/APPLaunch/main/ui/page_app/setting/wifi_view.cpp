#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"
#include "setup_page_access.hpp"

namespace setting {

namespace {

lv_obj_t *create_label(lv_obj_t *parent, const char *text, int x, int y, uint32_t color,
                       const lv_font_t *font)
{
    if (!parent) return nullptr;
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return nullptr;
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    return label;
}

lv_obj_t *begin_view(lv_obj_t *container)
{
    if (!container) return nullptr;
    lv_obj_t *view = lv_obj_create(container);
    if (!view) return nullptr;
    lv_obj_set_size(view, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(view, 0, 0);
    lv_obj_set_style_radius(view, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(view, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(view, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
    return view;
}

void commit_view(lv_obj_t *container, lv_obj_t *view)
{
    while (lv_obj_get_child_count(container) > 1) {
        lv_obj_t *child = lv_obj_get_child(container, 0);
        if (child == view) child = lv_obj_get_child(container, 1);
        lv_obj_delete(child);
    }
    lv_obj_remove_flag(view, LV_OBJ_FLAG_HIDDEN);
}

} // namespace

bool WiFi::build_list(UISetupPage &page)
{
    SetupPageAccess access(page);
    lv_obj_t *container = access.content_container();
    lv_obj_t *view = begin_view(container);
    if (!view) return false;
    auto fail = [&] { lv_obj_delete(view); return false; };

    cp0_wifi_status_t status{};
    cp0_wifi_status_read(&status);
    char title_text[128];
    if (status.connected)
        snprintf(title_text, sizeof(title_text), "Connected WiFi: %.64s  %.40s", status.ssid,
                 status.ip);
    else
        snprintf(title_text, sizeof(title_text), "WiFi: Not connected");
    lv_obj_t *title = create_label(
        view, title_text, 8, 2, 0x58A6FF,
        launcher_fonts().get("Montserrat-Bold.ttf", 12, LV_FREETYPE_FONT_STYLE_BOLD));
    if (!title) return fail();
    access.apply_overflow(title, 8, access.layout().wifi_title_width, true);

    if (!create_label(view, "SSID", 8, 18, 0x888888, &lv_font_montserrat_10) ||
        !create_label(view, "Security", 180, 18, 0x888888, &lv_font_montserrat_10) ||
        !create_label(view, "Signal", 270, 18, 0x888888, &lv_font_montserrat_10))
        return fail();

    if (ap_count_ == 0) {
        if (!create_label(view, "No networks found. Press R to rescan.", 8, 50, 0x666666,
                          &lv_font_montserrat_12))
            return fail();
        commit_view(container, view);
        return true;
    }

    constexpr int visible_count = 5;
    int offset = list_sel_ - visible_count / 2;
    if (offset < 0) offset = 0;
    if (offset > ap_count_ - visible_count) offset = ap_count_ - visible_count;
    if (offset < 0) offset = 0;

    for (int visible_index = 0;
         visible_index < visible_count && visible_index + offset < ap_count_; ++visible_index) {
        const int ap_index = visible_index + offset;
        const bool selected = ap_index == list_sel_;
        const cp0_wifi_ap_t &access_point = aps_[ap_index];
        const int y = 30 + visible_index * 22;

        if (selected) {
            lv_obj_t *background = lv_obj_create(view);
            if (!background) return fail();
            lv_obj_set_size(background, access.layout().screen_width - 8, 20);
            lv_obj_set_pos(background, 4, y);
            lv_obj_set_style_radius(background, 2, LV_PART_MAIN);
            lv_obj_set_style_bg_color(background, lv_color_hex(0x1F3A5F), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(background, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(background, 0, LV_PART_MAIN);
            lv_obj_clear_flag(background, LV_OBJ_FLAG_SCROLLABLE);
        }

        const uint32_t text_color =
            access_point.in_use ? 0x58A6FF : (selected ? 0xFFFFFF : 0xCCCCCC);
        char ssid_text[CP0_WIFI_SSID_MAX + 4];
        snprintf(ssid_text, sizeof(ssid_text), has_saved_profile(access_point.ssid) ? "%s *" : "%s",
                 access_point.ssid);
        lv_obj_t *ssid = create_label(view, ssid_text, 8, y + 2, text_color,
                                      &lv_font_montserrat_12);
        if (!ssid) return fail();
        lv_obj_set_width(ssid, 165);
        lv_label_set_long_mode(ssid, LV_LABEL_LONG_CLIP);

        if (!create_label(view, access_point.security[0] ? access_point.security : "Open", 180,
                          y + 2, text_color, &lv_font_montserrat_10))
            return fail();
        char signal_text[16];
        snprintf(signal_text, sizeof(signal_text), "%d%%", access_point.signal);
        if (!create_label(view, signal_text, 275, y + 2, text_color,
                          &lv_font_montserrat_10))
            return fail();
    }

    if (!create_label(view, "OK:connect  R:rescan  D:forget  ESC:back", 8,
                      access.content_height() - 14, 0x555555, &lv_font_montserrat_10))
        return fail();
    commit_view(container, view);
    return true;
}

bool WiFi::show_connecting(UISetupPage &page, const char *ssid)
{
    lv_obj_t *container = SetupPageAccess(page).content_container();
    lv_obj_t *view = begin_view(container);
    if (!view) return false;
    char message[128];
    snprintf(message, sizeof(message), "Connecting to %s...", ssid);
    if (!create_label(view, message, 8, 60, 0x58A6FF, &lv_font_montserrat_14)) {
        lv_obj_delete(view);
        return false;
    }
    commit_view(container, view);
    lv_refr_now(nullptr);
    return true;
}

bool WiFi::show_error(UISetupPage &page, const char *message)
{
    lv_obj_t *container = SetupPageAccess(page).content_container();
    lv_obj_t *view = begin_view(container);
    if (!view) return false;
    if (!create_label(view, message, 8, 60, 0xFF4444, &lv_font_montserrat_14)) {
        lv_obj_delete(view);
        return false;
    }
    commit_view(container, view);
    lv_refr_now(nullptr);
    return true;
}

bool WiFi::show_forget_confirmation(UISetupPage &page, const std::string &ssid)
{
    lv_obj_t *container = SetupPageAccess(page).content_container();
    lv_obj_t *view = begin_view(container);
    if (!view) return false;
    char message[128];
    snprintf(message, sizeof(message), "Forget '%s'?", ssid.c_str());
    if (!create_label(view, message, 8, 50, 0xFFAA00, &lv_font_montserrat_14) ||
        !create_label(view, "OK:confirm  ESC:cancel", 8, 75, 0x888888,
                      &lv_font_montserrat_10)) {
        lv_obj_delete(view);
        return false;
    }
    commit_view(container, view);
    lv_refr_now(nullptr);
    return true;
}

bool WiFi::show_pw_input(UISetupPage &page)
{
    SetupPageAccess access(page);
    lv_obj_t *container = access.content_container();
    lv_obj_t *view = begin_view(container);
    if (!view) return false;
    auto fail = [&] { lv_obj_delete(view); return false; };

    char title_text[128];
    snprintf(title_text, sizeof(title_text), "Connect: %s", password_model_.ssid().c_str());
    if (!create_label(view, title_text, 10, 10, 0x58A6FF, &lv_font_montserrat_12) ||
        !create_label(view, "Password:", 10, 35, 0xCCCCCC, &lv_font_montserrat_12))
        return fail();

    lv_obj_t *input = create_label(view, "_", 90, 35, 0xFFFFFF, &lv_font_montserrat_14);
    if (!input) return fail();
    lv_obj_add_event_cb(input, password_label_delete_cb, LV_EVENT_DELETE, this);
    lv_obj_set_width(input, 200);
    lv_label_set_long_mode(input, LV_LABEL_LONG_CLIP);

    lv_obj_t *hint = create_label(view, "Type pw, OK:connect, ESC:cancel", 10, 65,
                                  0x555555, &lv_font_montserrat_10);
    if (!hint) return fail();
    lv_obj_add_event_cb(hint, password_label_delete_cb, LV_EVENT_DELETE, this);

    commit_view(container, view);
    pw_input_lbl_ = input;
    pw_hint_lbl_ = hint;
    access.set_view(SetupViewState::WIFI_PW);
    return true;
}

void WiFi::pw_update_display()
{
    if (!pw_input_lbl_) return;
    const std::string display = password_model_.masked_display();
    lv_label_set_text(pw_input_lbl_, display.c_str());
}

} // namespace setting
