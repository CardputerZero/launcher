// SPDX-License-Identifier: MIT
// Carousel titles must never use LVGL's in-place dots: the draw task keeps a
// pointer to label->text while GET_SELF_SIZE may revert and rewrite the dots.

#include "animation/ui_launcher_animation.h"
#include "lvgl/src/misc/lv_text_private.h"
#include "lvgl/src/widgets/label/lv_label_private.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef LV_LABEL_DOT_BEGIN_INV
#define LV_LABEL_DOT_BEGIN_INV 0xFFFFFFFF  // private to lv_label.c
#endif

namespace {

int failures = 0;

void check(bool ok, const char *what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

int32_t text_width(lv_obj_t *label)
{
    const char *text = lv_label_get_text(label);
    lv_text_attributes_t attributes{};
    attributes.max_width = LV_COORD_MAX;
    return lv_text_get_width(text, std::strlen(text), lv_obj_get_style_text_font(label, LV_PART_MAIN),
                             &attributes);
}

bool no_in_place_dots(lv_obj_t *label)
{
    auto *l = reinterpret_cast<lv_label_t *>(label);
    return l->dot_begin == LV_LABEL_DOT_BEGIN_INV &&
           lv_label_get_long_mode(label) != LV_LABEL_LONG_MODE_DOTS;
}

lv_obj_t *make_title(int32_t width, const char *text)
{
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    launcher_home_animation::set_title_text(label, text);
    return label;
}

} // namespace

int main()
{
    lv_init();
    lv_display_t *display = lv_display_create(320, 170);

    const char *long_name = "Webov\xC3\xA1 \xC4\x8Dte\xC4\x8Dka s dlouh\xC3\xBDm n\xC3\xA1zvem";

    // Inactive: clipped copy with "...", fits, and no in-place dots.
    lv_obj_t *title = make_title(61, long_name);
    launcher_home_animation::apply_title_overflow(title, false);
    lv_obj_update_layout(title);
    std::string shown = lv_label_get_text(title);
    check(shown.size() >= 3 && shown.compare(shown.size() - 3, 3, "...") == 0, "inactive title ends with ...");
    check(text_width(title) <= 61, "inactive title fits the slot");
    check(no_in_place_dots(title), "inactive title has no in-place dots");

    // Size queries must leave the buffer untouched (the race window).
    const std::string before = lv_label_get_text(title);
    lv_obj_invalidate(title);
    lv_obj_refresh_self_size(title);
    (void)lv_obj_get_self_height(title);
    check(before == lv_label_get_text(title), "self-size query keeps the text");
    check(no_in_place_dots(title), "self-size query adds no dots");

    // Selected: full name, scroll mode, still no dots.
    launcher_home_animation::apply_title_overflow(title, true);
    check(std::strcmp(lv_label_get_text(title), long_name) == 0, "selected title shows the full name");
    check(lv_label_get_long_mode(title) == LV_LABEL_LONG_MODE_SCROLL, "selected title scrolls");
    check(no_in_place_dots(title), "selected title has no in-place dots");

    // Renaming an inactive title must not show the previous name.
    launcher_home_animation::apply_title_overflow(title, false);
    launcher_home_animation::set_title_text(title, "Python");
    check(std::strcmp(lv_label_get_text(title), "Python") == 0, "renamed title shows the new name");

    // Narrower than "...": result still fits.
    lv_obj_t *tiny = make_title(4, long_name);
    launcher_home_animation::apply_title_overflow(tiny, false);
    check(text_width(tiny) <= 4, "tiny slot keeps the title inside the width");

    // Empty and short names stay as they are.
    lv_obj_t *empty = make_title(61, "");
    launcher_home_animation::apply_title_overflow(empty, false);
    check(std::strcmp(lv_label_get_text(empty), "") == 0, "empty title stays empty");
    lv_obj_t *short_title = make_title(80, "ZClaw");
    launcher_home_animation::apply_title_overflow(short_title, false);
    check(std::strcmp(lv_label_get_text(short_title), "ZClaw") == 0, "short title is unchanged");

    lv_obj_delete(title);
    lv_obj_delete(tiny);
    lv_obj_delete(empty);
    lv_obj_delete(short_title);
    lv_display_delete(display);

    if (failures) return EXIT_FAILURE;
    std::puts("title clip tests passed");
    return EXIT_SUCCESS;
}
