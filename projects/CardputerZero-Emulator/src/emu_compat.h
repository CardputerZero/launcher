#pragma once
#ifndef LV_COLOR_16_SWAP
#define LV_COLOR_16_SWAP 0
#endif

// Stub freetype API when disabled (Emscripten build)
#if defined(LV_USE_FREETYPE) && LV_USE_FREETYPE == 0
#ifndef LV_FREETYPE_FONT_RENDER_MODE_BITMAP
#define LV_FREETYPE_FONT_RENDER_MODE_BITMAP 0
#define LV_FREETYPE_FONT_RENDER_MODE_BITMAP_MONO 2
#define LV_FREETYPE_FONT_STYLE_NORMAL 0
#define LV_FREETYPE_FONT_STYLE_BOLD   1
#include "lvgl/lvgl.h"
typedef int lv_freetype_font_style_t;
typedef int lv_freetype_font_render_mode_t;
static inline lv_font_t *lv_freetype_font_create(const char *path, lv_freetype_font_render_mode_t mode,
                                                 int size, lv_freetype_font_style_t style)
{ (void)path; (void)mode; (void)size; (void)style; return NULL; }
static inline void lv_freetype_font_delete(lv_font_t *font)
{ (void)font; }
#endif
#endif
