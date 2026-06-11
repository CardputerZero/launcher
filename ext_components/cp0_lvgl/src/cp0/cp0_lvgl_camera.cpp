#include "hal_lvgl_bsp.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <utility>
#include <vector>

#if __has_include(<jpeglib.h>) && __has_include(<libcamera/camera.h>)
#define CP0_CAMERA_HAS_LIBCAMERA 1
#include <jpeglib.h>
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/control_ids.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/formats.h>
#include <libcamera/pixel_format.h>
#include <libcamera/property_ids.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>
#else
#define CP0_CAMERA_HAS_LIBCAMERA 0
#endif

/*
 * Libcamera API extracted from UICameraPage:
 * - CameraManager start/stop and cameras() enumeration
 * - properties::Model filtering for IMX219
 * - Camera acquire/release, generateConfiguration(), configure(), start()/stop()
 * - StreamRole::Viewfinder with RGB565 request, validated to RGB888/BGR888/XRGB8888/XBGR8888/RGB565
 * - FrameBufferAllocator allocate()/buffers(), mmap() of the first plane fd
 * - Request createRequest(), addBuffer(), queueRequest(), reuse(ReuseBuffers)
 * - requestCompleted signal to consume frame metadata bytesused and re-queue buffers
 */
class CameraSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    CameraSystem() = default;
    ~CameraSystem()
    {
        close_camera();
    }

    void api_call(arg_t arg, callback_t callback)
    {
        if (arg.empty())
        {
            report(callback, -1, "empty camera api\n");
            return;
        }

#define map_fun(name) {#name, std::bind(&CameraSystem::name, this, std::placeholders::_1, std::placeholders::_2)}
        std::list<std::pair<std::string, std::function<void(arg_t, callback_t)>>> cmd_map = {
            map_fun(Open),
            map_fun(Start),
            map_fun(Close),
            map_fun(Stop),
            map_fun(Capture),
            map_fun(Photo),
            map_fun(Status),
            map_fun(SetCallback),
            map_fun(SetFrameCallback),
            map_fun(ZoomIn),
            map_fun(ZoomOut),
            map_fun(Pan),
        };
#undef map_fun

        for (const auto &it : cmd_map)
        {
            if (it.first == arg.front())
            {
                it.second(arg, callback);
                return;
            }
        }

        report(callback, -1, "unknown camera api\n");
    }

private:
    callback_t status_callback_;
    callback_t frame_callback_;
    std::mutex mutex_;

#if CP0_CAMERA_HAS_LIBCAMERA
    struct MappedBuffer
    {
        void *addr = nullptr;
        size_t size = 0;
        int fd = -1;
    };

    std::unique_ptr<libcamera::CameraManager> cm_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraConfiguration> config_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    libcamera::Stream *stream_ = nullptr;
    std::vector<std::unique_ptr<libcamera::Request>> requests_;
    std::unordered_map<const libcamera::FrameBuffer *, MappedBuffer> mapped_buffers_;

    bool streaming_ = false;
    int stream_w_ = 320;
    int stream_h_ = 150;
    int stream_stride_ = 320 * 2;
    libcamera::PixelFormat stream_format_ = libcamera::formats::RGB565;
    libcamera::Rectangle scaler_crop_max_{};
    int zoom_percent_ = 100;
    int view_x_percent_ = 50;
    int view_y_percent_ = 50;

    std::atomic<bool> capture_requested_{false};
    std::string pending_capture_path_;
    callback_t pending_capture_callback_;
    int capture_counter_ = 0;
#endif

    void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
        else if (status_callback_)
            status_callback_(code, data);
    }

    static std::string nth_arg(const arg_t &arg, size_t n)
    {
        if (arg.size() <= n)
            return "";
        auto it = arg.begin();
        std::advance(it, n);
        return *it;
    }

    static int to_int(const std::string &value, int fallback)
    {
        if (value.empty())
            return fallback;
        char *end = nullptr;
        long ret = std::strtol(value.c_str(), &end, 10);
        return end && *end == '\0' ? static_cast<int>(ret) : fallback;
    }

    void SetCallback(arg_t arg, callback_t callback)
    {
        (void)arg;
        status_callback_ = callback;
        report(callback, 0, "camera callback set\n");
    }

    void SetFrameCallback(arg_t arg, callback_t callback)
    {
        (void)arg;
        frame_callback_ = callback;
        report(callback, 0, "camera frame callback set\n");
    }

    void Open(arg_t arg, callback_t callback)
    {
        const int width = to_int(nth_arg(arg, 1), 320);
        const int height = to_int(nth_arg(arg, 2), 150);
        const int ret = open_camera(width, height);
        report(callback, ret, ret == 0 ? "camera open\n" : "camera open failed\n");
    }

    void Start(arg_t arg, callback_t callback)
    {
        Open(arg, callback);
    }

    void Close(arg_t arg, callback_t callback)
    {
        (void)arg;
        close_camera();
        report(callback, 0, "camera close\n");
    }

    void Stop(arg_t arg, callback_t callback)
    {
        Close(arg, callback);
    }

    void Capture(arg_t arg, callback_t callback)
    {
        std::string path = nth_arg(arg, 1);
        const int width = to_int(nth_arg(arg, 2), 320);
        const int height = to_int(nth_arg(arg, 3), 150);

        const int ret = capture(path, width, height, callback);
        if (ret != 0)
            report(callback, ret, "camera capture failed\n");
    }

    void Photo(arg_t arg, callback_t callback)
    {
        Capture(arg, callback);
    }

    void Status(arg_t arg, callback_t callback)
    {
        (void)arg;
#if CP0_CAMERA_HAS_LIBCAMERA
        bool streaming = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            streaming = streaming_;
        }
        report(callback, streaming ? 0 : 1, streaming ? "camera streaming\n" : "camera stopped\n");
#else
        report(callback, -10, "camera unavailable: libcamera/jpeg headers not found\n");
#endif
    }

    void ZoomIn(arg_t arg, callback_t callback)
    {
        (void)arg;
#if CP0_CAMERA_HAS_LIBCAMERA
        std::lock_guard<std::mutex> lock(mutex_);
        zoom_percent_ = zoom_percent_ < 250 ? 250 : 500;
        report(callback, 0, zoom_status_text_locked());
#else
        report(callback, -10, "camera unavailable: libcamera/jpeg headers not found\n");
#endif
    }

    void ZoomOut(arg_t arg, callback_t callback)
    {
        (void)arg;
#if CP0_CAMERA_HAS_LIBCAMERA
        std::lock_guard<std::mutex> lock(mutex_);
        zoom_percent_ = zoom_percent_ > 250 ? 250 : 100;
        if (zoom_percent_ == 100) {
            view_x_percent_ = 50;
            view_y_percent_ = 50;
        }
        report(callback, 0, zoom_status_text_locked());
#else
        report(callback, -10, "camera unavailable: libcamera/jpeg headers not found\n");
#endif
    }

    void Pan(arg_t arg, callback_t callback)
    {
#if CP0_CAMERA_HAS_LIBCAMERA
        const int dx = to_int(nth_arg(arg, 1), 0);
        const int dy = to_int(nth_arg(arg, 2), 0);
        std::lock_guard<std::mutex> lock(mutex_);
        if (zoom_percent_ > 100) {
            view_x_percent_ = std::max(0, std::min(100, view_x_percent_ + dx * 8));
            view_y_percent_ = std::max(0, std::min(100, view_y_percent_ + dy * 8));
        }
        report(callback, 0, zoom_status_text_locked());
#else
        (void)arg;
        report(callback, -10, "camera unavailable: libcamera/jpeg headers not found\n");
#endif
    }

#if CP0_CAMERA_HAS_LIBCAMERA
    static std::string lower_string(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    static void ensure_picture_dir()
    {
        const char *dir = "/home/pi/Pictures";
        struct stat st;
        if (stat(dir, &st) != 0)
            mkdir(dir, 0777);
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
        std::snprintf(path, sizeof(path), "/home/pi/Pictures/IMX219_%s_%03d.jpg", time_buf, ++capture_counter_);
        return std::string(path);
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
        struct dma_buf_sync sync = {flags};
        ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
    }

    static void rgb565_to_rgb888(uint16_t p, uint8_t &r, uint8_t &g, uint8_t &b)
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

    std::string zoom_status_text_locked() const
    {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "ZOOM %d %d %d\n", zoom_percent_, view_x_percent_, view_y_percent_);
        return std::string(buf);
    }

    libcamera::Rectangle crop_rect_locked() const
    {
        if (zoom_percent_ <= 100 || scaler_crop_max_.width <= 0 || scaler_crop_max_.height <= 0)
            return scaler_crop_max_;

        const int max_width = static_cast<int>(scaler_crop_max_.width);
        const int max_height = static_cast<int>(scaler_crop_max_.height);
        int crop_w = std::max(1, max_width * 100 / zoom_percent_);
        int crop_h = std::max(1, max_height * 100 / zoom_percent_);
        int max_x = std::max(0, max_width - crop_w);
        int max_y = std::max(0, max_height - crop_h);
        int x = scaler_crop_max_.x + max_x * std::max(0, std::min(100, view_x_percent_)) / 100;
        int y = scaler_crop_max_.y + max_y * std::max(0, std::min(100, view_y_percent_)) / 100;
        return libcamera::Rectangle(x, y, crop_w, crop_h);
    }

    void apply_crop_locked(libcamera::Request *request) const
    {
        if (!request || scaler_crop_max_.width <= 0 || scaler_crop_max_.height <= 0)
            return;
        request->controls().set(libcamera::controls::ScalerCrop, crop_rect_locked());
    }

    bool convert_to_rgb565(const uint8_t *src, size_t bytes_used, std::vector<uint16_t> &rgb565)
    {
        std::vector<uint8_t> rgb;
        if (!convert_to_rgb888(src, bytes_used, rgb))
            return false;
        rgb565.assign(stream_w_ * stream_h_, 0);
        for (int i = 0; i < stream_w_ * stream_h_; ++i)
            rgb565[i] = rgb888_to_rgb565(rgb[i * 3 + 0], rgb[i * 3 + 1], rgb[i * 3 + 2]);
        return true;
    }

    std::string make_frame_payload(const std::vector<uint16_t> &rgb565, int width, int height)
    {
        std::string payload;
        char header[64];
        const int header_len = std::snprintf(header, sizeof(header), "FRAME %d %d RGB565\n", width, height);
        payload.assign(header, header_len);
        payload.append(reinterpret_cast<const char *>(rgb565.data()), rgb565.size() * sizeof(uint16_t));
        return payload;
    }

    bool convert_to_rgb888(const uint8_t *src, size_t bytes_used, std::vector<uint8_t> &rgb)
    {
        const bool is_rgb888 = stream_format_ == libcamera::formats::RGB888;
        const bool is_bgr888 = stream_format_ == libcamera::formats::BGR888;
        const bool is_xrgb8888 = stream_format_ == libcamera::formats::XRGB8888;
        const bool is_xbgr8888 = stream_format_ == libcamera::formats::XBGR8888;
        const bool is_rgb565 = stream_format_ == libcamera::formats::RGB565;
        const int bytes_per_pixel = is_rgb888 || is_bgr888 ? 3 : is_rgb565 ? 2 : 4;
        const int min_stride = stream_w_ * bytes_per_pixel;
        const int row_stride = stream_stride_ > 0 ? stream_stride_ : min_stride;

        if (row_stride < min_stride)
            return false;

        rgb.assign(stream_w_ * stream_h_ * 3, 0);
        for (int y = 0; y < stream_h_; ++y)
        {
            const size_t row_offset = static_cast<size_t>(y) * row_stride;
            if (row_offset + min_stride > bytes_used)
                return y > 0;

            const uint8_t *line = src + row_offset;
            const int dst_y = stream_h_ - 1 - y;
            for (int x = 0; x < stream_w_; ++x)
            {
                const int dst_x = stream_w_ - 1 - x;
                const int dst = (dst_y * stream_w_ + dst_x) * 3;
                uint8_t r = 0, g = 0, b = 0;

                if (is_rgb888)
                {
                    const uint8_t *p = line + x * 3;
                    r = p[0]; g = p[1]; b = p[2];
                }
                else if (is_bgr888)
                {
                    const uint8_t *p = line + x * 3;
                    b = p[0]; g = p[1]; r = p[2];
                }
                else if (is_xrgb8888)
                {
                    const uint8_t *p = line + x * 4;
                    b = p[0]; g = p[1]; r = p[2];
                }
                else if (is_xbgr8888)
                {
                    const uint8_t *p = line + x * 4;
                    r = p[0]; g = p[1]; b = p[2];
                }
                else if (is_rgb565)
                {
                    const uint8_t *p = line + x * 2;
                    rgb565_to_rgb888(static_cast<uint16_t>(p[0] | (p[1] << 8)), r, g, b);
                }

                rgb[dst + 0] = r;
                rgb[dst + 1] = g;
                rgb[dst + 2] = b;
            }
        }
        return true;
    }

    static bool save_jpeg_rgb888(const std::string &path, const uint8_t *rgb, int width, int height, int quality = 90)
    {
        FILE *fp = std::fopen(path.c_str(), "wb");
        if (!fp)
            return false;

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
            row_pointer[0] = const_cast<JSAMPROW>(&rgb[cinfo.next_scanline * width * 3]);
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);
        std::fclose(fp);
        chmod(path.c_str(), 0666);
        return true;
    }

    int open_camera_impl(int width, int height)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (streaming_)
            return 0;
        if (cm_ || camera_)
            close_camera_locked();

        cm_ = std::make_unique<libcamera::CameraManager>();
        if (cm_->start())
        {
            cm_.reset();
            return -2;
        }

        std::shared_ptr<libcamera::Camera> selected;
        for (const std::shared_ptr<libcamera::Camera> &cam : cm_->cameras())
        {
            std::string model_text;
            const auto &props = cam->properties();
            auto model = props.get(libcamera::properties::Model);
            model_text = model ? *model : cam->id();
            if (lower_string(model_text).find("imx219") != std::string::npos)
            {
                selected = cam;
                break;
            }
        }
        if (!selected)
        {
            close_camera_locked();
            return -3;
        }

        camera_ = selected;
        if (camera_->acquire())
        {
            close_camera_locked();
            return -4;
        }

        config_ = camera_->generateConfiguration({libcamera::StreamRole::Viewfinder});
        if (!config_ || config_->empty())
        {
            close_camera_locked();
            return -5;
        }

        libcamera::StreamConfiguration &cfg = config_->at(0);
        cfg.size.width = width > 0 ? width : 320;
        cfg.size.height = height > 0 ? height : 150;
        cfg.pixelFormat = libcamera::formats::RGB565;
        cfg.bufferCount = 4;

        if (config_->validate() == libcamera::CameraConfiguration::Invalid)
        {
            close_camera_locked();
            return -6;
        }

        if (camera_->configure(config_.get()))
        {
            close_camera_locked();
            return -7;
        }

        cfg = config_->at(0);
        if (!is_supported_preview_format(cfg.pixelFormat))
        {
            close_camera_locked();
            return -8;
        }

        stream_ = cfg.stream();
        stream_w_ = cfg.size.width;
        stream_h_ = cfg.size.height;
        stream_stride_ = cfg.stride;
        stream_format_ = cfg.pixelFormat;
        const auto crop_max = camera_->properties().get(libcamera::properties::ScalerCropMaximum);
        scaler_crop_max_ = crop_max ? *crop_max : libcamera::Rectangle(0, 0, stream_w_, stream_h_);

        allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);
        if (allocator_->allocate(stream_) < 0)
        {
            close_camera_locked();
            return -9;
        }

        const std::vector<std::unique_ptr<libcamera::FrameBuffer>> &buffers = allocator_->buffers(stream_);
        for (const std::unique_ptr<libcamera::FrameBuffer> &buffer : buffers)
        {
            auto planes = buffer->planes();
            if (planes.empty())
                continue;

            const libcamera::FrameBuffer::Plane &plane = planes[0];
            void *memory = mmap(nullptr, plane.length, PROT_READ | PROT_WRITE, MAP_SHARED, plane.fd.get(), plane.offset);
            if (memory == MAP_FAILED)
                continue;

            mapped_buffers_[buffer.get()] = {memory, plane.length, plane.fd.get()};

            std::unique_ptr<libcamera::Request> request = camera_->createRequest();
            if (!request || request->addBuffer(stream_, buffer.get()) < 0)
                continue;

            requests_.push_back(std::move(request));
        }

        if (requests_.empty())
        {
            close_camera_locked();
            return -10;
        }

        camera_->requestCompleted.connect(this, &CameraSystem::request_complete);
        if (camera_->start())
        {
            close_camera_locked();
            return -11;
        }

        for (std::unique_ptr<libcamera::Request> &request : requests_)
        {
            apply_crop_locked(request.get());
            camera_->queueRequest(request.get());
        }

        streaming_ = true;
        return 0;
    }

    int capture_impl(const std::string &path_arg, int width, int height, callback_t callback)
    {
        int ret = open_camera_impl(width, height);
        if (ret != 0)
            return ret;

        std::lock_guard<std::mutex> lock(mutex_);
        pending_capture_path_ = path_arg.empty() ? make_photo_path() : path_arg;
        pending_capture_callback_ = callback;
        capture_requested_.store(true);
        return 0;
    }

    void request_complete(libcamera::Request *request)
    {
        if (request->status() == libcamera::Request::RequestCancelled)
            return;

        libcamera::FrameBuffer *buffer = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = request->buffers().find(stream_);
            if (it == request->buffers().end())
                return;
            buffer = it->second;
        }

        std::string save_path;
        callback_t callback;
        callback_t frame_callback;
        std::vector<uint8_t> rgb;
        std::vector<uint16_t> frame_rgb565;
        int save_w = 0;
        int save_h = 0;
        int frame_w = 0;
        int frame_h = 0;
        bool should_capture = capture_requested_.exchange(false);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto map_it = mapped_buffers_.find(buffer);
            if (map_it != mapped_buffers_.end())
            {
                const uint8_t *src = reinterpret_cast<const uint8_t *>(map_it->second.addr);
                size_t bytes_used = map_it->second.size;
                const auto &metadata = buffer->metadata();
                if (!metadata.planes().empty() && metadata.planes()[0].bytesused > 0)
                    bytes_used = std::min(bytes_used, static_cast<size_t>(metadata.planes()[0].bytesused));

                dma_buf_sync(map_it->second.fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ);
                if (frame_callback_ && convert_to_rgb565(src, bytes_used, frame_rgb565))
                {
                    frame_w = stream_w_;
                    frame_h = stream_h_;
                    frame_callback = frame_callback_;
                }
                if (should_capture && convert_to_rgb888(src, bytes_used, rgb))
                {
                    save_path = pending_capture_path_;
                    callback = pending_capture_callback_;
                    save_w = stream_w_;
                    save_h = stream_h_;
                }
                dma_buf_sync(map_it->second.fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ);
            }
        }

        if (frame_callback && !frame_rgb565.empty())
            frame_callback(0, make_frame_payload(frame_rgb565, frame_w, frame_h));

        if (!save_path.empty())
        {
            bool ok = save_jpeg_rgb888(save_path, rgb.data(), save_w, save_h, 90);
            report(callback, ok ? 0 : -12, ok ? save_path + "\n" : "camera save failed\n");
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (camera_ && streaming_)
            {
                request->reuse(libcamera::Request::ReuseBuffers);
                apply_crop_locked(request);
                camera_->queueRequest(request);
            }
        }
    }

    void close_camera_locked()
    {
        const bool was_streaming = streaming_;
        streaming_ = false;
        capture_requested_.store(false);
        frame_callback_ = nullptr;

        if (camera_)
        {
            camera_->requestCompleted.disconnect(this);
            if (was_streaming)
                camera_->stop();
        }

        requests_.clear();
        for (auto &it : mapped_buffers_)
        {
            if (it.second.addr && it.second.addr != MAP_FAILED)
                munmap(it.second.addr, it.second.size);
        }
        mapped_buffers_.clear();
        allocator_.reset();

        if (camera_)
        {
            camera_->release();
            camera_.reset();
        }
        if (cm_)
        {
            cm_->stop();
            cm_.reset();
        }
        config_.reset();
        stream_ = nullptr;
    }
#endif

    int open_camera(int width, int height)
    {
        (void)width;
        (void)height;
#if CP0_CAMERA_HAS_LIBCAMERA
        return open_camera_impl(width, height);
#else
        return -10;
#endif
    }

    int capture(const std::string &path_arg, int width, int height, callback_t callback)
    {
        (void)path_arg;
        (void)width;
        (void)height;
        (void)callback;
#if CP0_CAMERA_HAS_LIBCAMERA
        return capture_impl(path_arg, width, height, callback);
#else
        return -10;
#endif
    }

    void close_camera()
    {
#if CP0_CAMERA_HAS_LIBCAMERA
        std::lock_guard<std::mutex> lock(mutex_);
        close_camera_locked();
#endif
    }
};

extern "C" void init_camera(void)
{
    std::shared_ptr<CameraSystem> camera = std::make_shared<CameraSystem>();

    cp0_signal_camera_api.append([camera](std::list<std::string> arg, std::function<void(int, std::string)> callback)
                                  { camera->api_call(arg, callback); });
}
