#pragma once

#include "lvgl/lvgl.h"

#if LV_USE_FREETYPE
#include "lvgl/src/libs/freetype/lv_freetype.h"
#endif

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

class Cp0FontService
{
public:
    Cp0FontService() = default;
    ~Cp0FontService();

    Cp0FontService(const Cp0FontService &) = delete;
    Cp0FontService &operator=(const Cp0FontService &) = delete;

#if LV_USE_FREETYPE
    lv_font_t *get(const char *font_name, uint16_t size,
                   lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL,
                   lv_freetype_font_render_mode_t render_mode = LV_FREETYPE_FONT_RENDER_MODE_BITMAP);
    lv_font_t *get_mono(const char *font_name, uint16_t size,
                        lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL);
#endif

    lv_font_t *fallback(uint16_t size) const;
    void release();

private:
#if LV_USE_FREETYPE
    static std::string resolve_path(const char *font_name);
    static std::string key(const std::string &path, uint16_t size,
                           lv_freetype_font_style_t style,
                           lv_freetype_font_render_mode_t render_mode);
    std::unordered_map<std::string, lv_font_t *> fonts_;
    std::mutex mutex_;
#endif
};

Cp0FontService &cp0_fonts();
