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

inline const lv_font_t *cjk_sans(uint16_t size,
                                 lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL)
{
    auto &fonts = cp0_fonts();
    const lv_font_t *dejavu = sans(size, style);
    const lv_font_t *bitmap = fonts.fallback(size);
    const lv_font_t *noto = fonts.get("NotoSansCJK-Regular.ttc", size, style,
                                     LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                     dejavu == bitmap ? nullptr : dejavu);
    return noto == bitmap ? dejavu : noto;
}

inline const lv_font_t *cjk_serif(uint16_t size,
                                  lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL)
{
    const lv_font_t *font = cp0_fonts().get("NotoSerifCJK-Regular.ttc", size, style);
    if (font == cp0_fonts().fallback(size))
        font = cp0_fonts().get("AlibabaPuHuiTi-3-55-Regular.ttf", size, style);
    return font;
}

} // namespace settings_fonts
