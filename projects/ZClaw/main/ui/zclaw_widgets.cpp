#include "zclaw_widgets.h"

namespace zclaw::widgets {

lv_obj_t *box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
              lv_coord_t width, lv_coord_t height, uint32_t color,
              lv_coord_t radius)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_radius(object, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(object, lv_color_hex(color),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(object, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE |
                                                         LV_OBJ_FLAG_SCROLLABLE));
    return object;
}

lv_obj_t *label(lv_obj_t *parent, const char *text, lv_coord_t x,
                lv_coord_t y, lv_coord_t width, lv_coord_t height,
                const lv_font_t *font, uint32_t color,
                lv_text_align_t alignment)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, text ? text : "");
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_label_set_long_mode(object, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(object, lv_color_hex(color),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(object, LV_OPA_COVER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(object, alignment,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

lv_obj_t *label(lv_obj_t *parent, const std::string &text, lv_coord_t x,
                lv_coord_t y, lv_coord_t width, lv_coord_t height,
                const lv_font_t *font, uint32_t color,
                lv_text_align_t alignment)
{
    return label(parent, text.c_str(), x, y, width, height, font, color,
                 alignment);
}

lv_obj_t *image(lv_obj_t *parent, const std::string &path,
                lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *object = lv_img_create(parent);
    lv_img_set_src(object, path.c_str());
    lv_obj_set_pos(object, x, y);
    lv_obj_clear_flag(object, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE |
                                                         LV_OBJ_FLAG_SCROLLABLE));
    return object;
}

}  // namespace zclaw::widgets
