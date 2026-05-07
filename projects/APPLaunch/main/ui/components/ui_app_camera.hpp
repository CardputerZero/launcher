#pragma once
#include "ui_app_page.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include "hal/hal_process.h"
#include "compat/input_keys.h"

// ============================================================
//  Camera Capture  UICameraPage
//  Screen: 320 x 170  (top bar 20px, ui_APP_Container 320x150)
//
//  Keys:
//    ENTER      Capture photo (libcamera-still / raspistill)
//    SPACE      Toggle front/rear camera
//    UP/DOWN    Browse captured photos list
//    D          Delete selected photo
//    ESC        Go back home
// ============================================================
class UICameraPage : public app_base
{
public:
    UICameraPage() : app_base()
    {
        set_page_title("CAMERA");
        detect_camera();
        creat_UI();
        event_handler_init();
    }

    ~UICameraPage()
    {
        stop_capture();
        if (check_timer_) lv_timer_delete(check_timer_);
    }

private:
    // ==================== data members ====================
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    std::vector<std::string> photos_;
    int              selected_idx_  = 0;
    int              photo_counter_ = 0;
    int              camera_idx_    = 0;        // 0 = rear (default), 1 = front
    bool             camera_found_  = false;
    bool             capturing_     = false;
    hal_pid_t        capture_pid_   = -1;
    std::string      last_file_;
    lv_timer_t      *check_timer_   = nullptr;

    // ==================== camera detection ====================
    void detect_camera()
    {
        struct stat st;
        camera_found_ = (stat("/dev/video0", &st) == 0);
        printf("[Camera] /dev/video0 %s\n", camera_found_ ? "found" : "not found");
    }

    // ==================== UI construction ====================
    void creat_UI()
    {
        // background
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

        // title bar
        lv_obj_t *title_bar = lv_obj_create(bg);
        lv_obj_set_size(title_bar, 320, 22);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title_bar, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_title = lv_label_create(title_bar);
        lv_label_set_text(lbl_title, "Camera");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, "ENTER:Capture  ESC:Back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // content area
        lv_obj_t *content = lv_obj_create(bg);
        lv_obj_set_size(content, 320, 128);
        lv_obj_set_pos(content, 0, 22);
        lv_obj_set_style_radius(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["content"] = content;

        // preview placeholder (left side)
        lv_obj_t *preview = lv_obj_create(content);
        lv_obj_set_size(preview, 128, 96);
        lv_obj_set_pos(preview, 8, 4);
        lv_obj_set_style_radius(preview, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(preview, lv_color_hex(0x161B22), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(preview, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(preview, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(preview, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(preview, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_preview = lv_label_create(preview);
        lv_label_set_text(lbl_preview, "Preview");
        lv_obj_set_align(lbl_preview, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(lbl_preview, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_preview, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        // status label (right side)
        lv_obj_t *lbl_status = lv_label_create(content);
        lv_label_set_text(lbl_status, camera_found_ ? "Camera Ready" : "No Camera Detected");
        lv_obj_set_pos(lbl_status, 146, 4);
        lv_obj_set_style_text_color(lbl_status,
            camera_found_ ? lv_color_hex(0x2ECC71) : lv_color_hex(0xE74C3C),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_status"] = lbl_status;

        // camera index label
        lv_obj_t *lbl_cam = lv_label_create(content);
        lv_label_set_text(lbl_cam, "Cam: rear (0)");
        lv_obj_set_pos(lbl_cam, 146, 20);
        lv_obj_set_style_text_color(lbl_cam, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_cam, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_cam"] = lbl_cam;

        // photo count label
        lv_obj_t *lbl_count = lv_label_create(content);
        lv_label_set_text(lbl_count, "Photos: 0");
        lv_obj_set_pos(lbl_count, 146, 34);
        lv_obj_set_style_text_color(lbl_count, lv_color_hex(0xE6EDF3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_count, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_count"] = lbl_count;

        // last capture info
        lv_obj_t *lbl_last = lv_label_create(content);
        lv_label_set_text(lbl_last, "Last: (none)");
        lv_obj_set_pos(lbl_last, 146, 50);
        lv_obj_set_width(lbl_last, 168);
        lv_label_set_long_mode(lbl_last, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_color(lbl_last, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_last, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_last"] = lbl_last;

        // capture status (shows "Capturing..." while busy)
        lv_obj_t *lbl_cap_status = lv_label_create(content);
        lv_label_set_text(lbl_cap_status, "");
        lv_obj_set_pos(lbl_cap_status, 146, 64);
        lv_obj_set_style_text_color(lbl_cap_status, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_cap_status, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_cap_status"] = lbl_cap_status;

        // separator
        lv_obj_t *sep = lv_obj_create(content);
        lv_obj_set_size(sep, 300, 1);
        lv_obj_set_pos(sep, 10, 102);
        lv_obj_set_style_radius(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(sep, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

        // bottom key hints
        lv_obj_t *lbl_keys = lv_label_create(content);
        lv_label_set_text(lbl_keys, "SPACE:Toggle cam  UP/DN:Browse  D:Delete");
        lv_obj_set_pos(lbl_keys, 8, 106);
        lv_obj_set_width(lbl_keys, 304);
        lv_label_set_long_mode(lbl_keys, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_keys, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_keys, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // photos list container (bottom area, between sep and keys)
        lv_obj_t *list_cont = lv_obj_create(content);
        lv_obj_set_size(list_cont, 128, 20);
        lv_obj_set_pos(list_cont, 8, 80);
        lv_obj_set_style_radius(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["list_cont"] = list_cont;

        build_photo_list();
    }

    // ==================== photo list rows ====================
    void build_photo_list()
    {
        lv_obj_t *list_cont = ui_obj_["list_cont"];
        lv_obj_clean(list_cont);

        if (photos_.empty())
        {
            lv_obj_t *lbl = lv_label_create(list_cont);
            lv_label_set_text(lbl, "(no photos)");
            lv_obj_set_pos(lbl, 0, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
            return;
        }

        // show only the currently selected photo filename
        const std::string &path = photos_[selected_idx_];
        std::string display = path;
        size_t slash = path.rfind('/');
        if (slash != std::string::npos)
            display = path.substr(slash + 1);

        char buf[80];
        snprintf(buf, sizeof(buf), "[%d/%d] %s", selected_idx_ + 1, (int)photos_.size(), display.c_str());
        lv_obj_t *lbl = lv_label_create(list_cont);
        lv_label_set_text(lbl, buf);
        lv_obj_set_pos(lbl, 0, 0);
        lv_obj_set_width(lbl, 128);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xE6EDF3), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // ==================== update helpers ====================
    void update_count_label()
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Photos: %d", (int)photos_.size());
        lv_label_set_text(ui_obj_["lbl_count"], buf);
    }

    void update_last_label()
    {
        if (last_file_.empty())
        {
            lv_label_set_text(ui_obj_["lbl_last"], "Last: (none)");
            return;
        }
        // extract filename
        std::string fname = last_file_;
        size_t slash = last_file_.rfind('/');
        if (slash != std::string::npos)
            fname = last_file_.substr(slash + 1);

        // get file size
        struct stat st;
        long size_kb = 0;
        if (stat(last_file_.c_str(), &st) == 0)
            size_kb = st.st_size / 1024;

        char buf[128];
        snprintf(buf, sizeof(buf), "Last: %s (%ldKB)", fname.c_str(), size_kb);
        lv_label_set_text(ui_obj_["lbl_last"], buf);
    }

    void update_cam_label()
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Cam: %s (%d)",
                 camera_idx_ == 0 ? "rear" : "front", camera_idx_);
        lv_label_set_text(ui_obj_["lbl_cam"], buf);
    }

    // ==================== capture actions ====================
    void capture_photo()
    {
        if (capturing_) return;

        photo_counter_++;
        char fname[64];
        snprintf(fname, sizeof(fname), "/tmp/photo_%03d.jpg", photo_counter_);

        // build command: try libcamera-still first, fallback to raspistill
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "libcamera-still -o %s --width 640 --height 480 -t 1 --nopreview --camera %d 2>/dev/null "
            "|| raspistill -o %s -w 640 -h 480 -t 1 -cs %d 2>/dev/null",
            fname, camera_idx_, fname, camera_idx_);

        capture_pid_ = hal_process_spawn(cmd);
        capturing_ = true;
        last_file_ = fname;

        lv_label_set_text(ui_obj_["lbl_cap_status"], "Capturing...");
        printf("[Camera] Capturing: %s  pid=%d\n", fname, capture_pid_);

        // start a timer to poll for completion
        if (!check_timer_)
            check_timer_ = lv_timer_create(check_timer_cb, 500, this);
        else
            lv_timer_reset(check_timer_);
    }

    void stop_capture()
    {
        if (capture_pid_ > 0)
        {
            hal_process_stop(capture_pid_);
            capture_pid_ = -1;
        }
        capturing_ = false;
    }

    // timer to check if capture process completed
    static void check_timer_cb(lv_timer_t *t)
    {
        UICameraPage *self = static_cast<UICameraPage *>(lv_timer_get_user_data(t));
        if (self) self->on_check_tick();
    }

    void on_check_tick()
    {
        if (!capturing_) return;

        // check if the output file exists (capture completed)
        struct stat st;
        if (stat(last_file_.c_str(), &st) == 0 && st.st_size > 0)
        {
            // capture done
            capturing_ = false;
            capture_pid_ = -1;
            photos_.push_back(last_file_);
            selected_idx_ = (int)photos_.size() - 1;

            lv_label_set_text(ui_obj_["lbl_cap_status"], "Captured!");
            update_count_label();
            update_last_label();
            build_photo_list();
            printf("[Camera] Capture complete: %s  size=%ld\n", last_file_.c_str(), (long)st.st_size);

            // stop the check timer
            if (check_timer_)
            {
                lv_timer_delete(check_timer_);
                check_timer_ = nullptr;
            }
        }
    }

    void delete_selected()
    {
        if (photos_.empty()) return;
        if (capturing_) return;

        const std::string &file = photos_[selected_idx_];
        ::unlink(file.c_str());
        printf("[Camera] Deleted: %s\n", file.c_str());

        photos_.erase(photos_.begin() + selected_idx_);
        if (selected_idx_ >= (int)photos_.size() && selected_idx_ > 0)
            selected_idx_--;

        update_count_label();
        build_photo_list();
    }

    void toggle_camera()
    {
        camera_idx_ = (camera_idx_ == 0) ? 1 : 0;
        update_cam_label();
        printf("[Camera] Switched to camera %d\n", camera_idx_);
    }

    // ==================== event handling ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UICameraPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UICameraPage *self = static_cast<UICameraPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }

    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e))
        {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            handle_key(key);
        }
    }

    void handle_key(uint32_t key)
    {
        int count = (int)photos_.size();
        switch (key)
        {
        case KEY_ENTER:
            capture_photo();
            break;
        case KEY_SPACE:
            toggle_camera();
            break;
        case KEY_D:
            delete_selected();
            break;
        case KEY_UP:
            if (count > 0 && selected_idx_ > 0)
            {
                selected_idx_--;
                build_photo_list();
            }
            break;
        case KEY_DOWN:
            if (count > 0 && selected_idx_ < count - 1)
            {
                selected_idx_++;
                build_photo_list();
            }
            break;
        case KEY_ESC:
            stop_capture();
            if (go_back_home) go_back_home();
            break;
        default:
            break;
        }
    }
};
