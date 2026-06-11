#pragma once
#include "../ui_app_page.hpp"
#include "lvgl/src/draw/lv_image_decoder.h"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

// ============================================================
//  Gallery app  UIGalleryPage
//  Screen resolution: 320 x 170 (top bar20px, ui_APP_Container 320x150)
//
//  Reads .png files from ./dist/gallery/ by default
//  Key mapping (raw evdev keycodes):
//    44 (KEY_Z)     — previous image
//    46 (KEY_C)     — next image
//    57 (KEY_SPACE) — pause/resume slideshow
//    KEY_ESC        — return to the home screen
//  Auto-plays every 3 seconds by default with fade transitions
// ============================================================
class UIGalleryPage : public app_base
{
    static constexpr uint32_t K_PREV    = 44;   // KEY_Z
    static constexpr uint32_t K_NEXT    = 46;   // KEY_C
    static constexpr uint32_t K_PAUSE   = 57;   // KEY_SPACE
    static constexpr uint32_t SLIDE_MS    = 3000;

    static constexpr int DISP_W = 320;
    static constexpr int DISP_H = 150;

    // Image directory (relative to the program working directory)
    static constexpr const char *IMG_DIR      = "./dist/gallery";
    static constexpr const char *LVGL_IMG_DIR = "A:/dist/gallery";
    static constexpr const char *TMP_DIR      = "./dist/gallery/.tmp";
    static constexpr const char *LVGL_TMP_DIR = "A:/dist/gallery/.tmp";

public:
    UIGalleryPage() : app_base()
    {
#ifdef _WIN32
        mkdir(TMP_DIR);
#else
        mkdir(TMP_DIR, 0755);
#endif
        scan_images();
        create_UI();
        event_handler_init();
        if (!images_.empty()) {
            slideshow_timer_ = lv_timer_create(slideshow_timer_cb, SLIDE_MS, this);
        }
    }

    ~UIGalleryPage()
    {
        if (slideshow_timer_) lv_timer_del(slideshow_timer_);
        for (auto &kv : jpg_cache_) {
            unlink(kv.second.c_str());
        }
    }

private:
    std::vector<std::string> images_;
    int current_idx_ = 0;
    bool paused_ = false;
    bool switching_ = false;
    lv_timer_t *slideshow_timer_ = nullptr;
    lv_obj_t *img_obj_ = nullptr;
    lv_obj_t *info_label_ = nullptr;
    lv_obj_t *pause_icon_ = nullptr;

    std::string pending_path_;
    uint32_t pending_zoom_ = 256;
    std::unordered_map<std::string, std::string> jpg_cache_;
    int tmp_counter_ = 0;

    // Scan PNG images in the directory
    void scan_images()
    {
        DIR *dp = opendir(IMG_DIR);
        if (!dp) return;

        struct dirent *ent;
        while ((ent = readdir(dp)) != nullptr) {
            if (ent->d_name[0] == '.') continue;
            std::string name = ent->d_name;
            std::string lower = name;
            for (auto &c : lower) c = (char)std::tolower((unsigned char)c);

            bool is_image = false;
            if (lower.size() > 4 && lower.substr(lower.size() - 4) == ".png") is_image = true;
            else if (lower.size() > 4 && lower.substr(lower.size() - 4) == ".jpg") is_image = true;
            else if (lower.size() > 5 && lower.substr(lower.size() - 5) == ".jpeg") is_image = true;

            if (is_image) {
                images_.push_back(std::string(LVGL_IMG_DIR) + "/" + name);
            }
        }
        closedir(dp);
        std::sort(images_.begin(), images_.end());
    }

    void create_UI()
    {
        // Background
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, DISP_W, DISP_H);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

        // Image display object
        img_obj_ = lv_img_create(bg);
        lv_obj_set_size(img_obj_, DISP_W, DISP_H);
        lv_obj_set_pos(img_obj_, 0, 0);
        lv_obj_set_style_bg_opa(img_obj_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(img_obj_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        // bottom info bar
        info_label_ = lv_label_create(bg);
        lv_obj_set_pos(info_label_, 4, DISP_H - 18);
        lv_obj_set_width(info_label_, DISP_W - 8);
        lv_label_set_long_mode(info_label_, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(info_label_, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(info_label_, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(info_label_, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(info_label_, 160, LV_PART_MAIN | LV_STATE_DEFAULT);

        // pause indicator
        pause_icon_ = lv_label_create(bg);
        lv_label_set_text(pause_icon_, "||");
        lv_obj_set_pos(pause_icon_, DISP_W - 28, 4);
        lv_obj_set_style_text_color(pause_icon_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(pause_icon_, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(pause_icon_, LV_OBJ_FLAG_HIDDEN);

        if (!images_.empty()) {
            show_image(0, false);
        } else {
            lv_label_set_text(info_label_, "No PNG images. Put them into dist/gallery/");
        }
    }

    std::string prepare_image(const std::string &path)
    {
        std::string lower = path;
        for (auto &c : lower) c = (char)std::tolower((unsigned char)c);

        bool is_jpg = (lower.size() > 4 && lower.substr(lower.size() - 4) == ".jpg") ||
                      (lower.size() > 5 && lower.substr(lower.size() - 5) == ".jpeg");

        if (!is_jpg) return path;

        // Check cache
        auto it = jpg_cache_.find(path);
        if (it != jpg_cache_.end() && access(it->second.c_str(), F_OK) == 0) {
            return it->second;
        }

        // Generate temporary file path
        char tmp_name[64];
        snprintf(tmp_name, sizeof(tmp_name), "/gallery_tmp_%d.png", tmp_counter_++);
        std::string tmp_path = std::string(LVGL_TMP_DIR) + tmp_name;
        std::string tmp_sys = std::string(TMP_DIR) + tmp_name;

        // Build the source file system path (remove the A: prefix and keep a relative path)
        std::string src_sys = path;
        if (src_sys.size() > 2 && src_sys.substr(0, 2) == "A:") {
            src_sys = "." + src_sys.substr(2);
        }

        char cmd[512];
        snprintf(cmd, sizeof(cmd), "ffmpeg -y -loglevel error -i \"%s\" -vf scale=iw:ih \"%s\"", src_sys.c_str(), tmp_sys.c_str());

        int ret = system(cmd);
        if (ret == 0 && access(tmp_sys.c_str(), F_OK) == 0) {
            jpg_cache_[path] = tmp_path;
            return tmp_path;
        }

        return "";
    }

    void show_image(int idx, bool animate)
    {
        if (images_.empty() || idx < 0 || idx >= (int)images_.size()) return;
        current_idx_ = idx;

        std::string display_path = prepare_image(images_[idx]);
        if (display_path.empty()) {
            if (info_label_) lv_label_set_text(info_label_, "Image decode failed");
            return;
        }

        // Get original image dimensions and calculate proportional scaling
        lv_image_header_t header;
        lv_result_t res = lv_image_decoder_get_info(display_path.c_str(), &header);
        uint32_t zoom = 256;
        if (res == LV_RESULT_OK && header.w > 0 && header.h > 0) {
            float sx = (float)DISP_W / (float)header.w;
            float sy = (float)DISP_H / (float)header.h;
            float s = (sx < sy) ? sx : sy;
            s *= 0.96f; // leave a small margin
            zoom = (uint32_t)(s * 256.0f);
            if (zoom < 16) zoom = 16;
        }

        if (animate && img_obj_) {
            switching_ = true;
            pending_path_ = display_path;
            pending_zoom_ = zoom;

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, img_obj_);
            lv_anim_set_values(&a, 255, 0);
            lv_anim_set_time(&a, 200);
            lv_anim_set_user_data(&a, this);
            lv_anim_set_custom_exec_cb(&a, [](lv_anim_t *anim, int32_t v){
                UIGalleryPage *self = (UIGalleryPage *)lv_anim_get_user_data(anim);
                if (self && self->img_obj_) lv_obj_set_style_opa(self->img_obj_, v, LV_PART_MAIN | LV_STATE_DEFAULT);
            });
            lv_anim_set_ready_cb(&a, [](lv_anim_t *anim){
                UIGalleryPage *self = (UIGalleryPage *)lv_anim_get_user_data(anim);
                if (self) self->on_fade_out_done();
            });
            lv_anim_start(&a);
        } else {
            lv_img_set_src(img_obj_, display_path.c_str());
            lv_img_set_zoom(img_obj_, zoom);
            lv_obj_set_style_opa(img_obj_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            update_info();
        }
    }

    void on_fade_out_done()
    {
        if (!pending_path_.empty() && img_obj_) {
            lv_img_set_src(img_obj_, pending_path_.c_str());
            lv_img_set_zoom(img_obj_, pending_zoom_);
            pending_path_.clear();

            // Fade in
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, img_obj_);
            lv_anim_set_values(&a, 0, 255);
            lv_anim_set_time(&a, 350);
            lv_anim_set_user_data(&a, this);
            lv_anim_set_custom_exec_cb(&a, [](lv_anim_t *anim, int32_t v){
                UIGalleryPage *self = (UIGalleryPage *)lv_anim_get_user_data(anim);
                if (self && self->img_obj_) lv_obj_set_style_opa(self->img_obj_, v, LV_PART_MAIN | LV_STATE_DEFAULT);
            });
            lv_anim_set_ready_cb(&a, [](lv_anim_t *anim){
                UIGalleryPage *self = (UIGalleryPage *)lv_anim_get_user_data(anim);
                if (self) self->switching_ = false;
            });
            lv_anim_start(&a);
        }
        update_info();
    }

    void update_info()
    {
        if (!info_label_ || images_.empty()) return;
        char buf[128];
        const char *fname = strrchr(images_[current_idx_].c_str(), '/');
        if (!fname) fname = images_[current_idx_].c_str();
        else fname++;
        std::string lower = images_[current_idx_];
        for (auto &c : lower) c = (char)std::tolower((unsigned char)c);
        bool is_jpg = (lower.size() > 4 && lower.substr(lower.size() - 4) == ".jpg") ||
                      (lower.size() > 5 && lower.substr(lower.size() - 5) == ".jpeg");
        snprintf(buf, sizeof(buf), "%d/%d %s%s%s",
                 current_idx_ + 1, (int)images_.size(),
                 paused_ ? "[PAUSED] " : "",
                 is_jpg ? "[JPG] " : "",
                 fname);
        lv_label_set_text(info_label_, buf);
    }

    static void slideshow_timer_cb(lv_timer_t *t)
    {
        UIGalleryPage *self = (UIGalleryPage *)lv_timer_get_user_data(t);
        if (self && !self->paused_ && !self->images_.empty() && !self->switching_) {
            int next = (self->current_idx_ + 1) % (int)self->images_.size();
            self->show_image(next, true);
        }
    }

    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIGalleryPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIGalleryPage *self = static_cast<UIGalleryPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }

    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e)) {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            switch (key) {
            case K_PREV:
                if (!images_.empty() && !switching_) {
                    int prev = (current_idx_ - 1 + (int)images_.size()) % (int)images_.size();
                    show_image(prev, true);
                    reset_timer();
                }
                break;
            case K_NEXT:
                if (!images_.empty() && !switching_) {
                    int next = (current_idx_ + 1) % (int)images_.size();
                    show_image(next, true);
                    reset_timer();
                }
                break;
            case K_PAUSE:
                paused_ = !paused_;
                if (pause_icon_) {
                    if (paused_) lv_obj_clear_flag(pause_icon_, LV_OBJ_FLAG_HIDDEN);
                    else lv_obj_add_flag(pause_icon_, LV_OBJ_FLAG_HIDDEN);
                }
                update_info();
                break;
            case KEY_ESC:
                if (go_back_home) go_back_home();
                break;
            default:
                break;
            }
        }
    }

    void reset_timer()
    {
        if (slideshow_timer_) lv_timer_reset(slideshow_timer_);
    }
};
