#include "hal_lvgl_bsp.h"
#include "cp0_camera_frame_codec.hpp"
#include "cp0_camera_viewport.hpp"
#include "../cp0_camera_api_contract.hpp"
#include "../cp0_callback_contract.hpp"
#include "../cp0_signal_registration.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdint>
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

#if CP0_CAMERA_HAS_LIBCAMERA
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

    void shutdown()
    {
        close_camera();
        std::lock_guard<std::mutex> lock(mutex_);
        status_callback_ = {};
        frame_callback_ = {};
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
    cp0_camera::Viewport viewport_;

    std::atomic<bool> capture_requested_{false};
    std::string pending_capture_path_;
    callback_t pending_capture_callback_;
    int capture_counter_ = 0;
#endif

    void report(callback_t callback, int code, const std::string &data)
    {
        if (!callback) {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = status_callback_;
        }
        cp0::callback::invoke(callback, code, data);
    }

    static std::string nth_arg(const arg_t &arg, size_t n)
    {
        if (arg.size() <= n)
            return "";
        auto it = arg.begin();
        std::advance(it, n);
        return *it;
    }

    void SetCallback(arg_t arg, callback_t callback)
    {
        (void)arg;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            status_callback_ = callback;
        }
        report(callback, 0, "camera callback set\n");
    }

    void SetFrameCallback(arg_t arg, callback_t callback)
    {
        (void)arg;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            frame_callback_ = callback;
        }
        report(callback, 0, "camera frame callback set\n");
    }

    void Open(arg_t arg, callback_t callback)
    {
        const int width = cp0_camera_api::parse_integer_argument(
            nth_arg(arg, 1), cp0_camera_api::DEFAULT_WIDTH);
        const int height = cp0_camera_api::parse_integer_argument(
            nth_arg(arg, 2), cp0_camera_api::DEFAULT_HEIGHT);
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
        const int width = cp0_camera_api::parse_integer_argument(
            nth_arg(arg, 2), cp0_camera_api::DEFAULT_WIDTH);
        const int height = cp0_camera_api::parse_integer_argument(
            nth_arg(arg, 3), cp0_camera_api::DEFAULT_HEIGHT);

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
        std::string payload;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            viewport_.zoom_in();
            payload = viewport_.status_text();
        }
        report(callback, 0, payload);
#else
        report(callback, -10, "camera unavailable: libcamera/jpeg headers not found\n");
#endif
    }

    void ZoomOut(arg_t arg, callback_t callback)
    {
        (void)arg;
#if CP0_CAMERA_HAS_LIBCAMERA
        std::string payload;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            viewport_.zoom_out();
            payload = viewport_.status_text();
        }
        report(callback, 0, payload);
#else
        report(callback, -10, "camera unavailable: libcamera/jpeg headers not found\n");
#endif
    }

    void Pan(arg_t arg, callback_t callback)
    {
#if CP0_CAMERA_HAS_LIBCAMERA
        const int dx = cp0_camera_api::parse_integer_argument(nth_arg(arg, 1), 0);
        const int dy = cp0_camera_api::parse_integer_argument(nth_arg(arg, 2), 0);
        std::string payload;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            viewport_.pan(dx, dy);
            payload = viewport_.status_text();
        }
        report(callback, 0, payload);
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


    static void dma_buf_sync(int fd, uint64_t flags)
    {
        if (fd < 0)
            return;
        struct dma_buf_sync sync = {flags};
        ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
    }


    libcamera::Rectangle crop_rect_locked() const
    {
        const cp0_camera::CropRectangle crop = viewport_.crop({
            scaler_crop_max_.x,
            scaler_crop_max_.y,
            static_cast<int>(scaler_crop_max_.width),
            static_cast<int>(scaler_crop_max_.height),
        });
        return libcamera::Rectangle(crop.x, crop.y, crop.width, crop.height);
    }

    void apply_crop_locked(libcamera::Request *request) const
    {
        if (!request || scaler_crop_max_.width <= 0 || scaler_crop_max_.height <= 0)
            return;
        request->controls().set(libcamera::controls::ScalerCrop, crop_rect_locked());
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
        if (!cp0_camera_frame_codec::is_supported_preview_format(cfg.pixelFormat))
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
        if (capture_requested_.load())
            return -13;
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
        bool should_capture = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            should_capture = capture_requested_.exchange(false);
            if (should_capture)
            {
                save_path = pending_capture_path_;
                callback = pending_capture_callback_;
            }
            auto map_it = mapped_buffers_.find(buffer);
            if (map_it != mapped_buffers_.end())
            {
                const uint8_t *src = reinterpret_cast<const uint8_t *>(map_it->second.addr);
                size_t bytes_used = map_it->second.size;
                const auto &metadata = buffer->metadata();
                if (!metadata.planes().empty() && metadata.planes()[0].bytesused > 0)
                    bytes_used = std::min(bytes_used, static_cast<size_t>(metadata.planes()[0].bytesused));

                dma_buf_sync(map_it->second.fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ);
                if (frame_callback_ && cp0_camera_frame_codec::convert_to_rgb565(
                        src, bytes_used, stream_w_, stream_h_, stream_stride_, stream_format_,
                        frame_rgb565))
                {
                    frame_w = stream_w_;
                    frame_h = stream_h_;
                    frame_callback = frame_callback_;
                }
                if (should_capture && cp0_camera_frame_codec::convert_to_rgb888(
                        src, bytes_used, stream_w_, stream_h_, stream_stride_, stream_format_, rgb))
                {
                    save_w = stream_w_;
                    save_h = stream_h_;
                }
                dma_buf_sync(map_it->second.fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ);
            }
        }

        if (frame_callback && !frame_rgb565.empty())
            cp0::callback::invoke(
                frame_callback, 0,
                cp0_camera_frame_codec::make_frame_payload(frame_rgb565, frame_w, frame_h));

        if (should_capture)
        {
            const bool ok = !save_path.empty() && !rgb.empty() && save_w > 0 && save_h > 0 &&
                            cp0_camera_frame_codec::save_jpeg_rgb888(
                                save_path, rgb.data(), save_w, save_h, 90);
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
        pending_capture_path_.clear();
        pending_capture_callback_ = nullptr;
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
        callback_t cancelled_capture_callback;
        bool cancelled_capture = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelled_capture = capture_requested_.load();
            if (cancelled_capture)
                cancelled_capture_callback = pending_capture_callback_;
            close_camera_locked();
        }
        if (cancelled_capture)
            report(cancelled_capture_callback, -12, "camera capture failed\n");
#endif
    }
};

namespace {

std::shared_ptr<CameraSystem> &camera_system()
{
    static auto camera = std::make_shared<CameraSystem>();
    return camera;
}

cp0::SignalRegistration<decltype(cp0_signal_camera_api)> &camera_registration()
{
    static cp0::SignalRegistration<decltype(cp0_signal_camera_api)> registration;
    return registration;
}

} // namespace

extern "C" void init_camera(void)
{
    const auto camera = camera_system();
    camera_registration().replace(cp0_signal_camera_api,
                         [camera](std::list<std::string> arg,
                                  std::function<void(int, std::string)> callback) {
                             try {
                                 camera->api_call(std::move(arg), callback);
                             } catch (...) {
                                 cp0::callback::invoke(
                                     callback, -1, "camera backend failure\n");
                             }
                         });
}

extern "C" void deinit_camera(void) noexcept
{
    try {
        camera_registration().reset();
    } catch (...) {
    }
    try {
        camera_system()->shutdown();
    } catch (...) {
    }
}
