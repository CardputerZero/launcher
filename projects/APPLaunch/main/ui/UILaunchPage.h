#pragma once

#include "components/ui_app_page.hpp"
#include <memory>

class Launch;

class UILaunchPage : public home_base
{
public:
    explicit UILaunchPage(std::shared_ptr<Launch> launch);
    ~UILaunchPage();

    static void init_ui();
    static void load_home_screen();
    static void start_startup_gif();
    static void create_screen();
    static void init_input_group();
    static void bind_home_input_group();
    static lv_group_t *home_input_group();

private:
    static void init_images();
    static void init_fonts();
    static void create_top(lv_obj_t *parent);
    static void create_app_container(lv_obj_t *parent);

    std::shared_ptr<Launch> launch_;
};
