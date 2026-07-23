/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#define APP_PAGE_IMPLEMENTATION_UNIT
#include "ui_app_file.hpp"

#include <cstdio>

namespace {

void format_size(std::int64_t size, char *buffer, std::size_t buffer_length)
{
    if (size < 1024)
        std::snprintf(buffer, buffer_length, "%lldB", static_cast<long long>(size));
    else if (size < 1024 * 1024)
        std::snprintf(buffer, buffer_length, "%.1fKB", size / 1024.0);
    else
        std::snprintf(buffer, buffer_length, "%.1fMB", size / (1024.0 * 1024.0));
}

} // namespace

bool UIFilePage::create_ui()
{
    if (!ui_APP_Container) return false;
    lv_obj_t *background = lv_obj_create(ui_APP_Container);
    if (!background) return false;
    auto rollback = [&] {
        lv_obj_delete(background);
        view_.clear();
        return false;
    };
    lv_obj_set_size(background, 320, 150);
    lv_obj_set_pos(background, 0, 0);
    lv_obj_set_style_radius(background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(background, lv_color_hex(0x0D1117),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(background, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(background, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title_bar = lv_obj_create(background);
    if (!title_bar) return rollback();
    lv_obj_set_size(title_bar, 320, TITLE_H);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1F3A5F),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(title_bar, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *path_label = lv_label_create(title_bar);
    if (!path_label) return rollback();
    lv_label_set_text(path_label, "/");
    lv_obj_set_align(path_label, LV_ALIGN_LEFT_MID);
    lv_obj_set_width(path_label, 220);
    lv_label_set_long_mode(path_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(path_label, lv_color_hex(0xFFFFFF),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(path_label, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *hint_label = lv_label_create(title_bar);
    if (hint_label) {
        lv_label_set_text(hint_label, "ESC:back");
        lv_obj_set_align(hint_label, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(hint_label, -4);
        lv_obj_set_style_text_color(hint_label, lv_color_hex(0x7EA8D8),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_10,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_obj_t *list_container = lv_obj_create(background);
    if (!list_container) return rollback();
    lv_obj_set_size(list_container, 320, LIST_H);
    lv_obj_set_pos(list_container, 0, TITLE_H);
    lv_obj_set_style_radius(list_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(list_container, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(list_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(list_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);
    if (!view_.commit_tree(background, path_label, list_container)) return rollback();
    lv_obj_add_event_cb(background, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
    lv_obj_add_event_cb(path_label, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
    lv_obj_add_event_cb(list_container, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);

    read_directory();
    build_list_rows();
    return true;
}

bool UIFilePage::build_list_rows()
{
    auto *background = static_cast<lv_obj_t *>(view_.background());
    auto *old_list = static_cast<lv_obj_t *>(view_.list_container());
    if (!background || !old_list) return false;
    lv_obj_t *list_container = lv_obj_create(background);
    if (!list_container) return false;
    lv_obj_set_size(list_container, 320, LIST_H);
    lv_obj_set_pos(list_container, 0, TITLE_H);
    lv_obj_set_style_radius(list_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(list_container, LV_OPA_TRANSP,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(list_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(list_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);

    bool built = true;

    const auto &entries = model_.entries();
    if (entries.empty()) {
        lv_obj_t *empty_label = lv_label_create(list_container);
        built = empty_label != nullptr;
        if (empty_label) {
            lv_label_set_text(empty_label, model_.listing_failed()
                                               ? "(unable to list directory)"
                                               : "(empty directory)");
            lv_obj_set_pos(empty_label, 10, 50);
            lv_obj_set_style_text_color(empty_label, lv_color_hex(0x555555),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_12,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    } else {
        const int entry_count = static_cast<int>(entries.size());
        constexpr int visible_count = LIST_H / ITEM_H;
        int offset = model_.selected_index() - visible_count / 2;
        if (offset < 0) offset = 0;
        if (offset > entry_count - visible_count) offset = entry_count - visible_count;
        if (offset < 0) offset = 0;

        for (int visual_row = 0;
             built && visual_row < visible_count && visual_row + offset < entry_count;
             ++visual_row) {
            const int entry_index = visual_row + offset;
            built = create_row(list_container, visual_row, entry_index,
                               entry_index == model_.selected_index());
        }
    }
    if (!built) {
        lv_obj_delete(list_container);
        return false;
    }

    lv_obj_add_event_cb(list_container, static_owned_obj_delete_cb, LV_EVENT_DELETE, this);
    if (!view_.replace_list(old_list, list_container)) {
        lv_obj_delete(list_container);
        return false;
    }
    if (auto *path_label = static_cast<lv_obj_t *>(view_.path_label()))
        lv_label_set_text(path_label, model_.current_path().c_str());
    lv_obj_delete(old_list);
    return true;
}

bool UIFilePage::create_row(lv_obj_t *parent, int visual_row, int index, bool selected)
{
    if (!parent || index < 0 || index >= static_cast<int>(model_.entries().size())) return false;
    const FileBrowserEntry &entry = model_.entries()[static_cast<std::size_t>(index)];
    lv_obj_t *row = lv_obj_create(parent);
    if (!row) return false;
    lv_obj_set_size(row, 318, ITEM_H - 2);
    lv_obj_set_pos(row, 1, visual_row * ITEM_H + 1);
    lv_obj_set_style_radius(row, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(row, lv_color_hex(selected ? 0x1F3A5F : 0x161B22),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (selected) {
        lv_obj_t *selection_bar = lv_obj_create(row);
        if (!selection_bar) return false;
        {
            lv_obj_set_size(selection_bar, 3, ITEM_H - 8);
            lv_obj_set_pos(selection_bar, 2, 3);
            lv_obj_set_style_radius(selection_bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(selection_bar, lv_color_hex(0x1F6FEB),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(selection_bar, LV_OPA_COVER,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(selection_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(selection_bar, LV_OBJ_FLAG_SCROLLABLE);
        }
    }

    if (index < static_cast<int>(model_.entries().size()) - 1) {
        lv_obj_t *divider = lv_obj_create(parent);
        if (!divider) return false;
        lv_obj_set_size(divider, 310, 1);
        lv_obj_set_pos(divider, 5, (visual_row + 1) * ITEM_H);
        lv_obj_set_style_radius(divider, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(divider, lv_color_hex(0x21262D),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(divider, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_t *icon_label = lv_label_create(row);
    if (!icon_label) return false;
    lv_label_set_text(icon_label, entry.is_directory ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE);
    lv_obj_set_pos(icon_label, 8, (ITEM_H - 16) / 2);
    lv_obj_set_style_text_color(icon_label,
                                lv_color_hex(entry.is_directory ? 0x58A6FF : 0x7EA8D8),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    char name_text[256];
    std::snprintf(name_text, sizeof(name_text), entry.is_directory ? "%s/" : "%s",
                  entry.name.c_str());
    lv_obj_t *name_label = lv_label_create(row);
    if (!name_label) return false;
    lv_label_set_text(name_label, name_text);
    lv_obj_set_pos(name_label, 28, (ITEM_H - 16) / 2);
    lv_obj_set_width(name_label, entry.is_directory ? 270 : 210);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(name_label,
                                lv_color_hex(selected ? 0xFFFFFF : 0xE6EDF3),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    if (!entry.is_directory) {
        char size_text[32];
        format_size(entry.size, size_text, sizeof(size_text));
        lv_obj_t *size_label = lv_label_create(row);
        if (!size_label) return false;
        lv_label_set_text(size_label, size_text);
        lv_obj_set_pos(size_label, 260, (ITEM_H - 14) / 2);
        lv_obj_set_width(size_label, 56);
        lv_label_set_long_mode(size_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(size_label, lv_color_hex(0x555555),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(size_label, &lv_font_montserrat_10,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_t *arrow_label = lv_label_create(row);
        if (!arrow_label) return false;
        lv_label_set_text(arrow_label, LV_SYMBOL_RIGHT);
        lv_obj_set_pos(arrow_label, 300, (ITEM_H - 14) / 2);
        lv_obj_set_style_text_color(arrow_label,
                                    lv_color_hex(selected ? 0x58A6FF : 0x3A4A5A),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(arrow_label, &lv_font_montserrat_10,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    return true;
}
