SET(SOURCES components/ui_comp_button1.c
    components/ui_comp.c
    screens/ui_Screen1.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_zero_png.c
    images/ui_img_time_png.c
    images/ui_img_power_png.c
    images/ui_img_frame_3_png.c
    images/ui_img_frame_1_png.c
    images/ui_img_lorawan_gateway_on_coremp135_png.c
    images/ui_img_lora_1_png.c
    images/ui_img_rectangle_15_png.c
    images/ui_img_rectangle_16_png.c
    images/ui_img_vector_1_png.c
    images/ui_img_vector_2_png.c
    images/ui_img_vector_3_png.c
    images/ui_img_battery2_png.c
    images/ui_img_cm4_stack_logo1_png.c
    images/ui_img_1858829983.c
    fonts/ui_font_mono13.c
    fonts/ui_font_mono14.c
    fonts/ui_font_mono15.c
    fonts/ui_font_mono20.c)

add_library(ui ${SOURCES})
