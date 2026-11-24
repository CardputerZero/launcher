/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "cp0_font_service.hpp"

namespace settings_fonts {

inline const lv_font_t *sans(uint16_t size,
                             lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL)
{
    return cp0_fonts().get("DejaVuSans.ttf", size, style);
}

inline const lv_font_t *serif(uint16_t size,
                              lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL)
{
    return cp0_fonts().get("DejaVuSerif.ttf", size, style);
}

inline const lv_font_t *mono(uint16_t size,
                             lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL)
{
    return cp0_fonts().get_mono("JetBrainsMono-Regular.ttf", size, style);
}

inline lv_font_t *cjk(const char *noto_name, const char *dejavu_name, uint16_t size,
                      lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL)
{
    auto &fonts = cp0_fonts();
    const lv_font_t *bitmap = fonts.fallback(size);
    constexpr auto render = LV_FREETYPE_FONT_RENDER_MODE_BITMAP;
    auto *alibaba = fonts.get("AlibabaPuHuiTi-3-55-Regular.ttf", size, style);
    auto *dejavu = fonts.get(dejavu_name, size, style, render,
                             alibaba == bitmap ? nullptr : alibaba,
                             Cp0FontService::GlyphRange::LatinExtendedA);
    auto *fallback = dejavu == bitmap ? alibaba : dejavu;
    auto *noto = fonts.get(noto_name, size, style, render,
                           fallback == bitmap ? nullptr : fallback);
    return noto == bitmap ? fallback : noto;
}

inline const lv_font_t *cjk_sans(uint16_t size,
                                 lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL)
{
    return cjk("NotoSansCJK-Regular.ttc", "DejaVuSans.ttf", size, style);
}

inline const lv_font_t *cjk_serif(uint16_t size,
                                  lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL)
{
    return cjk("NotoSerifCJK-Regular.ttc", "DejaVuSerif.ttf", size, style);
}

} // namespace settings_fonts
