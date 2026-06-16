#include "hal_lvgl_bsp.h"

#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static void write_le16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }
static void write_le32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }

namespace {

class ScreenshotSystem {
public:
    typedef std::function<void(int, std::string)> callback_t;
    typedef std::list<std::string> arg_t;

    void api_call(arg_t arg, callback_t callback)
    {
        if (arg.empty()) {
            report(callback, -1, "empty screenshot api\n");
            return;
        }

        if (arg.front() == "Save") {
            Save(std::move(arg), std::move(callback));
            return;
        }

        report(callback, -1, "unknown screenshot api\n");
    }

private:
    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback) callback(code, data);
    }

    static std::string first_arg_after_command(const arg_t &arg)
    {
        if (arg.size() < 2) return "";
        return *std::next(arg.begin());
    }

    void Save(arg_t arg, callback_t callback)
    {
        std::string dir = first_arg_after_command(arg);
        if (dir.empty()) {
            report(callback, -1, "Save need dir\n");
            return;
        }

        int ret = save_to_bmp(dir.c_str());
        report(callback, ret, ret == 0 ? "screenshot saved\n" : "screenshot failed\n");
    }

    static int save_to_bmp(const char *dir)
    {
        const char *fbdev = getenv("APPLAUNCH_LINUX_FBDEV_DEVICE");
        if (!fbdev) fbdev = "/dev/fb0";

        int fd = open(fbdev, O_RDONLY);
        if (fd < 0) return -1;

        struct fb_var_screeninfo vinfo;
        if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
            close(fd);
            return -2;
        }

        int w = vinfo.xres;
        int h = vinfo.yres;
        int bpp = vinfo.bits_per_pixel;
        int fb_line_len = w * (bpp / 8);

        struct fb_fix_screeninfo finfo;
        if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == 0)
            fb_line_len = finfo.line_length;

        size_t fb_size = fb_line_len * h;
        void *fbmem = mmap(NULL, fb_size, PROT_READ, MAP_SHARED, fd, 0);
        if (fbmem == MAP_FAILED) {
            close(fd);
            return -3;
        }

        {
            struct stat st;
            if (stat(dir, &st) != 0) {
                char tmp[512];
                snprintf(tmp, sizeof(tmp), "%s", dir);
                for (char *p = tmp + 1; *p; ++p) {
                    if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
                }
                mkdir(tmp, 0755);
            }
        }

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char filename[512];
        snprintf(filename, sizeof(filename), "%s/scr_%04d%02d%02d_%02d%02d%02d.bmp",
                 dir, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec);

        FILE *fp = fopen(filename, "wb");
        if (!fp) {
            munmap(fbmem, fb_size);
            close(fd);
            return -4;
        }

        int row_size = w * 3;
        int pad = (4 - (row_size % 4)) % 4;
        int bmp_row = row_size + pad;
        uint32_t img_size = bmp_row * h;
        uint32_t file_size = 54 + img_size;

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

        uint8_t padding[3] = {0};
        for (int y = h - 1; y >= 0; --y) {
            uint8_t *row = (uint8_t *)fbmem + y * fb_line_len;
            for (int x = 0; x < w; ++x) {
                uint8_t r, g, b;
                if (bpp == 16) {
                    uint16_t px = ((uint16_t *)row)[x];
                    r = ((px >> 11) & 0x1F) << 3;
                    g = ((px >> 5) & 0x3F) << 2;
                    b = (px & 0x1F) << 3;
                } else if (bpp == 32) {
                    uint32_t px = ((uint32_t *)row)[x];
                    r = (px >> vinfo.red.offset) & 0xFF;
                    g = (px >> vinfo.green.offset) & 0xFF;
                    b = (px >> vinfo.blue.offset) & 0xFF;
                } else {
                    r = g = b = 0;
                }
                uint8_t bgr[3] = {b, g, r};
                fwrite(bgr, 1, 3, fp);
            }
            if (pad > 0) fwrite(padding, 1, pad, fp);
        }

        fclose(fp);
        munmap(fbmem, fb_size);
        close(fd);

        printf("[SCREENSHOT] Saved: %s (%dx%d %dbpp)\n", filename, w, h, bpp);
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
