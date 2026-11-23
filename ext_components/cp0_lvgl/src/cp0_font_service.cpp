/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_font_service.hpp"

#include "cp0_font_cache_policy.hpp"
#include "cp0_sync_signal.hpp"
#include "hal_lvgl_bsp.h"

#include <algorithm>
#include <utility>

extern "C" {
LV_FONT_DECLARE(cp0_montserrat_medium_latin_10)
LV_FONT_DECLARE(cp0_montserrat_medium_latin_12)
LV_FONT_DECLARE(cp0_montserrat_medium_latin_14)
}

namespace {

void set_glyph_fallback(lv_font_t &font, const lv_font_t &fallback)
{
    // LVGL positions a fallback glyph against the primary font's baseline.
    const int ascent = std::max(font.line_height - font.base_line,
                                fallback.line_height - fallback.base_line);
    font.base_line = std::max(font.base_line, fallback.base_line);
    font.line_height = ascent + font.base_line;
    font.fallback = &fallback;
}

template<const lv_font_t &Primary, const lv_font_t &Latin>
lv_font_t *montserrat_with_latin()
{
    static lv_font_t font = [] {
        lv_font_t copy = Primary;
        set_glyph_fallback(copy, Latin);
        return copy;
    }();
    return &font;
}

} // namespace

Cp0FontService::~Cp0FontService()
{
    release();
}

lv_font_t *Cp0FontService::get(const char *font_name, uint16_t size,
                               lv_freetype_font_style_t style,
                               lv_freetype_font_render_mode_t render_mode,
                               const lv_font_t *glyph_fallback, GlyphRange glyph_range)
{
#if LV_USE_FREETYPE
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string path = resolve_path(font_name);
    if (path.empty()) return fallback(size);

    const std::string font_key = key(path, size, style, render_mode, glyph_fallback, glyph_range);
    const auto found = fonts_.find(font_key);
    if (found != fonts_.end()) return found->second.font;

    lv_font_t *font = lv_freetype_font_create(path.c_str(), render_mode, size, style);
    if (!cp0::font::should_cache(font)) return fallback(size);
    if (glyph_fallback) set_glyph_fallback(*font, *glyph_fallback);
    try {
        auto entry = fonts_.emplace(font_key, CachedFont{font, font->get_glyph_dsc}).first;
        if (glyph_range == GlyphRange::LatinExtendedA) {
            // The cached callback's address stays valid across rehashes.
            font->user_data = &entry->second.get_glyph_dsc;
            font->get_glyph_dsc = [](const lv_font_t *face, lv_font_glyph_dsc_t *dsc,
                                     uint32_t letter, uint32_t next) {
                if (letter < 0x0100 || letter > 0x017F) return false;
                const auto get_glyph_dsc = *static_cast<const GlyphCallback *>(face->user_data);
                return get_glyph_dsc(face, dsc, letter, next);
            };
        }
    } catch (...) {
        lv_freetype_font_delete(font);
        return fallback(size);
    }
    return font;
#else
    (void)font_name;
    (void)style;
    (void)render_mode;
    (void)glyph_fallback;
    (void)glyph_range;
    return fallback(size);
#endif
}

lv_font_t *Cp0FontService::get_mono(const char *font_name, uint16_t size,
                                    lv_freetype_font_style_t style)
{
    return get(font_name, size, style,
               static_cast<lv_freetype_font_render_mode_t>(2));
}

#if LV_USE_FREETYPE
std::string Cp0FontService::resolve_path(const char *font_name)
{
    if (!font_name || !font_name[0]) return {};
    std::string value(font_name);
    if (value.find('/') != std::string::npos ||
        (value.size() > 1 && value[1] == ':')) return value;

    std::string resolved;
    cp0::signal::invoke_noexcept([&] { cp0_signal_filesystem_api({"Path", value}, [&](int code, std::string data) {
        if (code == 0) resolved = std::move(data);
    }); });
    return resolved.empty() ? value : resolved;
}

std::string Cp0FontService::key(const std::string &path, uint16_t size,
                                lv_freetype_font_style_t style,
                                lv_freetype_font_render_mode_t render_mode,
                                const lv_font_t *glyph_fallback, GlyphRange glyph_range)
{
    return path + "#" + std::to_string(size) + "#" +
           std::to_string(static_cast<int>(style)) + "#" +
           std::to_string(static_cast<int>(render_mode)) + "#" +
           std::to_string(reinterpret_cast<std::uintptr_t>(glyph_fallback)) +
           (glyph_range == GlyphRange::LatinExtendedA ? "#latin-extended-a" : "");
}
#endif

lv_font_t *Cp0FontService::fallback(uint16_t size) const
{
#if LV_FONT_MONTSERRAT_20
    if (size >= 18) return const_cast<lv_font_t *>(&lv_font_montserrat_20);
#endif
#if LV_FONT_MONTSERRAT_14
    if (size >= 14) return montserrat_with_latin<lv_font_montserrat_14,
                                                  cp0_montserrat_medium_latin_14>();
#endif
#if LV_FONT_MONTSERRAT_12
    if (size >= 12) return montserrat_with_latin<lv_font_montserrat_12,
                                                  cp0_montserrat_medium_latin_12>();
#endif
#if LV_FONT_MONTSERRAT_10
    (void)size;
    return montserrat_with_latin<lv_font_montserrat_10, cp0_montserrat_medium_latin_10>();
#else
    (void)size;
    return const_cast<lv_font_t *>(LV_FONT_DEFAULT);
#endif
}

void Cp0FontService::release()
{
#if LV_USE_FREETYPE
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &font : fonts_) {
        lv_freetype_font_delete(font.second.font);
    }
    fonts_.clear();
#endif
}

Cp0FontService &cp0_fonts()
{
    static Cp0FontService service;
    return service;
}
