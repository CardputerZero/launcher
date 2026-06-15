#include "hal_lvgl_bsp.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <utility>

namespace {

static void write_le16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }
static void write_le32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }

static int mkdir_p(const char *dir)
{
    if (!dir || !dir[0])
        return -1;
    char tmp[512];
    std::snprintf(tmp, sizeof(tmp), "%s", dir);
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

class ScreenshotSystem {
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    void api_call(arg_t arg, callback_t callback)
    {
        if (arg.empty()) {
            report(callback, -1, "empty screenshot api");
            return;
        }
        if (arg.front() == "Save") {
            const std::string dir = arg.size() >= 2 ? *std::next(arg.begin()) : std::string();
            int ret = save_to_bmp(dir.c_str());
            report(callback, ret, ret == 0 ? "screenshot saved\n" : "screenshot failed\n");
            return;
        }
        report(callback, -1, "unknown screenshot api");
    }

private:
    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    static int save_to_bmp(const char *dir)
    {
        if (mkdir_p(dir) != 0)
            return -1;

        std::time_t now = std::time(nullptr);
        std::tm *t = std::localtime(&now);
        if (!t)
            return -1;

        char filename[512];
        std::snprintf(filename, sizeof(filename), "%s/scr_%04d%02d%02d_%02d%02d%02d_sdl.bmp",
                      dir, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                      t->tm_hour, t->tm_min, t->tm_sec);

        FILE *fp = std::fopen(filename, "wb");
        if (!fp)
            return -1;

        constexpr int w = 320;
        constexpr int h = 240;
        const int row_size = w * 3;
        const int pad = (4 - (row_size % 4)) % 4;
        const int bmp_row = row_size + pad;
        const uint32_t img_size = static_cast<uint32_t>(bmp_row * h);
        const uint32_t file_size = 54 + img_size;

        fputc('B', fp); fputc('M', fp);
        write_le32(fp, file_size);
        write_le16(fp, 0); write_le16(fp, 0);
        write_le32(fp, 54);
        write_le32(fp, 40);
        write_le32(fp, w);
        write_le32(fp, h);
        write_le16(fp, 1);
        write_le16(fp, 24);
        write_le32(fp, 0);
        write_le32(fp, img_size);
        write_le32(fp, 2835); write_le32(fp, 2835);
        write_le32(fp, 0); write_le32(fp, 0);

        uint8_t padding[3] = {0, 0, 0};
        for (int y = h - 1; y >= 0; --y) {
            for (int x = 0; x < w; ++x) {
                const uint8_t r = static_cast<uint8_t>((x * 255) / (w - 1));
                const uint8_t g = static_cast<uint8_t>((y * 255) / (h - 1));
                const uint8_t b = static_cast<uint8_t>(((x / 20 + y / 20) % 2) ? 0x66 : 0x22);
                uint8_t bgr[3] = {b, g, r};
                fwrite(bgr, 1, 3, fp);
            }
            if (pad > 0)
                fwrite(padding, 1, static_cast<size_t>(pad), fp);
        }
        std::fclose(fp);
        std::printf("[SDL SCREENSHOT] Saved simulated screenshot: %s\n", filename);
        return 0;
    }
};

} // namespace

extern "C" void init_screenshot(void)
{
    auto screenshot = std::make_shared<ScreenshotSystem>();
    cp0_signal_screenshot_api.append([screenshot](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        screenshot->api_call(std::move(arg), std::move(callback));
    });
}
