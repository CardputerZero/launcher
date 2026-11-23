/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "hal_lvgl_bsp.h"
#include "settings_fonts.hpp"
#include "ui.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" {
LV_FONT_DECLARE(cp0_montserrat_medium_latin_10)
LV_FONT_DECLARE(cp0_montserrat_medium_latin_12)
LV_FONT_DECLARE(cp0_montserrat_medium_latin_14)
LV_FONT_DECLARE(launcher_montserrat_bold_latin_16)
}

decltype(cp0_signal_filesystem_api) cp0_signal_filesystem_api;

namespace {

void check_chain(const lv_font_t *font)
{
    const lv_font_t *slow = font;
    const lv_font_t *fast = font;
    while (fast && fast->fallback) {
        slow = slow->fallback;
        fast = fast->fallback->fallback;
        assert(slow != fast && "font fallback cycle");
    }
}

lv_font_glyph_dsc_t glyph(const lv_font_t *font, uint32_t codepoint, const char *name)
{
    lv_font_glyph_dsc_t dsc{};
    if (!lv_font_get_glyph_dsc(font, &dsc, codepoint, 0) || dsc.is_placeholder) {
        std::fprintf(stderr, "%s: missing glyph U+%04X\n", name, codepoint);
        std::exit(1);
    }
    return dsc;
}

void check_latin(const lv_font_t *font, const lv_font_t &fallback,
                 int height, int baseline, const char *name)
{
    check_chain(font);
    assert(font->fallback == &fallback && !fallback.fallback);
    assert(font->line_height == height && font->base_line == baseline);
    for (uint32_t codepoint = 0xA0; codepoint <= 0x17F; ++codepoint) {
        const auto dsc = glyph(font, codepoint, name);
        if (dsc.box_h != 0 &&
            (font->base_line + dsc.ofs_y < 0 ||
             font->base_line + dsc.ofs_y + dsc.box_h > font->line_height)) {
            std::fprintf(stderr, "%s: U+%04X exceeds line bounds\n", name, codepoint);
            std::exit(1);
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    assert(argc == 2);
    const std::string font_dir = argv[1];
    bool hide_noto = true;
    bool hide_dejavu = false;
    bool hide_alibaba = false;
    cp0_signal_filesystem_api.append([&](std::list<std::string> args, auto reply) {
        const auto &name = args.back();
        const bool missing =
            (hide_noto && name.rfind("Noto", 0) == 0) ||
            (hide_dejavu && name.rfind("DejaVu", 0) == 0) ||
            (hide_alibaba && name.rfind("Alibaba", 0) == 0);
        reply(0, font_dir + (missing ? "/missing/" : "/") + name);
    });

    lv_init();
    auto &fonts = cp0_fonts();
    check_latin(fonts.fallback(10), cp0_montserrat_medium_latin_10, 13, 3, "Medium 10");
    check_latin(fonts.fallback(12), cp0_montserrat_medium_latin_12, 17, 4, "Medium 12");
    check_latin(fonts.fallback(14), cp0_montserrat_medium_latin_14, 19, 4, "Medium 14");
    check_latin(launcher_ui::carousel_title_font(), launcher_montserrat_bold_latin_16,
                22, 5, "Bold 16 title");

    constexpr uint16_t size = 14;
    constexpr auto style = LV_FREETYPE_FONT_STYLE_NORMAL;
    constexpr uint32_t chinese = 0x4E2D;
    constexpr uint32_t extended = 0x017D;
    const auto *alibaba = fonts.get("AlibabaPuHuiTi-3-55-Regular.ttf", size, style);
    assert(alibaba != fonts.fallback(size));
    for (const auto select : {settings_fonts::cjk_sans, settings_fonts::cjk_serif}) {
        hide_noto = true;
        const auto *without_noto = select(size, style);
        check_chain(without_noto);
        assert(glyph(without_noto, chinese, "CJK without Noto").resolved_font == alibaba);
        assert(glyph(without_noto, extended, "Latin Extended-A without Noto").resolved_font ==
               without_noto);
        assert(without_noto->fallback == alibaba && !alibaba->fallback);
        lv_font_glyph_dsc_t dsc{};
        assert(!without_noto->get_glyph_dsc(without_noto, &dsc, 'A', 0));
        assert(!without_noto->get_glyph_dsc(without_noto, &dsc, 0xE9, 0));
        assert(!without_noto->get_glyph_dsc(without_noto, &dsc, chinese, 0));
        assert(glyph(without_noto, 'A', "ASCII without Noto").resolved_font == alibaba);

        hide_noto = false;
        const auto *full = select(size, style);
        check_chain(full);
        assert(full->fallback == without_noto);
        assert(glyph(full, chinese, "CJK with Noto").resolved_font == full);
        assert(glyph(full, extended, "Latin Extended-A with Noto").resolved_font == without_noto);

        hide_dejavu = true;
        const auto *without_dejavu = select(size, style);
        check_chain(without_dejavu);
        assert(without_dejavu->fallback == alibaba);
        hide_noto = true;
        assert(select(size, style) == alibaba);
        hide_alibaba = true;
        assert(select(size, style) == fonts.fallback(size));
        hide_alibaba = hide_dejavu = false;
    }

    fonts.release();
    lv_deinit();
    std::puts("Font fallback regression tests passed");
}
