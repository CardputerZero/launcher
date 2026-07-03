#include "zclaw_fonts.hpp"

#include "cp0_lvgl_file.hpp"

#if LV_USE_FREETYPE
#include "lvgl/src/libs/freetype/lv_freetype.h"
#endif

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

#if LV_USE_FREETYPE
bool file_exists(const std::string &path)
{
    if (path.empty()) return false;
    std::ifstream file(path);
    return file.good();
}

std::string runtime_font_path()
{
    const char *env_path = std::getenv("ZCLAW_FONT");
    if (env_path && env_path[0] && file_exists(env_path)) {
        return env_path;
    }

    std::vector<std::string> candidates = {
        cp0_file_path("AlibabaPuHuiTi-3-55-Regular.ttf"),
        cp0_file_path("share/font/AlibabaPuHuiTi-3-55-Regular.ttf"),
        "./APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf",
        "../APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf",
        "./dist/APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf",
        "../dist/APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf",
        "../../dist/APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf",
        "/usr/share/APPLaunch/share/font/AlibabaPuHuiTi-3-55-Regular.ttf",
    };
    for (const auto &candidate : candidates) {
        if (file_exists(candidate)) return candidate;
    }
    return "";
}

std::string runtime_fallback_font_path()
{
    const char *env_path = std::getenv("ZCLAW_FALLBACK_FONT");
    if (env_path && env_path[0] && file_exists(env_path)) {
        return env_path;
    }

    std::vector<std::string> candidates = {
        cp0_file_path("NotoEmoji-Regular.ttf"),
        cp0_file_path("Symbola.ttf"),
        "/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/truetype/ancient-scripts/Symbola_hint.ttf",
        "/usr/share/fonts/truetype/ancient-scripts/Symbola.ttf",
    };
    for (const auto &candidate : candidates) {
        if (file_exists(candidate)) return candidate;
    }
    return "";
}

const lv_font_t *built_in_fallback_font()
{
#if defined(LV_FONT_SOURCE_HAN_SANS_SC_14_CJK) && LV_FONT_SOURCE_HAN_SANS_SC_14_CJK
    return &lv_font_source_han_sans_sc_14_cjk;
#elif defined(LV_FONT_SOURCE_HAN_SANS_SC_16_CJK) && LV_FONT_SOURCE_HAN_SANS_SC_16_CJK
    return &lv_font_source_han_sans_sc_16_cjk;
#else
    return &lv_font_montserrat_12;
#endif
}

void set_fallback_chain(lv_font_t *primary, lv_font_t *fallback)
{
    if (!primary) return;
    if (fallback) {
        primary->fallback = fallback;
        fallback->fallback = built_in_fallback_font();
    } else {
        primary->fallback = built_in_fallback_font();
    }
}
#endif

}  // namespace

namespace zclaw {

void FontManager::init()
{
#if LV_USE_FREETYPE
    if (font_10_ || font_12_) return;

    const std::string font_path = runtime_font_path();
    if (font_path.empty()) return;

    font_10_ = lv_freetype_font_create(
        font_path.c_str(), LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 10,
        LV_FREETYPE_FONT_STYLE_NORMAL);
    font_12_ = lv_freetype_font_create(
        font_path.c_str(), LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 12,
        LV_FREETYPE_FONT_STYLE_NORMAL);

    const std::string fallback_path = runtime_fallback_font_path();
    if (!fallback_path.empty()) {
        fallback_font_10_ = lv_freetype_font_create(
            fallback_path.c_str(), LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 10,
            LV_FREETYPE_FONT_STYLE_NORMAL);
        fallback_font_12_ = lv_freetype_font_create(
            fallback_path.c_str(), LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 12,
            LV_FREETYPE_FONT_STYLE_NORMAL);
    }
    set_fallback_chain(font_10_, fallback_font_10_);
    set_fallback_chain(font_12_, fallback_font_12_);
#endif
}

const lv_font_t *FontManager::font_10() const
{
#if LV_USE_FREETYPE
    if (font_10_) return font_10_;
    if (fallback_font_10_) return fallback_font_10_;
#endif
#if defined(LV_FONT_SOURCE_HAN_SANS_SC_14_CJK) && LV_FONT_SOURCE_HAN_SANS_SC_14_CJK
    return &lv_font_source_han_sans_sc_14_cjk;
#elif defined(LV_FONT_SOURCE_HAN_SANS_SC_16_CJK) && LV_FONT_SOURCE_HAN_SANS_SC_16_CJK
    return &lv_font_source_han_sans_sc_16_cjk;
#endif
    return &lv_font_montserrat_10;
}

const lv_font_t *FontManager::font_12() const
{
#if LV_USE_FREETYPE
    if (font_12_) return font_12_;
    if (fallback_font_12_) return fallback_font_12_;
#endif
#if defined(LV_FONT_SOURCE_HAN_SANS_SC_14_CJK) && LV_FONT_SOURCE_HAN_SANS_SC_14_CJK
    return &lv_font_source_han_sans_sc_14_cjk;
#elif defined(LV_FONT_SOURCE_HAN_SANS_SC_16_CJK) && LV_FONT_SOURCE_HAN_SANS_SC_16_CJK
    return &lv_font_source_han_sans_sc_16_cjk;
#endif
    return &lv_font_montserrat_12;
}

}  // namespace zclaw
