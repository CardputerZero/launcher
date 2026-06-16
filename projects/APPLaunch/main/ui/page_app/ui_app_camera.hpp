/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "../ui_app_page.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <functional>
#include <iomanip>
#include <initializer_list>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#endif

#include "compat/input_keys.h"
#include "hal_lvgl_bsp.h"

namespace camera_app
{
static constexpr int kScreenW = 320;
static constexpr int kContentH = 150;
static constexpr int kPreviewW = 226;
static constexpr int kPreviewH = 150;
static constexpr int kBottomH = 25;
static constexpr uint32_t kBlack = 0x000000;
static constexpr uint32_t kText = 0xFFFFFF;
static constexpr uint32_t kMuted = 0xCAC4CF;
static constexpr uint32_t kPanel = 0x1C1B1E;
static constexpr uint32_t kPanelHigh = 0x2B292D;
static constexpr uint32_t kOutline = 0x49454E;
static constexpr uint32_t kPrimary = 0xCFBCFF;
static constexpr uint32_t kDanger = 0xFFB4AB;

static inline lv_color_t color(uint32_t hex)
{
    return lv_color_hex(hex);
}

static inline void clear_obj(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
}

static inline std::string trim_line(std::string value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
        value.pop_back();
    return value;
}

static inline std::string home_dir()
{
    const char *home = std::getenv("HOME");
    return home && home[0] ? std::string(home) : std::string("/home/pi");
}

static inline int make_dir(const char *path, int mode)
{
#ifdef _WIN32
    (void)mode;
    return _mkdir(path);
#else
    return mkdir(path, mode);
#endif
}

static inline void local_time(const std::time_t *time, std::tm *out)
{
#ifdef _WIN32
    localtime_s(out, time);
#else
    localtime_r(time, out);
#endif
}

static inline void ensure_dir(const std::string &dir)
{
    std::string current;
    if (!dir.empty() && dir[0] == '/')
        current = "/";

    size_t start = current == "/" ? 1 : 0;
    while (start <= dir.size())
    {
        size_t slash = dir.find('/', start);
        std::string part = dir.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!part.empty())
        {
            if (current.size() > 1)
                current += "/";
            current += part;
            struct stat st;
            if (stat(current.c_str(), &st) != 0)
                make_dir(current.c_str(), 0777);
            chmod(current.c_str(), 0777);
        }
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
}

static inline std::string pictures_dir()
{
    std::string dir = home_dir() + "/Pictures/DCIM/Camera";
    ensure_dir(dir);
    return dir;
}

static inline std::string filename_only(const std::string &path)
{
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static inline bool is_image_name(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return (name.size() > 4 && name.substr(name.size() - 4) == ".jpg") ||
           (name.size() > 5 && name.substr(name.size() - 5) == ".jpeg") ||
           (name.size() > 4 && name.substr(name.size() - 4) == ".png");
}

static inline std::string make_photo_path()
{
    std::time_t now = std::time(nullptr);
    std::tm tm_now{};
    local_time(&now, &tm_now);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", &tm_now);
    return pictures_dir() + "/CAM_" + time_buf + ".jpg";
}

static inline std::string format_file_time(const std::string &path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return "Unknown";
    std::tm tm_now{};
    local_time(&st.st_mtime, &tm_now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_now);
    return buf;
}

struct Frame
{
    int width = 0;
    int height = 0;
    std::vector<uint16_t> rgb565;
};

struct ZoomState
{
    int zoom = 100;
    int view_x = 50;
    int view_y = 50;
};

class HardwareCameraClient
{
public:
    using Callback = std::function<void(int, std::string)>;

    void set_status_callback(Callback cb)
    {
        request({"SetCallback"}, std::move(cb), false);
    }

    void set_frame_callback(Callback cb)
    {
        request({"SetFrameCallback"}, std::move(cb), false);
    }

    void start(int w, int h, Callback cb)
    {
        request({"Start", std::to_string(w), std::to_string(h)}, std::move(cb));
    }

    void stop()
    {
        request({"Stop"});
    }

    void capture(const std::string &path, int w, int h, Callback cb)
    {
        request({"Capture", path, std::to_string(w), std::to_string(h)}, std::move(cb));
    }

    void zoom_in(Callback cb)
    {
        request({"ZoomIn"}, std::move(cb));
    }

    void zoom_out(Callback cb)
    {
        request({"ZoomOut"}, std::move(cb));
    }

    void pan(int dx, int dy, Callback cb)
    {
        request({"Pan", std::to_string(dx), std::to_string(dy)}, std::move(cb));
    }

private:
    void request(std::initializer_list<std::string> args, Callback cb = nullptr, bool swallow = true)
    {
        if (!cb && swallow)
            cb = [](int, std::string) {};
        cp0_signal_camera_api(std::list<std::string>(args), std::move(cb));
    }
};

class GalleryStore
{
public:
    void refresh()
    {
        items_.clear();
        DIR *dir = opendir(pictures_dir().c_str());
        if (!dir)
            return;

        struct dirent *entry = nullptr;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_name[0] == '.')
                continue;
            std::string name = entry->d_name;
            if (is_image_name(name))
                items_.push_back(pictures_dir() + "/" + name);
        }
        closedir(dir);
        std::sort(items_.begin(), items_.end());
        if (index_ >= static_cast<int>(items_.size()))
            index_ = items_.empty() ? 0 : static_cast<int>(items_.size()) - 1;
    }

    bool empty() const { return items_.empty(); }
    int count() const { return static_cast<int>(items_.size()); }
    int index() const { return items_.empty() ? 0 : index_ + 1; }
    const std::string &current() const
    {
        static const std::string empty_path;
        return items_.empty() ? empty_path : items_[index_];
    }

    void prev()
    {
        if (items_.empty())
            return;
        index_ = index_ > 0 ? index_ - 1 : static_cast<int>(items_.size()) - 1;
    }

    void next()
    {
        if (items_.empty())
            return;
        index_ = index_ < static_cast<int>(items_.size()) - 1 ? index_ + 1 : 0;
    }

    bool delete_current()
    {
        if (items_.empty())
            return false;
        std::string path = items_[index_];
        if (std::remove(path.c_str()) != 0)
            return false;
        refresh();
        return true;
    }

private:
    std::vector<std::string> items_;
    int index_ = 0;
};
} // namespace camera_app

class UICameraPage : public AppPage
{
public:
    UICameraPage() : AppPage()
    {
        page_title_ = "CAMERA";
        set_page_title(page_title_);
        build_ui();
        bind_keyboard();
        start_camera();
        ui_timer_ = lv_timer_create(&UICameraPage::ui_timer_cb, 33, this);
    }

    ~UICameraPage()
    {
        alive_->store(false);
        camera_.set_frame_callback(nullptr);
        camera_.set_status_callback(nullptr);
        camera_.stop();
        if (ui_timer_)
            lv_timer_delete(ui_timer_);
    }

private:
    enum class Page
    {
        Camera,
        Gallery,
        DeleteConfirm,
        Info,
    };

    struct LvglCall
    {
        void *data = nullptr;
        std::function<void(lv_event_code_t, void *, void *)> cb;
    };

    static void lvgl_event_handler(lv_event_t *e)
    {
        LvglCall *call = static_cast<LvglCall *>(lv_event_get_user_data(e));
        if (!call)
            return;
        if (lv_event_get_code(e) == LV_EVENT_DELETE)
        {
            delete call;
            return;
        }
        if (call->cb)
            call->cb(lv_event_get_code(e), lv_event_get_param(e), call->data);
    }

    static void ui_timer_cb(lv_timer_t *timer)
    {
        UICameraPage *self = static_cast<UICameraPage *>(lv_timer_get_user_data(timer));
        if (self)
            self->poll_ui();
    }

    void lvgl_add_call(lv_obj_t *obj, std::function<void(lv_event_code_t, void *, void *)> cb, void *data = nullptr)
    {
        if (!obj || !cb)
            return;
        LvglCall *call = new LvglCall;
        call->data = data;
        call->cb = std::move(cb);
        lv_obj_add_event_cb(obj, &UICameraPage::lvgl_event_handler, LV_EVENT_ALL, call);
    }

    void build_ui()
    {
        lv_obj_clean(ui_APP_Container);
        lv_obj_set_height(ui_APP_Container, camera_app::kContentH);
        lv_obj_set_y(ui_APP_Container, 20);
        lv_obj_clear_flag(ui_APP_Container, LV_OBJ_FLAG_SCROLLABLE);

        page_camera_ = make_page();
        page_gallery_ = make_page();
        build_camera_page();
        build_gallery_page();
        build_bottom_bar();
        build_delete_dialog();
        build_info_panel();
        show_page(Page::Camera);
    }

    lv_obj_t *make_page()
    {
        lv_obj_t *page = lv_obj_create(ui_APP_Container);
        camera_app::clear_obj(page);
        lv_obj_set_size(page, camera_app::kScreenW, camera_app::kContentH);
        lv_obj_set_pos(page, 0, 0);
        lv_obj_set_style_bg_color(page, camera_app::color(camera_app::kBlack), 0);
        lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
        lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
        return page;
    }

    void build_camera_page()
    {
        preview_box_ = lv_obj_create(page_camera_);
        camera_app::clear_obj(preview_box_);
        lv_obj_set_size(preview_box_, camera_app::kPreviewW, camera_app::kPreviewH);
        lv_obj_set_pos(preview_box_, (camera_app::kScreenW - camera_app::kPreviewW) / 2, 0);
        lv_obj_set_style_bg_color(preview_box_, camera_app::color(0x050505), 0);
        lv_obj_set_style_bg_opa(preview_box_, LV_OPA_COVER, 0);

        init_image_descriptor(camera_app::kPreviewW, camera_app::kPreviewH);
        preview_img_ = lv_img_create(preview_box_);
        lv_obj_set_size(preview_img_, camera_app::kPreviewW, camera_app::kPreviewH);
        lv_img_set_src(preview_img_, &preview_dsc_);
        lv_obj_center(preview_img_);

        status_label_ = lv_label_create(preview_box_);
        lv_obj_set_width(status_label_, camera_app::kPreviewW - 20);
        lv_obj_align(status_label_, LV_ALIGN_TOP_MID, 0, 4);
        lv_label_set_long_mode(status_label_, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(status_label_, camera_app::color(camera_app::kText), 0);
        lv_obj_set_style_bg_color(status_label_, camera_app::color(camera_app::kBlack), 0);
        lv_obj_set_style_bg_opa(status_label_, LV_OPA_50, 0);
        lv_obj_set_style_pad_hor(status_label_, 5, 0);
        lv_label_set_text(status_label_, "Opening camera...");

        zoom_map_ = lv_obj_create(page_camera_);
        camera_app::clear_obj(zoom_map_);
        lv_obj_set_size(zoom_map_, 64, 46);
        lv_obj_align(zoom_map_, LV_ALIGN_TOP_RIGHT, -8, 8);
        lv_obj_set_style_bg_color(zoom_map_, camera_app::color(camera_app::kBlack), 0);
        lv_obj_set_style_bg_opa(zoom_map_, LV_OPA_40, 0);
        lv_obj_set_style_border_color(zoom_map_, camera_app::color(camera_app::kText), 0);
        lv_obj_set_style_border_width(zoom_map_, 1, 0);
        zoom_view_ = lv_obj_create(zoom_map_);
        camera_app::clear_obj(zoom_view_);
        lv_obj_set_size(zoom_view_, 24, 18);
        lv_obj_set_style_bg_color(zoom_view_, camera_app::color(camera_app::kText), 0);
        lv_obj_set_style_bg_opa(zoom_view_, LV_OPA_20, 0);
        lv_obj_set_style_border_color(zoom_view_, camera_app::color(camera_app::kText), 0);
        lv_obj_set_style_border_width(zoom_view_, 1, 0);
        zoom_label_ = lv_label_create(zoom_map_);
        lv_obj_set_style_text_font(zoom_label_, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(zoom_label_, camera_app::color(camera_app::kText), 0);
        lv_obj_align(zoom_label_, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
        update_zoom_ui();

        flash_ = lv_obj_create(page_camera_);
        camera_app::clear_obj(flash_);
        lv_obj_set_size(flash_, camera_app::kScreenW, camera_app::kContentH);
        lv_obj_set_style_bg_color(flash_, camera_app::color(camera_app::kText), 0);
        lv_obj_set_style_bg_opa(flash_, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(flash_, LV_OBJ_FLAG_HIDDEN);
    }

    void build_gallery_page()
    {
        gallery_img_ = lv_img_create(page_gallery_);
        lv_obj_set_size(gallery_img_, camera_app::kScreenW, camera_app::kContentH);
        lv_obj_center(gallery_img_);
        lv_image_set_inner_align(gallery_img_, LV_IMAGE_ALIGN_CONTAIN);

        gallery_empty_ = lv_label_create(page_gallery_);
        lv_obj_set_width(gallery_empty_, camera_app::kScreenW);
        lv_obj_set_style_text_align(gallery_empty_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(gallery_empty_, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(gallery_empty_, camera_app::color(camera_app::kText), 0);
        lv_label_set_text(gallery_empty_, "No photos");
        lv_obj_center(gallery_empty_);

        gallery_top_ = lv_obj_create(page_gallery_);
        camera_app::clear_obj(gallery_top_);
        lv_obj_set_size(gallery_top_, camera_app::kScreenW, 24);
        lv_obj_set_pos(gallery_top_, 0, 0);
        lv_obj_set_style_bg_color(gallery_top_, camera_app::color(camera_app::kBlack), 0);
        lv_obj_set_style_bg_opa(gallery_top_, LV_OPA_50, 0);

        gallery_counter_ = lv_label_create(gallery_top_);
        lv_obj_set_pos(gallery_counter_, 8, 5);
        lv_obj_set_width(gallery_counter_, 60);
        lv_obj_set_style_text_font(gallery_counter_, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(gallery_counter_, camera_app::color(camera_app::kText), 0);

        gallery_title_ = lv_label_create(gallery_top_);
        lv_obj_set_pos(gallery_title_, 72, 5);
        lv_obj_set_width(gallery_title_, 240);
        lv_label_set_long_mode(gallery_title_, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(gallery_title_, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(gallery_title_, camera_app::color(camera_app::kMuted), 0);
    }

    void build_bottom_bar()
    {
        bottom_bar_ = lv_obj_create(root_screen_);
        camera_app::clear_obj(bottom_bar_);
        lv_obj_set_size(bottom_bar_, camera_app::kScreenW, camera_app::kBottomH);
        lv_obj_set_pos(bottom_bar_, 0, 145);
        lv_obj_set_style_bg_color(bottom_bar_, camera_app::color(camera_app::kBlack), 0);
        lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_40, 0);

        for (int i = 0; i < 5; ++i)
        {
            bottom_btn_[i] = lv_btn_create(bottom_bar_);
            lv_obj_remove_style_all(bottom_btn_[i]);
            lv_obj_set_size(bottom_btn_[i], 64, camera_app::kBottomH);
            lv_obj_set_pos(bottom_btn_[i], i * 64, 0);
            lv_obj_add_flag(bottom_btn_[i], LV_OBJ_FLAG_CLICKABLE);
            bottom_label_[i] = lv_label_create(bottom_btn_[i]);
            lv_obj_set_style_text_font(bottom_label_[i], &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(bottom_label_[i], camera_app::color(camera_app::kText), 0);
            lv_obj_center(bottom_label_[i]);
            lvgl_add_call(bottom_btn_[i], [this, i](lv_event_code_t c, void *, void *) {
                if (c == LV_EVENT_CLICKED)
                    dispatch_button(i);
            });
        }
    }

    void build_delete_dialog()
    {
        dialog_scrim_ = lv_obj_create(root_screen_);
        camera_app::clear_obj(dialog_scrim_);
        lv_obj_set_size(dialog_scrim_, camera_app::kScreenW, 170);
        lv_obj_set_pos(dialog_scrim_, 0, 0);
        lv_obj_set_style_bg_color(dialog_scrim_, camera_app::color(camera_app::kBlack), 0);
        lv_obj_set_style_bg_opa(dialog_scrim_, LV_OPA_70, 0);

        lv_obj_t *panel = lv_obj_create(dialog_scrim_);
        camera_app::clear_obj(panel);
        lv_obj_set_size(panel, 236, 112);
        lv_obj_center(panel);
        lv_obj_set_style_bg_color(panel, camera_app::color(camera_app::kPanelHigh), 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(panel, camera_app::color(camera_app::kOutline), 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_set_style_radius(panel, 8, 0);
        lv_obj_set_style_pad_all(panel, 10, 0);

        lv_obj_t *title = lv_label_create(panel);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(title, camera_app::color(camera_app::kText), 0);
        lv_label_set_text(title, "Delete photo?");
        lv_obj_set_pos(title, 10, 9);

        lv_obj_t *body = lv_label_create(panel);
        lv_obj_set_style_text_font(body, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(body, camera_app::color(camera_app::kMuted), 0);
        lv_label_set_text(body, "This cannot be undone.");
        lv_obj_set_pos(body, 10, 35);

        dialog_cancel_ = make_dialog_button(panel, "Cancel", 28, false);
        dialog_confirm_ = make_dialog_button(panel, "Confirm", 132, true);
        lv_obj_add_flag(dialog_scrim_, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *make_dialog_button(lv_obj_t *parent, const char *text, int x, bool danger)
    {
        lv_obj_t *btn = lv_obj_create(parent);
        camera_app::clear_obj(btn);
        lv_obj_set_size(btn, 76, 30);
        lv_obj_set_pos(btn, x, 72);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_t *label = lv_label_create(btn);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
        lv_label_set_text(label, text);
        lv_obj_center(label);
        (void)danger;
        return btn;
    }

    void build_info_panel()
    {
        info_scrim_ = lv_obj_create(root_screen_);
        camera_app::clear_obj(info_scrim_);
        lv_obj_set_size(info_scrim_, camera_app::kScreenW, 170);
        lv_obj_set_pos(info_scrim_, 0, 0);
        lv_obj_set_style_bg_color(info_scrim_, camera_app::color(camera_app::kBlack), 0);
        lv_obj_set_style_bg_opa(info_scrim_, LV_OPA_70, 0);

        lv_obj_t *panel = lv_obj_create(info_scrim_);
        camera_app::clear_obj(panel);
        lv_obj_set_size(panel, 244, 160);
        lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 5);
        lv_obj_set_style_bg_color(panel, camera_app::color(camera_app::kPanelHigh), 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(panel, camera_app::color(camera_app::kOutline), 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_set_style_radius(panel, 8, 0);
        lv_obj_set_style_pad_all(panel, 10, 0);

        lv_obj_t *title = lv_label_create(panel);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(title, camera_app::color(camera_app::kText), 0);
        lv_label_set_text(title, "Photo info");
        lv_obj_set_pos(title, 10, 8);

        info_body_ = lv_label_create(panel);
        lv_obj_set_pos(info_body_, 10, 32);
        lv_obj_set_width(info_body_, 224);
        lv_label_set_long_mode(info_body_, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(info_body_, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(info_body_, camera_app::color(camera_app::kMuted), 0);
        lv_obj_add_flag(info_scrim_, LV_OBJ_FLAG_HIDDEN);
    }

    void init_image_descriptor(int w, int h)
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        display_buf_.assign(w * h, 0);
        std::memset(&preview_dsc_, 0, sizeof(preview_dsc_));
        preview_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
        preview_dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
        preview_dsc_.header.w = w;
        preview_dsc_.header.h = h;
        preview_dsc_.header.stride = w * sizeof(uint16_t);
        preview_dsc_.data_size = display_buf_.size() * sizeof(uint16_t);
        preview_dsc_.data = reinterpret_cast<const uint8_t *>(display_buf_.data());
    }

    void bind_keyboard()
    {
        lvgl_add_call(root_screen_, [this](lv_event_code_t c, void *event_param, void *) {
            if (c != static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD))
                return;
            struct key_item *key = static_cast<struct key_item *>(event_param);
            if (!key || key->key_state != 0)
                return;
            handle_key(key->key_code);
        });
    }

    void start_camera()
    {
        camera_.set_frame_callback(make_callback([this](int code, std::string data) {
            if (code == 0)
                on_frame_payload(std::move(data));
        }));
        camera_.set_status_callback(make_callback([this](int code, std::string data) {
            set_async_status(code == 0 ? camera_app::trim_line(data) : "Camera unavailable");
        }));
        camera_.start(camera_app::kPreviewW, camera_app::kPreviewH, make_callback([this](int code, std::string data) {
            set_async_status(code == 0 ? "Camera ready" : camera_app::trim_line(data));
        }));
    }

    camera_app::HardwareCameraClient::Callback make_callback(camera_app::HardwareCameraClient::Callback cb)
    {
        auto alive = alive_;
        return [alive, cb = std::move(cb)](int code, std::string data) mutable {
            if (!alive->load() || !cb)
                return;
            cb(code, std::move(data));
        };
    }

    void handle_key(uint32_t key)
    {
        if (key == KEY_ESC)
        {
            handle_exit();
            return;
        }
        if (key == KEY_ENTER)
        {
            if (current_page_ == Page::DeleteConfirm)
                confirm_delete();
            else if (current_page_ == Page::Camera)
                capture_photo();
            return;
        }
        if (key == KEY_UP)
        {
            if (current_page_ == Page::Camera)
                pan(0, -1);
            return;
        }
        if (key == KEY_DOWN)
        {
            if (current_page_ == Page::Camera)
                pan(0, 1);
            return;
        }
        if (key == KEY_LEFT)
        {
            if (current_page_ == Page::Gallery)
                gallery_prev();
            else if (current_page_ == Page::DeleteConfirm)
                delete_choice_ = 0, update_delete_choice_ui();
            else if (current_page_ == Page::Camera)
                pan(-1, 0);
            return;
        }
        if (key == KEY_RIGHT)
        {
            if (current_page_ == Page::Gallery)
                gallery_next();
            else if (current_page_ == Page::DeleteConfirm)
                delete_choice_ = 1, update_delete_choice_ui();
            else if (current_page_ == Page::Camera)
                pan(1, 0);
            return;
        }

        int button = -1;
        switch (key)
        {
        case KEY_1: button = 0; break;
        case KEY_2: button = 1; break;
        case KEY_3: button = 2; break;
        case KEY_4: button = 3; break;
        case KEY_5: button = 4; break;
        default: break;
        }
        if (button >= 0)
            dispatch_button(button);
    }

    void dispatch_button(int idx)
    {
        switch (current_page_)
        {
        case Page::Camera:
            if (idx == 0) handle_exit();
            else if (idx == 1) zoom_out();
            else if (idx == 2) capture_photo();
            else if (idx == 3) zoom_in();
            else if (idx == 4) open_gallery();
            break;
        case Page::Gallery:
            if (idx == 0) show_page(Page::Camera);
            else if (idx == 1) gallery_prev();
            else if (idx == 2) show_info();
            else if (idx == 3) gallery_next();
            else if (idx == 4) open_delete_confirm();
            break;
        case Page::DeleteConfirm:
            if (idx == 0) close_delete_confirm();
            else if (idx == 1) delete_choice_ = 0, update_delete_choice_ui();
            else if (idx == 3) delete_choice_ = 1, update_delete_choice_ui();
            else if (idx == 2 || idx == 4) confirm_delete();
            break;
        case Page::Info:
            if (idx == 0 || idx == 2) close_info();
            break;
        }
    }

    void handle_exit()
    {
        if (current_page_ == Page::Info)
        {
            close_info();
            return;
        }
        if (current_page_ == Page::DeleteConfirm)
        {
            close_delete_confirm();
            return;
        }
        if (current_page_ == Page::Gallery)
        {
            show_page(Page::Camera);
            return;
        }
        if (navigate_home)
            navigate_home();
    }

    void show_page(Page page)
    {
        current_page_ = page;
        if (page == Page::Camera)
        {
            lv_obj_clear_flag(page_camera_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(page_gallery_, LV_OBJ_FLAG_HIDDEN);
            set_buttons({"<", "-", "O", "+", "G"});
        }
        else if (page == Page::Gallery)
        {
            lv_obj_add_flag(page_camera_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(page_gallery_, LV_OBJ_FLAG_HIDDEN);
            set_buttons({"<", "<", "i", ">", "X"});
            refresh_gallery_ui();
        }
    }

    void set_buttons(std::initializer_list<const char *> labels)
    {
        int i = 0;
        for (const char *label : labels)
        {
            if (i < 5 && bottom_label_[i])
                lv_label_set_text(bottom_label_[i], label);
            ++i;
        }
    }

    void capture_photo()
    {
        if (capture_pending_)
            return;
        capture_pending_ = true;
        std::string path = camera_app::make_photo_path();
        play_flash();
        set_async_status("Capturing...");
        camera_.capture(path, camera_app::kPreviewW, camera_app::kPreviewH, make_callback([this](int code, std::string data) {
            capture_pending_ = false;
            if (code == 0)
                set_async_status("Saved " + camera_app::trim_line(data));
            else
                set_async_status("Capture failed");
            status_hide_at_ = lv_tick_get() + 2800;
        }));
    }

    void zoom_in()
    {
        zoom_.zoom = zoom_.zoom < 250 ? 250 : 500;
        update_zoom_ui();
        camera_.zoom_in(make_callback([this](int, std::string data) { parse_zoom(data); }));
    }

    void zoom_out()
    {
        zoom_.zoom = zoom_.zoom > 250 ? 250 : 100;
        if (zoom_.zoom == 100)
            zoom_.view_x = zoom_.view_y = 50;
        update_zoom_ui();
        camera_.zoom_out(make_callback([this](int, std::string data) { parse_zoom(data); }));
    }

    void pan(int dx, int dy)
    {
        if (zoom_.zoom <= 100)
            return;
        zoom_.view_x = std::max(0, std::min(100, zoom_.view_x + dx * 8));
        zoom_.view_y = std::max(0, std::min(100, zoom_.view_y + dy * 8));
        update_zoom_ui();
        camera_.pan(dx, dy, make_callback([this](int, std::string data) { parse_zoom(data); }));
    }

    void parse_zoom(const std::string &data)
    {
        int z = 0, x = 0, y = 0;
        if (std::sscanf(data.c_str(), "ZOOM %d %d %d", &z, &x, &y) == 3)
        {
            zoom_.zoom = z;
            zoom_.view_x = x;
            zoom_.view_y = y;
            zoom_dirty_.store(true);
        }
    }

    void update_zoom_ui()
    {
        if (!zoom_map_ || !zoom_view_)
            return;
        if (zoom_.zoom <= 100)
        {
            lv_obj_add_flag(zoom_map_, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        lv_obj_clear_flag(zoom_map_, LV_OBJ_FLAG_HIDDEN);
        int inner_w = 56;
        int inner_h = 38;
        int view_w = std::max(8, inner_w * 100 / zoom_.zoom);
        int view_h = std::max(6, inner_h * 100 / zoom_.zoom);
        int max_x = std::max(0, inner_w - view_w);
        int max_y = std::max(0, inner_h - view_h);
        lv_obj_set_size(zoom_view_, view_w, view_h);
        lv_obj_set_pos(zoom_view_, 4 + max_x * zoom_.view_x / 100, 4 + max_y * zoom_.view_y / 100);
        lv_label_set_text(zoom_label_, zoom_.zoom >= 500 ? "x5" : "x2.5");
    }

    void open_gallery()
    {
        gallery_.refresh();
        show_page(Page::Gallery);
    }

    void gallery_prev()
    {
        gallery_.prev();
        refresh_gallery_ui();
    }

    void gallery_next()
    {
        gallery_.next();
        refresh_gallery_ui();
    }

    void refresh_gallery_ui()
    {
        gallery_.refresh();
        bool empty = gallery_.empty();
        if (empty)
        {
            lv_obj_clear_flag(gallery_empty_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(gallery_img_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(gallery_counter_, "0 / 0");
            lv_label_set_text(gallery_title_, "No photos");
            return;
        }
        lv_obj_add_flag(gallery_empty_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(gallery_img_, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(gallery_img_, gallery_.current().c_str());
        char counter[32];
        std::snprintf(counter, sizeof(counter), "%d / %d", gallery_.index(), gallery_.count());
        lv_label_set_text(gallery_counter_, counter);
        lv_label_set_text(gallery_title_, camera_app::filename_only(gallery_.current()).c_str());
    }

    void open_delete_confirm()
    {
        if (gallery_.empty())
            return;
        current_page_ = Page::DeleteConfirm;
        delete_choice_ = 0;
        update_delete_choice_ui();
        lv_obj_clear_flag(dialog_scrim_, LV_OBJ_FLAG_HIDDEN);
        set_buttons({"<", "<", "OK", ">", "OK"});
    }

    void close_delete_confirm()
    {
        lv_obj_add_flag(dialog_scrim_, LV_OBJ_FLAG_HIDDEN);
        show_page(Page::Gallery);
    }

    void update_delete_choice_ui()
    {
        style_dialog_button(dialog_cancel_, delete_choice_ == 0, false);
        style_dialog_button(dialog_confirm_, delete_choice_ == 1, true);
    }

    void style_dialog_button(lv_obj_t *obj, bool selected, bool danger)
    {
        lv_obj_set_style_bg_color(obj, camera_app::color(selected ? (danger ? 0x93000A : 0x4F378A) : camera_app::kPanel), 0);
        lv_obj_set_style_bg_opa(obj, selected ? LV_OPA_COVER : LV_OPA_60, 0);
        lv_obj_set_style_border_color(obj, camera_app::color(selected ? (danger ? camera_app::kDanger : camera_app::kPrimary) : camera_app::kOutline), 0);
        lv_obj_set_style_text_color(obj, camera_app::color(selected ? camera_app::kText : camera_app::kMuted), 0);
    }

    void confirm_delete()
    {
        if (delete_choice_ == 1)
            gallery_.delete_current();
        close_delete_confirm();
        refresh_gallery_ui();
    }

    void show_info()
    {
        if (gallery_.empty())
            return;
        current_page_ = Page::Info;
        const std::string &path = gallery_.current();
        std::ostringstream out;
        out << "File\n" << camera_app::filename_only(path) << "\n\n"
            << "Path\n" << path << "\n\n"
            << "Created\n" << camera_app::format_file_time(path);
        lv_label_set_text(info_body_, out.str().c_str());
        lv_obj_clear_flag(info_scrim_, LV_OBJ_FLAG_HIDDEN);
        set_buttons({"<", "", "i", "", ""});
    }

    void close_info()
    {
        lv_obj_add_flag(info_scrim_, LV_OBJ_FLAG_HIDDEN);
        show_page(Page::Gallery);
    }

    void on_frame_payload(std::string payload)
    {
        int w = 0, h = 0;
        char fmt[16]{};
        int header_len = 0;
        if (std::sscanf(payload.c_str(), "FRAME %d %d %15s\n%n", &w, &h, fmt, &header_len) != 3)
            return;
        if (w <= 0 || h <= 0 || std::strcmp(fmt, "RGB565") != 0)
            return;
        size_t bytes = static_cast<size_t>(w) * h * sizeof(uint16_t);
        if (payload.size() < static_cast<size_t>(header_len) + bytes)
            return;
        std::vector<uint16_t> pixels(static_cast<size_t>(w) * h);
        std::memcpy(pixels.data(), payload.data() + header_len, bytes);
        std::lock_guard<std::mutex> lock(frame_mutex_);
        pending_frame_.width = w;
        pending_frame_.height = h;
        pending_frame_.rgb565 = std::move(pixels);
        new_frame_.store(true);
    }

    void poll_ui()
    {
        if (new_frame_.exchange(false))
        {
            camera_app::Frame frame;
            {
                std::lock_guard<std::mutex> lock(frame_mutex_);
                frame = pending_frame_;
            }
            if (frame.width > 0 && frame.height > 0 && !frame.rgb565.empty())
            {
                if (frame.width != static_cast<int>(preview_dsc_.header.w) ||
                    frame.height != static_cast<int>(preview_dsc_.header.h))
                {
                    init_image_descriptor(frame.width, frame.height);
                    lv_obj_set_size(preview_box_, frame.width, frame.height);
                    lv_img_set_src(preview_img_, &preview_dsc_);
                }
                display_buf_ = std::move(frame.rgb565);
                preview_dsc_.data = reinterpret_cast<const uint8_t *>(display_buf_.data());
                preview_dsc_.data_size = display_buf_.size() * sizeof(uint16_t);
                lv_img_set_src(preview_img_, &preview_dsc_);
                lv_obj_invalidate(preview_img_);
            }
        }
        if (status_dirty_.exchange(false))
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            lv_label_set_text(status_label_, status_text_.c_str());
            lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (zoom_dirty_.exchange(false))
            update_zoom_ui();
        if (status_hide_at_ && static_cast<int32_t>(lv_tick_get() - status_hide_at_) >= 0)
        {
            status_hide_at_ = 0;
            lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void set_async_status(std::string text)
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_text_ = std::move(text);
        status_dirty_.store(true);
    }

    static void flash_opa_anim_cb(void *obj, int32_t value)
    {
        lv_obj_set_style_bg_opa(static_cast<lv_obj_t *>(obj), static_cast<lv_opa_t>(value), 0);
    }

    static void flash_done_cb(lv_anim_t *anim)
    {
        lv_obj_t *obj = static_cast<lv_obj_t *>(lv_anim_get_user_data(anim));
        if (obj)
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }

    void play_flash()
    {
        if (!flash_)
            return;
        lv_obj_clear_flag(flash_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(flash_);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, flash_);
        lv_anim_set_values(&a, LV_OPA_80, LV_OPA_TRANSP);
        lv_anim_set_time(&a, 320);
        lv_anim_set_exec_cb(&a, flash_opa_anim_cb);
        lv_anim_set_user_data(&a, flash_);
        lv_anim_set_ready_cb(&a, flash_done_cb);
        lv_anim_start(&a);
    }

    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    camera_app::HardwareCameraClient camera_;
    camera_app::GalleryStore gallery_;
    Page current_page_ = Page::Camera;
    camera_app::ZoomState zoom_;

    lv_obj_t *page_camera_ = nullptr;
    lv_obj_t *page_gallery_ = nullptr;
    lv_obj_t *preview_box_ = nullptr;
    lv_obj_t *preview_img_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *zoom_map_ = nullptr;
    lv_obj_t *zoom_view_ = nullptr;
    lv_obj_t *zoom_label_ = nullptr;
    lv_obj_t *flash_ = nullptr;
    lv_obj_t *gallery_img_ = nullptr;
    lv_obj_t *gallery_empty_ = nullptr;
    lv_obj_t *gallery_top_ = nullptr;
    lv_obj_t *gallery_counter_ = nullptr;
    lv_obj_t *gallery_title_ = nullptr;
    lv_obj_t *bottom_bar_ = nullptr;
    lv_obj_t *bottom_btn_[5]{};
    lv_obj_t *bottom_label_[5]{};
    lv_obj_t *dialog_scrim_ = nullptr;
    lv_obj_t *dialog_cancel_ = nullptr;
    lv_obj_t *dialog_confirm_ = nullptr;
    lv_obj_t *info_scrim_ = nullptr;
    lv_obj_t *info_body_ = nullptr;
    lv_timer_t *ui_timer_ = nullptr;

    lv_image_dsc_t preview_dsc_{};
    std::vector<uint16_t> display_buf_;
    camera_app::Frame pending_frame_;
    std::mutex frame_mutex_;
    std::atomic<bool> new_frame_{false};
    std::atomic<bool> status_dirty_{false};
    std::atomic<bool> zoom_dirty_{false};
    std::mutex status_mutex_;
    std::string status_text_;
    uint32_t status_hide_at_ = 0;
    bool capture_pending_ = false;
    int delete_choice_ = 0;
};
