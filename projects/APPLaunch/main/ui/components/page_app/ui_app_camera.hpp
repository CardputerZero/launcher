#pragma once

#include "../ui_app_page.hpp"

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <cctype>

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/dma-buf.h>

#include <jpeglib.h>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/formats.h>
#include <libcamera/pixel_format.h>
#include <libcamera/property_ids.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>

#include "compat/input_keys.h"

// ============================================================
//  Raspberry Pi MIPI IMX219 Camera Page
//
//  - Direct libcamera C++ API video stream
//  - LVGL fullscreen preview
//  - ENTER: capture current stream frame
//  - Save JPEG to /home/pi/Pictures
//  - chmod 666 after save
// ============================================================

class UICameraPage : public app_base
{
public:
    UICameraPage() : app_base()
    {
        set_page_title("CAMERA");

        ensure_picture_dir();

        create_UI();
        event_handler_init();

        open_imx219_camera();

        frame_timer_ = lv_timer_create(frame_timer_cb, 33, this);
    }

    ~UICameraPage()
    {
        if (frame_timer_)
        {
            lv_timer_delete(frame_timer_);
            frame_timer_ = nullptr;
        }

        close_camera();
    }

private:
    // ==================== constants ====================

    static constexpr int PREVIEW_W = 320;
    static constexpr int PREVIEW_H = 150;

    // ==================== LVGL ====================

    std::unordered_map<std::string, lv_obj_t *> ui_obj_;

    lv_image_dsc_t img_dsc_{};
    lv_image_dsc_t snapshot_img_dsc_{};

    std::vector<uint16_t> display_buf_;
    std::vector<uint16_t> pending_lv_buf_;
    std::vector<uint16_t> snapshot_lv_buf_;

    std::vector<uint8_t> pending_rgb_buf_;
    std::vector<uint8_t> snapshot_rgb_buf_;

    std::mutex frame_mutex_;

    bool new_frame_ = false;
    bool snapshot_ready_ = false;
    bool snapshot_review_active_ = false;
    uint32_t snapshot_review_until_ = 0;

    lv_timer_t *frame_timer_ = nullptr;

    // ==================== camera ====================

    std::unique_ptr<libcamera::CameraManager> cm_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraConfiguration> config_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;

    libcamera::Stream *stream_ = nullptr;

    std::vector<std::unique_ptr<libcamera::Request>> requests_;

    struct MappedBuffer
    {
        void  *addr = nullptr;
        size_t size = 0;
        int fd = -1;
    };

    std::unordered_map<const libcamera::FrameBuffer *, MappedBuffer> mapped_buffers_;

    bool camera_found_ = false;
    bool streaming_ = false;

    int stream_w_ = PREVIEW_W;
    int stream_h_ = PREVIEW_H;
    int stream_stride_ = PREVIEW_W * 2;
    libcamera::PixelFormat stream_format_ = libcamera::formats::RGB565;

    std::atomic<bool> capture_requested_{false};

    int capture_counter_ = 0;
    std::string last_file_;

private:
    // ============================================================
    //  UI
    // ============================================================

    void create_UI()
    {
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, 320, 150);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        init_image_buffer(PREVIEW_W, PREVIEW_H);

        lv_obj_t *img = lv_img_create(bg);
        lv_obj_set_pos(img, 0, 0);
        lv_img_set_src(img, &img_dsc_);
        ui_obj_["img"] = img;

        // top overlay
        lv_obj_t *top = lv_obj_create(bg);
        lv_obj_set_size(top, 320, 24);
        lv_obj_set_pos(top, 0, 0);
        lv_obj_set_style_radius(top, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(top, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(top, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(top, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(top, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_title = lv_label_create(top);
        lv_label_set_text(lbl_title, "IMX219 Stream");
        lv_obj_set_pos(lbl_title, 6, 4);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_status = lv_label_create(top);
        lv_label_set_text(lbl_status, "Opening...");
        lv_obj_set_pos(lbl_status, 220, 4);
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xF1C40F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_status"] = lbl_status;

        lv_obj_t *photo_frame = lv_obj_create(bg);
        lv_obj_set_size(photo_frame, 186, 118);
        lv_obj_center(photo_frame);
        lv_obj_set_style_radius(photo_frame, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(photo_frame, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(photo_frame, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(photo_frame, lv_color_hex(0xF7F7F7), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(photo_frame, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(photo_frame, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(photo_frame, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_transform_rotation(photo_frame, -60, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(photo_frame, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(photo_frame, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(photo_frame, LV_OBJ_FLAG_HIDDEN);
        ui_obj_["photo_frame"] = photo_frame;

        lv_obj_t *photo_img = lv_img_create(photo_frame);
        lv_obj_set_size(photo_img, 174, 78);
        lv_obj_align(photo_img, LV_ALIGN_TOP_MID, 0, 0);
        lv_image_set_inner_align(photo_img, LV_IMAGE_ALIGN_COVER);
        lv_img_set_src(photo_img, &snapshot_img_dsc_);
        ui_obj_["photo_img"] = photo_img;

        lv_obj_t *lbl_save_info = lv_label_create(photo_frame);
        lv_label_set_text(lbl_save_info, "");
        lv_obj_set_width(lbl_save_info, 174);
        lv_obj_align(lbl_save_info, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_label_set_long_mode(lbl_save_info, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(lbl_save_info, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_save_info, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["lbl_info"] = lbl_save_info;
    }

    void init_image_buffer(int w, int h)
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);

        stream_w_ = w;
        stream_h_ = h;

        display_buf_.assign(w * h, 0);
        pending_lv_buf_.assign(w * h, 0);
        snapshot_lv_buf_.assign(w * h, 0);
        pending_rgb_buf_.assign(w * h * 3, 0);

        memset(&img_dsc_, 0, sizeof(img_dsc_));

        // LVGL 9.x image descriptor
        img_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
        img_dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
        img_dsc_.header.flags = 0;
        img_dsc_.header.w = w;
        img_dsc_.header.h = h;
        img_dsc_.header.stride = w * sizeof(uint16_t);

        img_dsc_.data_size = display_buf_.size() * sizeof(uint16_t);
        img_dsc_.data = reinterpret_cast<const uint8_t *>(display_buf_.data());

        snapshot_img_dsc_ = img_dsc_;
        snapshot_img_dsc_.data_size = snapshot_lv_buf_.size() * sizeof(uint16_t);
        snapshot_img_dsc_.data = reinterpret_cast<const uint8_t *>(snapshot_lv_buf_.data());
    }

    // ============================================================
    //  directory / filename / jpeg
    // ============================================================

    void ensure_picture_dir()
    {
        const char *dir = "/home/pi/Pictures";

        struct stat st;
        if (stat(dir, &st) != 0)
        {
            mkdir(dir, 0777);
        }

        chmod(dir, 0777);
    }

    std::string make_photo_path()
    {
        ensure_picture_dir();

        time_t now = time(nullptr);

        struct tm tm_now;
        localtime_r(&now, &tm_now);

        char time_buf[64];
        strftime(time_buf, sizeof(time_buf), "%Y%m%d_%H%M%S", &tm_now);

        char path[256];
        snprintf(path,
                 sizeof(path),
                 "/home/pi/Pictures/IMX219_%s_%03d.jpg",
                 time_buf,
                 ++capture_counter_);

        return std::string(path);
    }

    bool save_jpeg_rgb888(const std::string &path,
                          const uint8_t *rgb,
                          int width,
                          int height,
                          int quality = 90)
    {
        FILE *fp = fopen(path.c_str(), "wb");
        if (!fp)
        {
            printf("[Camera] Failed to open jpeg file: %s\n", path.c_str());
            return false;
        }

        jpeg_compress_struct cinfo;
        jpeg_error_mgr jerr;

        cinfo.err = jpeg_std_error(&jerr);

        jpeg_create_compress(&cinfo);
        jpeg_stdio_dest(&cinfo, fp);

        cinfo.image_width = width;
        cinfo.image_height = height;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;

        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, quality, TRUE);

        jpeg_start_compress(&cinfo, TRUE);

        while (cinfo.next_scanline < cinfo.image_height)
        {
            JSAMPROW row_pointer[1];
            row_pointer[0] = const_cast<JSAMPROW>(
                &rgb[cinfo.next_scanline * width * 3]);

            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);

        fclose(fp);

        chmod(path.c_str(), 0666);

        return true;
    }

    // ============================================================
    //  libcamera open / close
    // ============================================================

    static std::string lower_string(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    void open_imx219_camera()
    {
        cm_ = std::make_unique<libcamera::CameraManager>();

        if (cm_->start())
        {
            set_status("CM failed", false);
            printf("[Camera] CameraManager start failed\n");
            return;
        }

        std::shared_ptr<libcamera::Camera> selected;

        for (const std::shared_ptr<libcamera::Camera> &cam : cm_->cameras())
        {
            std::string model_text;

            const auto &props = cam->properties();
            auto model = props.get(libcamera::properties::Model);

            if (model)
                model_text = *model;
            else
                model_text = cam->id();

            std::string lower = lower_string(model_text);

            printf("[Camera] Found camera: %s\n", model_text.c_str());

            if (lower.find("imx219") != std::string::npos)
            {
                selected = cam;
                break;
            }
        }

        if (!selected)
        {
            camera_found_ = false;
            set_status("No IMX219", false);
            printf("[Camera] IMX219 not found\n");
            return;
        }

        camera_ = selected;
        camera_found_ = true;

        if (camera_->acquire())
        {
            set_status("Acquire fail", false);
            printf("[Camera] Camera acquire failed\n");
            camera_.reset();
            return;
        }

        config_ = camera_->generateConfiguration({ libcamera::StreamRole::Viewfinder });

        if (!config_ || config_->empty())
        {
            set_status("Config fail", false);
            printf("[Camera] generateConfiguration failed\n");
            camera_->release();
            camera_.reset();
            return;
        }

        libcamera::StreamConfiguration &cfg = config_->at(0);

        cfg.size.width = PREVIEW_W;
        cfg.size.height = PREVIEW_H;
        cfg.pixelFormat = libcamera::formats::RGB565;
        cfg.bufferCount = 4;

        libcamera::CameraConfiguration::Status validation = config_->validate();

        if (validation == libcamera::CameraConfiguration::Invalid)
        {
            set_status("Invalid cfg", false);
            printf("[Camera] Invalid camera configuration\n");
            camera_->release();
            camera_.reset();
            return;
        }

        if (camera_->configure(config_.get()))
        {
            set_status("Cfg failed", false);
            printf("[Camera] camera configure failed\n");
            camera_->release();
            camera_.reset();
            return;
        }

        cfg = config_->at(0);

        if (!is_supported_preview_format(cfg.pixelFormat))
        {
            set_status("Fmt fail", false);
            printf("[Camera] Unsupported preview format=%s\n",
                   cfg.pixelFormat.toString().c_str());

            camera_->release();
            camera_.reset();
            return;
        }

        stream_ = cfg.stream();

        stream_w_ = cfg.size.width;
        stream_h_ = cfg.size.height;
        stream_stride_ = cfg.stride;
        stream_format_ = cfg.pixelFormat;

        printf("[Camera] Stream: %dx%d stride=%d format=%s\n",
               stream_w_,
               stream_h_,
               stream_stride_,
               cfg.pixelFormat.toString().c_str());

        init_image_buffer(stream_w_, stream_h_);

        if (ui_obj_.count("img"))
            lv_img_set_src(ui_obj_["img"], &img_dsc_);

        allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);

        if (allocator_->allocate(stream_) < 0)
        {
            set_status("Alloc fail", false);
            printf("[Camera] FrameBuffer allocation failed\n");
            camera_->release();
            camera_.reset();
            return;
        }

        const std::vector<std::unique_ptr<libcamera::FrameBuffer>> &buffers =
            allocator_->buffers(stream_);

        for (const std::unique_ptr<libcamera::FrameBuffer> &buffer : buffers)
        {
            auto planes = buffer->planes();

            if (planes.empty())
                continue;

            const libcamera::FrameBuffer::Plane &plane = planes[0];

            void *memory = mmap(nullptr,
                                plane.length,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                plane.fd.get(),
                                plane.offset);

            if (memory == MAP_FAILED)
            {
                printf("[Camera] mmap failed\n");
                continue;
            }

            mapped_buffers_[buffer.get()] = { memory, plane.length, plane.fd.get() };

            std::unique_ptr<libcamera::Request> request =
                camera_->createRequest();

            if (!request)
            {
                printf("[Camera] createRequest failed\n");
                continue;
            }

            if (request->addBuffer(stream_, buffer.get()) < 0)
            {
                printf("[Camera] addBuffer failed\n");
                continue;
            }

            requests_.push_back(std::move(request));
        }

        if (requests_.empty())
        {
            set_status("Req fail", false);
            printf("[Camera] No camera request created\n");
            close_camera();
            return;
        }

        camera_->requestCompleted.connect(this, &UICameraPage::request_complete);

        if (camera_->start())
        {
            set_status("Start fail", false);
            printf("[Camera] camera start failed\n");
            close_camera();
            return;
        }

        for (std::unique_ptr<libcamera::Request> &request : requests_)
        {
            camera_->queueRequest(request.get());
        }

        streaming_ = true;

        set_status("STREAMING", true);

        printf("[Camera] IMX219 stream started\n");
    }

    void close_camera()
    {
        streaming_ = false;

        if (camera_)
        {
            camera_->requestCompleted.disconnect(this);

            camera_->stop();

            requests_.clear();

            for (auto &it : mapped_buffers_)
            {
                if (it.second.addr && it.second.addr != MAP_FAILED)
                    munmap(it.second.addr, it.second.size);
            }

            mapped_buffers_.clear();

            allocator_.reset();

            camera_->release();
            camera_.reset();
        }

        if (cm_)
        {
            cm_->stop();
            cm_.reset();
        }
    }

    void set_status(const char *text, bool ok)
    {
        if (!ui_obj_.count("lbl_status"))
            return;

        lv_label_set_text(ui_obj_["lbl_status"], text);

        lv_obj_set_style_text_color(ui_obj_["lbl_status"],
                                    ok ? lv_color_hex(0x2ECC71)
                                       : lv_color_hex(0xE74C3C),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // ============================================================
    //  frame callback
    // ============================================================

    void request_complete(libcamera::Request *request)
    {
        if (request->status() == libcamera::Request::RequestCancelled)
            return;

        auto it = request->buffers().find(stream_);
        if (it == request->buffers().end())
            return;

        libcamera::FrameBuffer *buffer = it->second;

        auto map_it = mapped_buffers_.find(buffer);
        if (map_it == mapped_buffers_.end())
            return;

        const uint8_t *src =
            reinterpret_cast<const uint8_t *>(map_it->second.addr);

        size_t bytes_used = map_it->second.size;
        const auto &metadata = buffer->metadata();
        if (!metadata.planes().empty() && metadata.planes()[0].bytesused > 0)
            bytes_used = std::min(bytes_used,
                                  static_cast<size_t>(metadata.planes()[0].bytesused));

        dma_buf_sync(map_it->second.fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ);
        convert_preview_frame(src, bytes_used);
        dma_buf_sync(map_it->second.fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ);

        request->reuse(libcamera::Request::ReuseBuffers);
        camera_->queueRequest(request);
    }

    static bool is_supported_preview_format(const libcamera::PixelFormat &format)
    {
        return format == libcamera::formats::RGB888 ||
               format == libcamera::formats::BGR888 ||
               format == libcamera::formats::XRGB8888 ||
               format == libcamera::formats::XBGR8888 ||
               format == libcamera::formats::RGB565;
    }

    static void dma_buf_sync(int fd, uint64_t flags)
    {
        if (fd < 0)
            return;

        struct dma_buf_sync sync = { flags };
        ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
    }

    static void rgb565_to_rgb888(uint16_t p,
                                 uint8_t &r,
                                 uint8_t &g,
                                 uint8_t &b)
    {
        r = ((p >> 11) & 0x1F) << 3;
        g = ((p >> 5) & 0x3F) << 2;
        b = (p & 0x1F) << 3;
        r |= r >> 5;
        g |= g >> 6;
        b |= b >> 5;
    }

    static uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
    {
        return static_cast<uint16_t>(((r & 0xF8) << 8) |
                                     ((g & 0xFC) << 3) |
                                     (b >> 3));
    }

    void store_preview_pixel(int idx, uint8_t r, uint8_t g, uint8_t b)
    {
        pending_lv_buf_[idx] = rgb888_to_rgb565(r, g, b);

        int rgb_idx = idx * 3;
        pending_rgb_buf_[rgb_idx + 0] = r;
        pending_rgb_buf_[rgb_idx + 1] = g;
        pending_rgb_buf_[rgb_idx + 2] = b;
    }

    void store_preview_pixel_rgb565(int idx, uint16_t rgb565)
    {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;

        pending_lv_buf_[idx] = rgb565;
        rgb565_to_rgb888(rgb565, r, g, b);

        int rgb_idx = idx * 3;
        pending_rgb_buf_[rgb_idx + 0] = r;
        pending_rgb_buf_[rgb_idx + 1] = g;
        pending_rgb_buf_[rgb_idx + 2] = b;
    }

    void convert_preview_frame(const uint8_t *src, size_t bytes_used)
    {
        std::lock_guard<std::mutex> lock(frame_mutex_);

        if ((int)pending_lv_buf_.size() != stream_w_ * stream_h_)
            return;

        if ((int)pending_rgb_buf_.size() != stream_w_ * stream_h_ * 3)
            return;

        if ((int)snapshot_lv_buf_.size() != stream_w_ * stream_h_)
            return;

        const bool is_rgb888 = stream_format_ == libcamera::formats::RGB888;
        const bool is_bgr888 = stream_format_ == libcamera::formats::BGR888;
        const bool is_xrgb8888 = stream_format_ == libcamera::formats::XRGB8888;
        const bool is_xbgr8888 = stream_format_ == libcamera::formats::XBGR8888;
        const bool is_rgb565 = stream_format_ == libcamera::formats::RGB565;

        const int bytes_per_pixel = is_rgb888 || is_bgr888 ? 3 :
                                    is_rgb565 ? 2 : 4;
        const int min_stride = stream_w_ * bytes_per_pixel;
        const int row_stride = stream_stride_ > 0 ? stream_stride_ : min_stride;

        if (row_stride < min_stride)
            return;

        for (int y = 0; y < stream_h_; ++y)
        {
            const size_t row_offset = static_cast<size_t>(y) * row_stride;
            if (row_offset + min_stride > bytes_used)
                break;

            const uint8_t *line = src + row_offset;
            const int dst_y = stream_h_ - 1 - y;

            for (int x = 0; x < stream_w_; ++x)
            {
                const int dst_x = stream_w_ - 1 - x;
                uint8_t r = 0;
                uint8_t g = 0;
                uint8_t b = 0;

                if (is_rgb888)
                {
                    const uint8_t *p = line + x * 3;
                    r = p[0];
                    g = p[1];
                    b = p[2];
                }
                else if (is_bgr888)
                {
                    const uint8_t *p = line + x * 3;
                    b = p[0];
                    g = p[1];
                    r = p[2];
                }
                else if (is_xrgb8888)
                {
                    const uint8_t *p = line + x * 4;
                    b = p[0];
                    g = p[1];
                    r = p[2];
                }
                else if (is_xbgr8888)
                {
                    const uint8_t *p = line + x * 4;
                    r = p[0];
                    g = p[1];
                    b = p[2];
                }
                else if (is_rgb565)
                {
                    const uint8_t *p = line + x * 2;
                    int idx = dst_y * stream_w_ + dst_x;
                    store_preview_pixel_rgb565(idx,
                                               static_cast<uint16_t>(p[0] | (p[1] << 8)));
                    continue;
                }

                int idx = dst_y * stream_w_ + dst_x;
                store_preview_pixel(idx, r, g, b);
            }
        }

        if (capture_requested_.exchange(false))
        {
            snapshot_rgb_buf_ = pending_rgb_buf_;
            snapshot_lv_buf_ = pending_lv_buf_;
            snapshot_ready_ = true;
        }

        new_frame_ = true;
    }

    // ============================================================
    //  LVGL timer: update display and save snapshot
    // ============================================================

    static void frame_timer_cb(lv_timer_t *t)
    {
        UICameraPage *self =
            static_cast<UICameraPage *>(lv_timer_get_user_data(t));

        if (self)
            self->on_frame_timer();
    }

    void on_frame_timer()
    {
        bool need_update = false;

        bool need_save = false;
        std::vector<uint8_t> save_rgb;
        int save_w = 0;
        int save_h = 0;
        std::string save_path;

        {
            std::lock_guard<std::mutex> lock(frame_mutex_);

            if (new_frame_)
            {
                std::swap(display_buf_, pending_lv_buf_);

                img_dsc_.data =
                    reinterpret_cast<const uint8_t *>(display_buf_.data());

                new_frame_ = false;
                need_update = true;
            }

            if (snapshot_ready_)
            {
                save_rgb = snapshot_rgb_buf_;
                save_w = stream_w_;
                save_h = stream_h_;
                save_path = last_file_;

                snapshot_ready_ = false;
                need_save = true;
            }
        }

        if (need_update)
        {
            lv_img_set_src(ui_obj_["img"], &img_dsc_);
            lv_obj_invalidate(ui_obj_["img"]);
        }

        if (need_save)
        {
            show_snapshot_review();

            if (save_jpeg_rgb888(save_path,
                                 save_rgb.data(),
                                 save_w,
                                 save_h,
                                 90))
            {
                char buf[256];

                snprintf(buf,
                         sizeof(buf),
                         "Saved: %s  chmod 666",
                         save_path.c_str());

                lv_label_set_text(ui_obj_["lbl_info"], buf);

                printf("[Camera] Saved frame: %s\n", save_path.c_str());
            }
            else
            {
                lv_label_set_text(ui_obj_["lbl_info"], "Save failed");
            }
        }

        update_snapshot_review_timeout();
    }

    void show_snapshot_review()
    {
        if (!ui_obj_.count("photo_frame") || !ui_obj_.count("photo_img"))
            return;

        snapshot_img_dsc_.data = reinterpret_cast<const uint8_t *>(snapshot_lv_buf_.data());
        lv_img_set_src(ui_obj_["photo_img"], &snapshot_img_dsc_);
        lv_obj_invalidate(ui_obj_["photo_img"]);

        lv_obj_remove_flag(ui_obj_["photo_frame"], LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_obj_["photo_frame"]);

        snapshot_review_active_ = true;
        snapshot_review_until_ = lv_tick_get() + 3000;
    }

    void update_snapshot_review_timeout()
    {
        if (!snapshot_review_active_)
            return;

        if (static_cast<int32_t>(lv_tick_get() - snapshot_review_until_) < 0)
            return;

        if (ui_obj_.count("photo_frame"))
            lv_obj_add_flag(ui_obj_["photo_frame"], LV_OBJ_FLAG_HIDDEN);

        snapshot_review_active_ = false;
    }

    // ============================================================
    //  capture
    // ============================================================

    void capture_from_stream()
    {
        if (!camera_found_ || !streaming_)
        {
            set_status("Not ready", false);
            return;
        }

        last_file_ = make_photo_path();

        capture_requested_ = true;
    }

    // ============================================================
    //  event
    // ============================================================

    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root,
                            UICameraPage::static_lvgl_handler,
                            LV_EVENT_ALL,
                            this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UICameraPage *self =
            static_cast<UICameraPage *>(lv_event_get_user_data(e));

        if (self)
            self->event_handler(e);
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
        switch (key)
        {
        case KEY_ENTER:
            capture_from_stream();
            break;

        case KEY_ESC:
            close_camera();

            if (go_back_home)
                go_back_home();

            break;

        default:
            break;
        }
    }
};
