#include "cp0_lvgl.h"
#include "cp0_lvgl_app.h"
#include "../cp0_init_once.hpp"
#include "../cp0_rpc_runtime_contract.hpp"

#ifndef CP0_LVGL_USE_ZMQ_RPC
#define CP0_LVGL_USE_ZMQ_RPC 0
#endif

#if CP0_LVGL_USE_ZMQ_RPC

#include "cp0_framebuffer_codec.hpp"
#include "keyboard_input.h"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <future>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <zmq.hpp>

namespace {

constexpr const char *kDefaultRpcEndpoint = "tcp://127.0.0.1:5557";
constexpr const char *kDefaultKeyEndpoint = "tcp://127.0.0.1:5558";
constexpr uint32_t kKeyMagic = 0x4350304b;

struct RpcKeyEvent {
    uint32_t magic;
    uint32_t sender_pid;
    uint32_t code;
    uint32_t state;
    uint32_t mods;
};

const char *env_or_default(const char *name, const char *fallback)
{
    const char *value = std::getenv(name);
    return value && value[0] ? value : fallback;
}

void secure_clear(std::string &value)
{
    volatile char *data = value.empty() ? nullptr : &value[0];
    for (size_t i = 0; data && i < value.size(); ++i) data[i] = 0;
    value.clear();
}

bool capture_ppm(std::vector<uint8_t> &out, std::string &error)
{
    const char *device = env_or_default("APPLAUNCH_LINUX_FBDEV_DEVICE", "/dev/fb0");
    int fd = open(device, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        error = std::string("open framebuffer: ") + std::strerror(errno);
        return false;
    }

    fb_var_screeninfo vinfo {};
    fb_fix_screeninfo finfo {};
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) != 0 ||
        ioctl(fd, FBIOGET_FSCREENINFO, &finfo) != 0) {
        error = std::string("query framebuffer: ") + std::strerror(errno);
        close(fd);
        return false;
    }
    if ((vinfo.bits_per_pixel != 16 && vinfo.bits_per_pixel != 32) ||
        vinfo.xres == 0 || vinfo.yres == 0) {
        error = "unsupported framebuffer format";
        close(fd);
        return false;
    }

    const size_t map_size = finfo.smem_len;
    const auto *fb = static_cast<const uint8_t *>(
        mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd, 0));
    if (fb == MAP_FAILED) {
        error = std::string("map framebuffer: ") + std::strerror(errno);
        close(fd);
        return false;
    }

    const cp0::framebuffer::Layout layout {
        vinfo.xres,
        vinfo.yres,
        vinfo.xoffset,
        vinfo.yoffset,
        vinfo.bits_per_pixel,
        finfo.line_length,
        {static_cast<uint8_t>(vinfo.red.offset), static_cast<uint8_t>(vinfo.red.length)},
        {static_cast<uint8_t>(vinfo.green.offset), static_cast<uint8_t>(vinfo.green.length)},
        {static_cast<uint8_t>(vinfo.blue.offset), static_cast<uint8_t>(vinfo.blue.length)},
    };
    const bool encoded = cp0::framebuffer::encode_ppm(fb, map_size, layout, out, error);

    munmap(const_cast<uint8_t *>(fb), map_size);
    close(fd);
    return encoded;
}

void key_subscriber(std::shared_ptr<zmq::context_t> context,
                    const std::atomic<bool> *stopping)
{
    try {
        zmq::socket_t sub(*context, zmq::socket_type::sub);
        sub.set(zmq::sockopt::linger, 0);
        sub.set(zmq::sockopt::subscribe, "");
        sub.connect(env_or_default("CP0_ZMQ_KEY_ENDPOINT", kDefaultKeyEndpoint));
        while (!stopping->load()) {
            RpcKeyEvent event {};
            zmq::recv_buffer_result_t result = sub.recv(zmq::buffer(&event, sizeof(event)));
            if (stopping->load()) break;
            if (!result || result->truncated() || result->size != sizeof(event) ||
                event.magic != kKeyMagic)
                continue;
            if (event.sender_pid == static_cast<uint32_t>(getpid())) continue;
            cp0_keyboard_inject(event.code, static_cast<int>(event.state), event.mods);
        }
    } catch (const zmq::error_t &e) {
        std::fprintf(stderr, "[rpc] key subscriber stopped: %s\n", e.what());
    }
}

void send_text(zmq::socket_t &socket, const std::string &text)
{
    socket.send(zmq::buffer(text), zmq::send_flags::none);
}

void rpc_broker(std::shared_ptr<zmq::context_t> context,
                const std::atomic<bool> *stopping, std::promise<bool> ready)
{
    bool readiness_reported = false;
    try {
        zmq::socket_t rep(*context, zmq::socket_type::rep);
        zmq::socket_t pub(*context, zmq::socket_type::pub);
        rep.set(zmq::sockopt::linger, 0);
        pub.set(zmq::sockopt::linger, 0);
        rep.bind(env_or_default("CP0_ZMQ_RPC_ENDPOINT", kDefaultRpcEndpoint));
        pub.bind(env_or_default("CP0_ZMQ_KEY_ENDPOINT", kDefaultKeyEndpoint));
        ready.set_value(true);
        readiness_reported = true;
        std::fprintf(stderr, "[rpc] automation broker ready pid=%d endpoint=%s\n",
                     static_cast<int>(getpid()),
                     env_or_default("CP0_ZMQ_RPC_ENDPOINT", kDefaultRpcEndpoint));

        while (!stopping->load()) {
            zmq::message_t request;
            if (!rep.recv(request, zmq::recv_flags::none)) continue;
            if (stopping->load()) break;
            std::string text(static_cast<const char *>(request.data()), request.size());
            std::istringstream input(text);
            std::string command;
            input >> command;

            if (command == "ping") {
                send_text(rep, "OK pong");
                continue;
            }
            if (command == "key") {
                uint32_t code = 0, state = 0, mods = 0;
                if (!(input >> code >> state >> mods) || state > KBD_KEY_REPEATED) {
                    send_text(rep, "ERR usage: key <evdev-code> <0|1|2> <mods>");
                    continue;
                }
                RpcKeyEvent event {kKeyMagic, static_cast<uint32_t>(getpid()), code, state, mods};
                cp0_keyboard_inject(code, static_cast<int>(state), mods);
                pub.send(zmq::buffer(&event, sizeof(event)), zmq::send_flags::none);
                send_text(rep, "OK key");
                continue;
            }
            if (command == "text") {
                std::string value;
                std::getline(input, value);
                if (!value.empty() && value.front() == ' ') value.erase(0, 1);
                if (value.empty()) {
                    send_text(rep, "ERR usage: text <utf8>");
                    continue;
                }
                if (cp0_keyboard_inject_text(value.c_str()) != 0) {
                    send_text(rep, "ERR invalid utf8 text");
                    continue;
                }
                send_text(rep, "OK text");
                continue;
            }
            if (command == "password") {
                std::string value;
                std::getline(input, value);
                if (!value.empty() && value.front() == ' ') value.erase(0, 1);
                if (value.empty()) {
                    send_text(rep, "ERR usage: password <value>");
                    continue;
                }
                const int result = cp0_sudo_queue_password(value.c_str());
                secure_clear(value);
                if (result != 0) {
                    send_text(rep, "ERR unable to queue password " + std::to_string(result));
                    continue;
                }
                send_text(rep, "OK password");
                continue;
            }
            if (command == "screenshot") {
                std::vector<uint8_t> ppm;
                std::string error;
                if (!capture_ppm(ppm, error)) {
                    send_text(rep, "ERR " + error);
                    continue;
                }
                rep.send(zmq::buffer("OK image/x-portable-pixmap"), zmq::send_flags::sndmore);
                rep.send(zmq::buffer(ppm), zmq::send_flags::none);
                continue;
            }
            send_text(rep, "ERR unknown command");
        }
    } catch (const zmq::error_t &e) {
        if (!readiness_reported)
            ready.set_value(false);
        std::fprintf(stderr, "[rpc] broker unavailable pid=%d: %s\n",
                     static_cast<int>(getpid()), e.what());
    } catch (const std::exception &e) {
        if (!readiness_reported)
            ready.set_value(false);
        std::fprintf(stderr, "[rpc] broker stopped pid=%d: %s\n",
                     static_cast<int>(getpid()), e.what());
    } catch (...) {
        if (!readiness_reported)
            ready.set_value(false);
        std::fprintf(stderr, "[rpc] broker stopped pid=%d: unknown error\n",
                     static_cast<int>(getpid()));
    }
}

class RpcRuntime {
public:
    ~RpcRuntime() { stop(); }

    bool start()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_.running()) return true;
        if (!state_.can_start()) return false;

        stopping_.store(false);
        auto context = std::make_shared<zmq::context_t>(1);
        std::promise<bool> ready;
        std::future<bool> readiness = ready.get_future();
        std::thread broker;
        std::thread subscriber;
        try {
            broker = std::thread(rpc_broker, context, &stopping_, std::move(ready));
        } catch (...) {
            return false;
        }
        const bool ready_ok = cp0::rpc::await_readiness(readiness);
        if (!ready_ok) {
            rollback_start(context, subscriber, broker);
            return false;
        }
        try {
            subscriber = std::thread(key_subscriber, context, &stopping_);
        } catch (...) {
            rollback_start(context, subscriber, broker);
            return false;
        }
        context_ = std::move(context);
        subscriber_ = std::move(subscriber);
        broker_ = std::move(broker);
        state_.mark_started();
        return true;
    }

    void stop() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_.can_start()) return;
        if (state_.running()) state_.mark_stopping();
        const auto stopped = cp0::rpc::shutdown_runtime(
            [&] { stopping_.store(true); },
            [&] { if (context_) context_->shutdown(); },
            [&] { if (subscriber_.joinable()) subscriber_.join(); },
            [&] { if (broker_.joinable()) broker_.join(); });
        if (stopped.workers_joined()) {
            context_.reset();
            state_.mark_stopped();
        }
    }

private:
    void rollback_start(const std::shared_ptr<zmq::context_t> &context,
                        std::thread &subscriber, std::thread &broker) noexcept
    {
        const auto stopped = cp0::rpc::shutdown_runtime(
            [&] { stopping_.store(true); },
            [&] { if (context) context->shutdown(); },
            [&] { if (subscriber.joinable()) subscriber.join(); },
            [&] { if (broker.joinable()) broker.join(); });
        if (!stopped.workers_joined()) {
            context_ = context;
            if (subscriber.joinable()) subscriber_ = std::move(subscriber);
            if (broker.joinable()) broker_ = std::move(broker);
            state_.mark_stopping();
        }
    }

    std::mutex mutex_;
    std::atomic<bool> stopping_{false};
    cp0::rpc::RuntimeState state_;
    std::shared_ptr<zmq::context_t> context_;
    std::thread subscriber_;
    std::thread broker_;
};

RpcRuntime &rpc_runtime()
{
    static RpcRuntime runtime;
    return runtime;
}

} // namespace

extern "C" void init_rpc(void)
{
    try { (void)rpc_runtime().start(); } catch (...) {}
}

extern "C" void deinit_rpc(void) noexcept
{
    try { rpc_runtime().stop(); } catch (...) {}
}

#else

extern "C" void init_rpc(void) {}
extern "C" void deinit_rpc(void) noexcept {}

#endif
