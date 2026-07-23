#pragma once

#include "lvgl/lvgl.h"

#include <string>

namespace zclaw::widgets {

lv_obj_t *box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
              lv_coord_t width, lv_coord_t height, uint32_t color,
              lv_coord_t radius = 0);
lv_obj_t *label(lv_obj_t *parent, const char *text, lv_coord_t x,
                lv_coord_t y, lv_coord_t width, lv_coord_t height,
                const lv_font_t *font, uint32_t color,
                lv_text_align_t alignment = LV_TEXT_ALIGN_LEFT);
lv_obj_t *label(lv_obj_t *parent, const std::string &text, lv_coord_t x,
                lv_coord_t y, lv_coord_t width, lv_coord_t height,
                const lv_font_t *font, uint32_t color,
                lv_text_align_t alignment = LV_TEXT_ALIGN_LEFT);
lv_obj_t *image(lv_obj_t *parent, const std::string &path,
                lv_coord_t x, lv_coord_t y);

}  // namespace zclaw::widgets
