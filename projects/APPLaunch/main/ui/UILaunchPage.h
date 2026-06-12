#pragma once

#include "components/ui_app_page.hpp"
#include <array>
#include <memory>

class Launch;

class UILaunchPage : public home_base
{
public:
    explicit UILaunchPage(std::shared_ptr<Launch> launch);
    ~UILaunchPage();

    void load_home_screen();
    void start_startup_gif();
    void create_screen();
    enum LauncherCarouselElement : size_t {
        kCardFarLeft = 0,
        kCardLeft,
        kCardCenter,
        kCardRight,
        kCardFarRight,
        kTitleFarLeft,
        kTitleLeft,
        kTitleCenter,
        kTitleRight,
        kTitleFarRight,
        kPageDot0,
        kPageDot1,
        kPageDot2,
        kPageDot3,
        kPageDot4,
        kLauncherCarouselElementCount,
    };

    void init_input_group();
    static void bind_home_input_group();
    static lv_group_t *home_input_group();
    static lv_obj_t *panel(size_t slot);
    static lv_obj_t *label(size_t slot);

    void update_left_slot(lv_obj_t *panel, lv_obj_t *label);
    void update_right_slot(lv_obj_t *panel, lv_obj_t *label);
    void launch_selected_app();

    static std::array<lv_obj_t *, kLauncherCarouselElementCount> carousel_elements;

private:
    static void create_app_container(lv_obj_t *parent);

    std::shared_ptr<Launch> launch_;
};
