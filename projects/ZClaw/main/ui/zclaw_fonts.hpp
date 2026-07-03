#pragma once

#include "lvgl/lvgl.h"

namespace zclaw {

class FontManager {
public:
    FontManager() = default;
    FontManager(const FontManager &) = delete;
    FontManager &operator=(const FontManager &) = delete;

    void init();
    const lv_font_t *font_10() const;
    const lv_font_t *font_12() const;

private:
    lv_font_t *font_10_ = nullptr;
    lv_font_t *font_12_ = nullptr;
    lv_font_t *fallback_font_10_ = nullptr;
    lv_font_t *fallback_font_12_ = nullptr;
};

}  // namespace zclaw
