#pragma once

#include "ui/ui.h"
#include "../ui_app_page.hpp"
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>

#include "compat/input_keys.h"
#include <keyboard_input.h>
#include <iostream>

/*
 * ============================================================
 *  UIMidiPage
 *
 *  Full-screen MIDI music
 *  Screen: 320 x 170
 *
 *  Displayed content:
 *    - Temperature
 *    - Humidity
 *    - date and time
 *    - temperature/humidity trend chart
 *
 *  Keys:
 *    ESC return to the home screen
 * ============================================================
 */
class UIMidiPage : public app_
{
public:
    UIMidiPage() : app_()
    {
        system("gpioset -c gpiochip0 4=1 &");
        system("gpioset -c gpiochip0 17=1 &");
        system("sh -c 'sleep 4 ; /home/pi/roller485 -b 1 mode 1 ; /home/pi/roller485 -b 1 enable ; /home/pi/roller485 -b 1 speed 30 ' &");
        creat_UI();
        event_handler_init();
    }

    ~UIMidiPage()
    {
        system("pkill gpioset");
        system("gpioset -c gpiochip0 17=0 &");
        system("sleep 0.1 && pkill gpioset");
    }
private:
    lv_obj_t *play_gif;
private:
    std::string app_name = img_path("audio.png");
    
    /*
     * ============================================================
     * UI construction
     * ============================================================
     */
    void creat_UI()
    {
        std::cout << "UIMidiPage: app_name = " << app_name << std::endl;
        play_gif = lv_gif_create(ui_root);
        lv_gif_set_src(play_gif, app_name.c_str());
        lv_obj_center(play_gif);
    }

private:
    /*
     * ============================================================
     * Key events
     * ============================================================
     */
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIMidiPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIMidiPage *self = static_cast<UIMidiPage *>(lv_event_get_user_data(e));
        if (self) {
            self->event_handler(e);
        }
    }

    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e)) {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            handle_key(key);
        }
    }

    void handle_key(uint32_t key)
    {
        switch (key) {
        case KEY_ESC:
            if (go_back_home) {
                go_back_home();
            }
            break;

        default:
            break;
        }
    }
};