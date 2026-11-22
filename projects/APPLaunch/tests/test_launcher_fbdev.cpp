/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "../main/src/launcher_fbdev.h"
#include "../main/ui/animation/ui_launcher_animation.h"
#include "../main/ui/animation/launcher_carousel_layout.h"
#include "lvgl/lvgl.h"
#include "lvgl/lvgl_private.h"

#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string_view>
#include <linux/fb.h>
#include <sys/types.h>
#include <vector>

namespace {

constexpr int kFakeFd = 101;
constexpr int kWidth = 320;
constexpr int kHeight = 170;
constexpr off_t kStride = 640;
alignas(LV_DRAW_BUF_ALIGN) std::array<uint8_t, kStride * kHeight> pixels{};
unsigned original_flushes = 0;
unsigned opens = 0;
unsigned closes = 0;
int fb_stride = kStride;
int fb_width = kWidth;

enum class Fault { None, PartialThenEio, Zero, Permanent };
Fault fault = Fault::None;
struct Write {
    off_t offset;
    size_t size;
};
std::vector<Write> writes;
bool passed = true;

void expect(bool condition, const char *scenario)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", scenario);
    passed &= condition;
}

void original_flush(lv_display_t *display, const lv_area_t *, uint8_t *)
{
    ++original_flushes;
    lv_display_flush_ready(display);
}

lv_display_t *new_display(int width = kWidth,
                          lv_display_render_mode_t mode = LV_DISPLAY_RENDER_MODE_PARTIAL)
{
    auto *display = lv_display_create(width, kHeight);
    lv_display_set_default(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, pixels.data(), nullptr, pixels.size(), mode);
    lv_display_set_flush_cb(display, original_flush);
    return display;
}

void invalidate_band(lv_display_t *display)
{
    const lv_area_t band{14, 73, 25, 79};
    lv_inv_area(display, &band);
}

void turn()
{
    lv_tick_inc(100);
    lv_timer_handler();
}

void exercise_failure(Fault scenario)
{
    auto *display = new_display();
    launcher_enable_fbdev_stripes();
    turn();
    writes.clear();
    fault = scenario;
    invalidate_band(display);
    turn();
    const size_t first_refresh = writes.size();
    for (int i = 0; i < 8; ++i) turn();
    constexpr off_t offset = 73 * kStride;
    constexpr size_t size = 7 * kStride;
    if (scenario == Fault::PartialThenEio) {
        expect(first_refresh == 2 && writes.size() == 3 &&
                   writes[0].offset == offset && writes[0].size == size &&
                   writes[1].offset == offset + 2 && writes[1].size == size - 2 &&
                   writes[2].offset == offset && writes[2].size == size,
               "short write then EIO redraws the original band on the next turn");
    } else if (scenario == Fault::Zero) {
        expect(first_refresh == 1 && writes.size() == 2 &&
                   writes[1].offset == offset && writes[1].size == size,
               "zero write redraws the original band on the next turn");
    } else {
        expect(first_refresh == 1 && writes.size() == 3,
               "permanent EIO stops after exactly two redraws");
        fault = Fault::None;
        invalidate_band(display);
        turn();
        writes.clear();
        fault = Fault::Zero;
        invalidate_band(display);
        turn();
        turn();
        expect(writes.size() == 2, "successful refresh re-arms the retry budget");
    }
    fault = Fault::None;
    lv_display_delete(display);
}

void exercise_carousel(lv_display_t *display, uint32_t expected, const char *scenario)
{
    lv_obj_t *items[launcher_carousel_layout::kElementCount]{};
    launcher_home_animation::animate_right(items, {}, {});
    expect(lv_display_get_refr_timer(display)->period == expected &&
               lv_anim_get_timer()->period == expected,
           scenario);
    for (int i = 0; i < 4; ++i) turn();
    expect(lv_display_get_refr_timer(display)->period == LV_DEF_REFR_PERIOD &&
               lv_anim_get_timer()->period == LV_DEF_REFR_PERIOD,
           "carousel restores the default cadence after transition");
}

void expect_fallback(lv_display_t *display, const char *scenario)
{
    writes.clear();
    original_flushes = 0;
    launcher_enable_fbdev_stripes();
    turn();
    expect(original_flushes > 0 && writes.empty(), scenario);
    lv_display_delete(display);
}

} // namespace

extern "C" int __real_open(const char *, int, ...);
extern "C" int __real_close(int);
extern "C" ssize_t __real_pwrite(int, const void *, size_t, off_t);

extern "C" int __wrap_open(const char *name, int flags, ...)
{
    if (std::string_view(name) == "/fake-fb") {
        ++opens;
        return kFakeFd;
    }
    if ((flags & O_CREAT) != 0) {
        va_list args;
        va_start(args, flags);
        const int mode = va_arg(args, int);
        va_end(args);
        return __real_open(name, flags, mode);
    }
    return __real_open(name, flags);
}

extern "C" int __wrap_close(int fd)
{
    if (fd != kFakeFd) return __real_close(fd);
    ++closes;
    return 0;
}

extern "C" int __wrap_ioctl(int fd, unsigned long request, ...)
{
    if (fd != kFakeFd) std::abort();
    va_list args;
    va_start(args, request);
    if (request == FBIOGET_FSCREENINFO) {
        auto *fixed = va_arg(args, fb_fix_screeninfo *);
        fixed->line_length = fb_stride;
        fixed->smem_len = fb_stride * kHeight;
    } else if (request == FBIOGET_VSCREENINFO) {
        auto *var = va_arg(args, fb_var_screeninfo *);
        var->xres = fb_width;
        var->yres = kHeight;
        var->bits_per_pixel = 16;
        var->red.offset = 11;
        var->green.offset = 5;
    } else {
        std::abort();
    }
    va_end(args);
    return 0;
}

extern "C" ssize_t __wrap_pwrite(int fd, const void *data, size_t size, off_t offset)
{
    if (fd != kFakeFd) return __real_pwrite(fd, data, size, offset);
    writes.push_back({offset, size});
    if (fault == Fault::Zero && writes.size() == 1) return 0;
    if (fault == Fault::PartialThenEio && writes.size() == 1) return 2;
    if (fault == Fault::Permanent || (fault == Fault::PartialThenEio && writes.size() == 2)) {
        errno = EIO;
        return -1;
    }
    return size;
}

int main()
{
    setenv("LV_LINUX_FBDEV_DEVICE", "/fake-fb", 1);
    lv_init();
    exercise_failure(Fault::PartialThenEio);
    exercise_failure(Fault::Zero);
    exercise_failure(Fault::Permanent);

    auto *display = new_display();
    lv_display_set_offset(display, 1, 0);
    expect_fallback(display, "LVGL X offset keeps the original flush");
    display = new_display();
    lv_display_set_offset(display, 0, 1);
    expect_fallback(display, "LVGL Y offset keeps the original flush");
    expect_fallback(new_display(kWidth - 1), "logical width mismatch keeps the original flush");
    expect_fallback(new_display(kWidth, LV_DISPLAY_RENDER_MODE_DIRECT),
                    "direct render mode keeps the original flush");
    expect_fallback(new_display(kWidth, LV_DISPLAY_RENDER_MODE_FULL),
                    "full render mode keeps the original flush");
    fb_stride = kStride + 2;
    expect_fallback(new_display(), "framebuffer stride mismatch keeps the original flush");
    fb_stride = kStride;
    auto *fallback = new_display();
    lv_display_set_offset(fallback, 1, 0);
    launcher_enable_fbdev_stripes();
    exercise_carousel(fallback, LV_DEF_REFR_PERIOD, "fallback display keeps the default carousel cadence");
    lv_display_delete(fallback);

    fb_width = kWidth - 1;
    expect_fallback(new_display(), "physical width mismatch keeps the original flush");
    fb_width = kWidth;

    display = new_display();
    launcher_enable_fbdev_stripes();
    exercise_carousel(display, 24, "stripe display uses the 24 ms carousel cadence");
    writes.clear();
    original_flushes = 0;
    lv_display_set_offset(display, 1, 0);
    invalidate_band(display);
    turn();
    expect(original_flushes > 0 && writes.empty(), "offset change forwards to the original flush");
    lv_display_delete(display);

    expect(opens == closes, "display deletion closes each opened framebuffer");
    lv_deinit();
    return passed ? 0 : 1;
}
