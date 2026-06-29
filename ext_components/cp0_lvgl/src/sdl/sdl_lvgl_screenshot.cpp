#include "hal_lvgl_bsp.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"

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
#include <SDL.h>

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

        lv_display_t *disp = lv_display_get_default();
        if (!disp)
            return -2;
        lv_refr_now(disp);

        SDL_Renderer *renderer = static_cast<SDL_Renderer *>(lv_sdl_window_get_renderer(disp));
        if (!renderer)
            return -3;

        int w = 0;
        int h = 0;
        if (SDL_GetRendererOutputSize(renderer, &w, &h) != 0 || w <= 0 || h <= 0)
            return -4;

        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surface)
            return -5;

        int ret = 0;
        if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) != 0) {
            ret = -6;
        } else {
            std::time_t now = std::time(nullptr);
            std::tm *t = std::localtime(&now);
            if (!t) {
                ret = -7;
            } else {
                char filename[512];
                std::snprintf(filename, sizeof(filename), "%s/scr_%04d%02d%02d_%02d%02d%02d_sdl.bmp",
                              dir, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                              t->tm_hour, t->tm_min, t->tm_sec);
                if (SDL_SaveBMP(surface, filename) != 0) {
                    ret = -8;
                } else {
                    std::printf("[SDL SCREENSHOT] Saved screenshot: %s\n", filename);
                }
            }
        }

        SDL_FreeSurface(surface);
        return ret;
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
