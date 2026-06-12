#pragma once
#include "sample_log.h"
#include "../ui_app_page.hpp"
#include "compat/input_keys.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>

// ============================================================
//  File Browser  UIFilePage
//  Screen: 320 x 170  (top bar 20px, ui_APP_Container 320x150)
//
//  Features:
//    - Browse filesystem starting at "/"
//    - Directories first (sorted), then files (sorted)
//    - Navigate with UP/DOWN, enter directory with RIGHT/ENTER
//    - LEFT or ESC goes to parent (ESC at root goes home)
// ============================================================

class UIFilePage : public AppPage
{
    struct FileEntry
    {
        std::string name;
        bool        is_dir;
        off_t       size;       // file size in bytes (0 for dirs)
    };

public:
    UIFilePage() : AppPage()
    {
        set_page_title("FILES");
        current_path_ = "/";
        creat_UI();
        event_handler_init();
    }
    ~UIFilePage() {}

private:
    // ==================== data members ====================
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    std::string              current_path_;
    std::vector<FileEntry>   entries_;
    int selected_idx_ = 0;

    // layout constants (content area 320x150, title bar 22px, list area 128px)
    static constexpr int ITEM_H       = 30;
    static constexpr int VISIBLE_ROWS = 4;
    static constexpr int TITLE_H      = 22;
    static constexpr int LIST_H       = 128;

    // ==================== path utilities ====================
    static std::string path_parent(const std::string &path)
    {
        if (path.empty()) return "/";
        std::vector<char> buf(path.begin(), path.end());
        buf.push_back('\0');
        char *p = dirname(buf.data());
        return std::string(p ? p : "/");
    }

    static std::string path_join(const std::string &base, const std::string &name)
    {
        if (base.empty()) return name;
        if (name.empty()) return base;
        if (base == "/") return "/" + name;
        if (!base.empty() && base.back() == '/') return base + name;
        return base + "/" + name;
    }

    // ==================== format file size ====================
    static void format_size(off_t size, char *buf, size_t buflen)
    {
        if (size < 1024)
            snprintf(buf, buflen, "%dB", (int)size);
        else if (size < 1024 * 1024)
            snprintf(buf, buflen, "%.1fKB", size / 1024.0);
        else
            snprintf(buf, buflen, "%.1fMB", size / (1024.0 * 1024.0));
    }

    // ==================== read directory ====================
    void read_directory()
    {
        entries_.clear();

        DIR *dp = opendir(current_path_.c_str());
        if (!dp)
        {
            SLOGI("[File] opendir failed: %s", current_path_.c_str());
            return;
        }

        struct dirent *ent;
        while ((ent = readdir(dp)) != nullptr)
        {
            // skip "." and ".."
            if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' ||
                (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
                continue;

            std::string full = path_join(current_path_, ent->d_name);
            struct stat st;
            if (stat(full.c_str(), &st) != 0)
                continue;

            FileEntry fe;
            fe.name   = ent->d_name;
            fe.is_dir = S_ISDIR(st.st_mode);
            fe.size   = fe.is_dir ? 0 : st.st_size;
            entries_.push_back(fe);
        }
        closedir(dp);

        // sort: directories first (alphabetically), then files (alphabetically)
        std::sort(entries_.begin(), entries_.end(),
                  [](const FileEntry &a, const FileEntry &b) {
                      if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
                      return a.name < b.name;
                  });

        // clamp selected index
        if (selected_idx_ >= (int)entries_.size())
            selected_idx_ = entries_.empty() ? 0 : (int)entries_.size() - 1;
    }

    // ==================== UI construction ====================
    void creat_UI()
    {
        // ---- background ----
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, 320, 150);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        // ---- title bar ----
        lv_obj_t *title_bar = lv_obj_create(bg);
        lv_obj_set_size(title_bar, 320, TITLE_H);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title_bar, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

        // path label (left side of title bar)
        lv_obj_t *lbl_path = lv_label_create(title_bar);
        lv_label_set_text(lbl_path, "/");
        lv_obj_set_align(lbl_path, LV_ALIGN_LEFT_MID);
        lv_obj_set_width(lbl_path, 220);
        lv_label_set_long_mode(lbl_path, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_color(lbl_path, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_path, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["title_path"] = lbl_path;

        // hint (right side of title bar)
        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, "ESC:back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // ---- list container ----
        lv_obj_t *list_cont = lv_obj_create(bg);
        lv_obj_set_size(list_cont, 320, LIST_H);
        lv_obj_set_pos(list_cont, 0, TITLE_H);
        lv_obj_set_style_radius(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["list_cont"] = list_cont;

        // initial read and render
        read_directory();
        build_list_rows();
    }

    // ==================== build list rows ====================
    void build_list_rows()
    {
        lv_obj_t *list_cont = ui_obj_["list_cont"];
        lv_obj_clean(list_cont);

        // update title path
        if (ui_obj_.count("title_path") && ui_obj_["title_path"])
            lv_label_set_text(ui_obj_["title_path"], current_path_.c_str());

        if (entries_.empty())
        {
            lv_obj_t *lbl = lv_label_create(list_cont);
            lv_label_set_text(lbl, "(empty directory)");
            lv_obj_set_pos(lbl, 10, 50);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
            return;
        }

        int item_count = (int)entries_.size();
        int visible    = LIST_H / ITEM_H;

        // scroll offset to center selected item
        int offset = selected_idx_ - visible / 2;
        if (offset < 0) offset = 0;
        if (offset > item_count - visible) offset = item_count - visible;
        if (offset < 0) offset = 0;

        for (int vi = 0; vi < visible && (vi + offset) < item_count; ++vi)
        {
            int  mi  = vi + offset;
            bool sel = (mi == selected_idx_);
            create_row(list_cont, vi, mi, sel);
        }
    }

    // ==================== create single row ====================
    void create_row(lv_obj_t *parent, int visual_row, int idx, bool selected)
    {
        const FileEntry &fe = entries_[idx];

        // row background
        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_size(row, 318, ITEM_H - 2);
        lv_obj_set_pos(row, 1, visual_row * ITEM_H + 1);
        lv_obj_set_style_radius(row, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        if (selected)
        {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            // left highlight bar
            lv_obj_t *sel_bar = lv_obj_create(row);
            lv_obj_set_size(sel_bar, 3, ITEM_H - 8);
            lv_obj_set_pos(sel_bar, 2, 3);
            lv_obj_set_style_radius(sel_bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(sel_bar, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(sel_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(sel_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(sel_bar, LV_OBJ_FLAG_SCROLLABLE);
        }
        else
        {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x161B22), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        // divider line
        if (idx < (int)entries_.size() - 1)
        {
            lv_obj_t *div = lv_obj_create(parent);
            lv_obj_set_size(div, 310, 1);
            lv_obj_set_pos(div, 5, (visual_row + 1) * ITEM_H);
            lv_obj_set_style_radius(div, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(div, lv_color_hex(0x21262D), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(div, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(div, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
        }

        // icon indicator (folder or file)
        lv_obj_t *lbl_icon = lv_label_create(row);
        lv_label_set_text(lbl_icon, fe.is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE);
        lv_obj_set_pos(lbl_icon, 8, (ITEM_H - 2 - 14) / 2);
        lv_obj_set_style_text_color(lbl_icon,
            fe.is_dir ? lv_color_hex(0x58A6FF) : lv_color_hex(0x7EA8D8),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        // file/directory name
        char name_buf[256];
        if (fe.is_dir)
            snprintf(name_buf, sizeof(name_buf), "%s/", fe.name.c_str());
        else
            snprintf(name_buf, sizeof(name_buf), "%s", fe.name.c_str());

        lv_obj_t *lbl_name = lv_label_create(row);
        lv_label_set_text(lbl_name, name_buf);
        lv_obj_set_pos(lbl_name, 28, (ITEM_H - 2 - 14) / 2);
        lv_obj_set_width(lbl_name, fe.is_dir ? 270 : 210);
        lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_name,
            selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xE6EDF3),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        // file size (only for files)
        if (!fe.is_dir)
        {
            char size_buf[32];
            format_size(fe.size, size_buf, sizeof(size_buf));

            lv_obj_t *lbl_size = lv_label_create(row);
            lv_label_set_text(lbl_size, size_buf);
            lv_obj_set_pos(lbl_size, 260, (ITEM_H - 2 - 12) / 2);
            lv_obj_set_width(lbl_size, 56);
            lv_label_set_long_mode(lbl_size, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_color(lbl_size, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl_size, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        // arrow indicator for directories
        if (fe.is_dir)
        {
            lv_obj_t *lbl_arrow = lv_label_create(row);
            lv_label_set_text(lbl_arrow, LV_SYMBOL_RIGHT);
            lv_obj_set_pos(lbl_arrow, 300, (ITEM_H - 2 - 12) / 2);
            lv_obj_set_style_text_color(lbl_arrow,
                selected ? lv_color_hex(0x58A6FF) : lv_color_hex(0x3A4A5A),
                LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl_arrow, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    // ==================== navigation ====================
    void navigate_into(int idx)
    {
        if (idx < 0 || idx >= (int)entries_.size()) return;
        if (!entries_[idx].is_dir) return;

        current_path_ = path_join(current_path_, entries_[idx].name);
        selected_idx_ = 0;
        read_directory();
        build_list_rows();
    }

    void navigate_parent()
    {
        if (current_path_ == "/") return;
        current_path_ = path_parent(current_path_);
        selected_idx_ = 0;
        read_directory();
        build_list_rows();
    }

    // ==================== event binding ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(root_screen_, UIFilePage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIFilePage *self = static_cast<UIFilePage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }

    void event_handler(lv_event_t *e)
    {
        if (launcher_ui::events::is_key_released(e))
        {
            uint32_t key = launcher_ui::events::keyboard_key(e);
            int count = (int)entries_.size();

            switch (key)
            {
            case KEY_UP:
                if (selected_idx_ > 0)
                {
                    --selected_idx_;
                    build_list_rows();
                }
                break;

            case KEY_DOWN:
                if (selected_idx_ < count - 1)
                {
                    ++selected_idx_;
                    build_list_rows();
                }
                break;

            case KEY_RIGHT:
            case KEY_ENTER:
                if (count > 0 && entries_[selected_idx_].is_dir)
                    navigate_into(selected_idx_);
                break;

            case KEY_LEFT:
                navigate_parent();
                break;

            case KEY_ESC:
                if (current_path_ != "/")
                    navigate_parent();
                else if (navigate_home)
                    navigate_home();
                break;

            default:
                break;
            }
        }
    }
};
