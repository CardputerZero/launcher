#include "cp0_lvgl.h"
#include "cp0_lvgl_app.h"

#ifndef CP0_LVGL_USE_ZMQ_RPC
#define CP0_LVGL_USE_ZMQ_RPC 0
#endif

#if CP0_LVGL_USE_ZMQ_RPC

#include "keyboard_input.h"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
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

    const std::string header = "P6\n" + std::to_string(vinfo.xres) + " " +
                               std::to_string(vinfo.yres) + "\n255\n";
    out.assign(header.begin(), header.end());
    out.resize(header.size() + static_cast<size_t>(vinfo.xres) * vinfo.yres * 3);
    uint8_t *dst = out.data() + header.size();
    const size_t pixel_bytes = vinfo.bits_per_pixel / 8;
    for (uint32_t y = 0; y < vinfo.yres; ++y) {
        const size_t row_offset = static_cast<size_t>(y + vinfo.yoffset) * finfo.line_length +
                                  static_cast<size_t>(vinfo.xoffset) * pixel_bytes;
        if (row_offset + static_cast<size_t>(vinfo.xres) * pixel_bytes > map_size) {
            munmap(const_cast<uint8_t *>(fb), map_size);
            close(fd);
            error = "framebuffer bounds mismatch";
            out.clear();
            return false;
        }
        const uint8_t *row = fb + row_offset;
        for (uint32_t x = 0; x < vinfo.xres; ++x) {
            uint8_t r = 0, g = 0, b = 0;
            if (vinfo.bits_per_pixel == 16) {
                uint16_t px = 0;
                std::memcpy(&px, row + x * 2, sizeof(px));
                r = static_cast<uint8_t>(((px >> 11) & 0x1f) * 255 / 31);
                g = static_cast<uint8_t>(((px >> 5) & 0x3f) * 255 / 63);
                b = static_cast<uint8_t>((px & 0x1f) * 255 / 31);
            } else {
                uint32_t px = 0;
                std::memcpy(&px, row + x * 4, sizeof(px));
                r = static_cast<uint8_t>((px >> vinfo.red.offset) & 0xff);
                g = static_cast<uint8_t>((px >> vinfo.green.offset) & 0xff);
                b = static_cast<uint8_t>((px >> vinfo.blue.offset) & 0xff);
            }
            *dst++ = r;
            *dst++ = g;
            *dst++ = b;
        }
    }

    munmap(const_cast<uint8_t *>(fb), map_size);
    close(fd);
    return true;
}

void key_subscriber(std::shared_ptr<zmq::context_t> context)
{
    try {
        zmq::socket_t sub(*context, zmq::socket_type::sub);
        sub.set(zmq::sockopt::linger, 0);
        sub.set(zmq::sockopt::subscribe, "");
        sub.connect(env_or_default("CP0_ZMQ_KEY_ENDPOINT", kDefaultKeyEndpoint));
        for (;;) {
            RpcKeyEvent event {};
            zmq::recv_buffer_result_t result = sub.recv(zmq::buffer(&event, sizeof(event)));
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

void rpc_broker(std::shared_ptr<zmq::context_t> context)
{
    try {
        zmq::socket_t rep(*context, zmq::socket_type::rep);
        zmq::socket_t pub(*context, zmq::socket_type::pub);
        rep.set(zmq::sockopt::linger, 0);
        pub.set(zmq::sockopt::linger, 0);
        rep.bind(env_or_default("CP0_ZMQ_RPC_ENDPOINT", kDefaultRpcEndpoint));
        pub.bind(env_or_default("CP0_ZMQ_KEY_ENDPOINT", kDefaultKeyEndpoint));
        std::fprintf(stderr, "[rpc] automation broker ready pid=%d endpoint=%s\n",
                     static_cast<int>(getpid()),
                     env_or_default("CP0_ZMQ_RPC_ENDPOINT", kDefaultRpcEndpoint));

        for (;;) {
            zmq::message_t request;
            if (!rep.recv(request, zmq::recv_flags::none)) continue;
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
        std::fprintf(stderr, "[rpc] broker unavailable pid=%d: %s\n",
                     static_cast<int>(getpid()), e.what());
    }
}

} // namespace

extern "C" void init_rpc(void)
{
    auto context = std::make_shared<zmq::context_t>(1);
    std::thread(key_subscriber, context).detach();
    std::thread(rpc_broker, std::move(context)).detach();
}

#else

extern "C" void init_rpc(void) {}

#endif
