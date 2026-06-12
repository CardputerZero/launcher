#include "app_font.h"

#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace ui::font {
namespace {

struct FontSlot {
    int32_t size = 0;
    lv_font_t* font = nullptr;
};

std::array<FontSlot, 4> slots{};
bool initialized = false;

std::vector<std::string> candidate_paths()
{
    return {
        "assets/svgfont.ttf",
        "./assets/svgfont.ttf",
        "../assets/svgfont.ttf",
        "../../assets/svgfont.ttf",
    };
}

void log_attempt(const char* path, bool success)
{
    if (success) {
        printf("[AppFont] svgfont loaded from: %s\n", path);
    } else {
        printf("[AppFont] svgfont failed to load from: %s\n", path);
    }
}

lv_font_t* fallback_font()
{
    return const_cast<lv_font_t*>(&lv_font_montserrat_12);
}

} // namespace

void AppFont::init()
{
    if (initialized) {
        return;
    }
    slots = {};
    initialized = true;
}

void AppFont::deinit()
{
#if LV_USE_FREETYPE
    for (auto& s : slots) {
        if (s.font) {
            lv_freetype_font_delete(s.font);
        }
        s.font = nullptr;
        s.size = 0;
    }
#endif
    initialized = false;
}

lv_font_t* AppFont::svgfont(int32_t size)
{
    init();
    if (size <= 0) {
        size = 12;
    }

    // Cache hit
    for (auto& s : slots) {
        if (s.font && s.size == size) {
            return s.font;
        }
    }

    // Find empty slot and load
    for (auto& s : slots) {
        if (!s.font) {
#if LV_USE_FREETYPE
            const auto paths = candidate_paths();
            printf("[AppFont] Trying to load svgfont (size=%d), candidates=%zu\n",
                   size, paths.size());
            for (const auto& p : paths) {
                s.font = lv_freetype_font_create(
                    p.c_str(),
                    LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                    static_cast<uint32_t>(size),
                    LV_FREETYPE_FONT_STYLE_NORMAL);
                log_attempt(p.c_str(), s.font != nullptr);
                if (s.font) {
                    s.size = size;
                    return s.font;
                }
            }
            printf("[AppFont] All svgfont candidates failed, falling back to montserrat_12\n");
#else
            printf("[AppFont] LV_USE_FREETYPE is disabled, falling back to montserrat_12\n");
            (void)candidate_paths;
#endif
            return fallback_font();
        }
    }

    printf("[AppFont] svgfont cache is full, falling back to montserrat_12\n");
    return fallback_font();
}

} // namespace ui::font
