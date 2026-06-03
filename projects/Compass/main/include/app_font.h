#pragma once

#include "lvgl/lvgl.h"

namespace ui::font {

/* codemap used by font 'assets/svgfont.ttf' (Unicode Private Use Area) */
/* C++ compiler automatically converts \uXXXX to correct UTF-8 bytes       */
inline constexpr const char* ICON_EXIT          = "\uEA01"; // .svgfont-exit
inline constexpr const char* ICON_FAST_FORWARD  = "\uEA02"; // .svgfont-fast_forward
inline constexpr const char* ICON_FAST_REWIND   = "\uEA03"; // .svgfont-fast_rewind
inline constexpr const char* ICON_LIST          = "\uEA04"; // .svgfont-list
inline constexpr const char* ICON_PAUSE         = "\uEA05"; // .svgfont-pause
inline constexpr const char* ICON_PLAY          = "\uEA06"; // .svgfont-play
inline constexpr const char* ICON_RECORD        = "\uEA07"; // .svgfont-record
inline constexpr const char* ICON_SAMPLE_RATE   = "\uEA08"; // .svgfont-sample_rate
inline constexpr const char* ICON_SPEED         = "\uEA09"; // .svgfont-speed
inline constexpr const char* ICON_STOP          = "\uEA0A"; // .svgfont-stop

class AppFont {
public:
    static void init();
    static void deinit();
    static lv_font_t* svgfont(int32_t size);

private:
    AppFont() = delete;
};

} // namespace ui::font
