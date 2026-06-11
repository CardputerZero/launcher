#pragma once
#if !defined(HAL_PLATFORM_SDL)
#include "../ui_app_page.hpp"
#include "TinyGPS++.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#pragma once
#include "../../ui.h"
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

/*
 * ============================================================
 *  UIHikePodPage
 *
 *  Account Overview Dashboard
 *  Screen: 320 x 170
 *
 *  Keys:
 *    ESC return to the home screen
 * ============================================================
 */
class UIHikePodPage : public app_
{
    public:
    UIHikePodPage() : app_()
    {
        creat_UI();
        event_handler_init();
    }

    ~UIHikePodPage()
    {
    }
private:
    lv_obj_t *play_gif;
private:
    std::string app_name = img_path("hikepod.gif");
    
    /*
     * ============================================================
     * UI construction
     * ============================================================
     */
    void creat_UI()
    {
        // std::cout << "UIHikePodPage: app_name = " << app_name << std::endl;
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
        lv_obj_add_event_cb(ui_root, UIHikePodPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t *e)
    {
        UIHikePodPage *self = static_cast<UIHikePodPage *>(lv_event_get_user_data(e));
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

// class UIHikePodPage : public app_base
// {
//     enum class ViewMode
//     {
//         NAV,
//         GPS_INFO
//     };

//     struct RoutePoint
//     {
//         int x;
//         int y;
//     };

//     struct SatInfo
//     {
//         const char *system;
//         int id;
//         int elevation;
//         int azimuth;
//         int snr;
//         bool used;
//         bool visible;
//     };

//     static constexpr uint32_t KEY_SWITCH_VIEW = 15;
//     static constexpr uint32_t KEY_UP_CODE = 33;
//     static constexpr uint32_t KEY_DOWN_CODE = 45;
//     static constexpr uint32_t KEY_LEFT_CODE = 44;
//     static constexpr uint32_t KEY_RIGHT_CODE = 46;

// public:
//     UIHikePodPage() : app_base()
//     {
//         init_gps_serial();
//         init_mock_data();
//         creat_UI();
//         event_handler_init();
//         tick_timer_ = lv_timer_create(UIHikePodPage::static_tick_cb, 250, this);
//     }

//     ~UIHikePodPage()
//     {
//         if (tick_timer_)
//         {
//             lv_timer_del(tick_timer_);
//             tick_timer_ = nullptr;
//         }
//         if (gps_fd_ != -1)
//         {
//             close(gps_fd_);
//         }
//     }

// private:
//     std::unordered_map<std::string, lv_obj_t *> ui_obj_;
//     lv_timer_t *tick_timer_ = nullptr;
//     ViewMode view_mode_ = ViewMode::NAV;

//     TinyGPSPlus gps_;
//     int gps_fd_ = -1;
//     std::string nmea_line_buffer_;

//     std::vector<RoutePoint> route_points_;
//     std::vector<lv_point_precise_t> route_line_points_;
//     std::vector<RoutePoint> track_points_;
//     std::vector<lv_point_precise_t> track_line_points_;
//     std::vector<SatInfo> satellites_;

//     float current_lat_ = 39.90420f;
//     float current_lng_ = 116.40740f;
//     float current_alt_ = 128.4f;
//     float current_speed_ = 4.2f;
//     float current_course_ = 72.0f;
//     float current_hdop_ = 1.2f;
//     int battery_level_ = 86;
//     int zoom_level_ = 7;
//     bool gps_fixed_ = true;
//     // NMEA sentence presence flags (if sentence has appeared on the raw stream)
//     bool seen_gga_ = false;
//     bool seen_rmc_ = false;
//     bool seen_vtg_ = false;
//     bool seen_zda_ = false;
//     bool seen_gsv_ = false;
//     bool seen_gll_ = false;
//     bool seen_txt_ = false;
//     bool seen_gsa_ = false;

//     void init_gps_serial()
//     {
//         gps_fd_ = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY);
//         if (gps_fd_ == -1)
//             return;

//         struct termios options;
//         tcgetattr(gps_fd_, &options);
//         cfsetispeed(&options, B115200);
//         cfsetospeed(&options, B115200);
//         options.c_cflag |= (CLOCAL | CREAD);
//         options.c_cflag &= ~PARENB;
//         options.c_cflag &= ~CSTOPB;
//         options.c_cflag &= ~CSIZE;
//         options.c_cflag |= CS8;
//         options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
//         options.c_iflag &= ~(IXON | IXOFF | IXANY);
//         options.c_oflag &= ~OPOST;
//         tcsetattr(gps_fd_, TCSANOW, &options);
//         fcntl(gps_fd_, F_SETFL, FNDELAY);
//     }

//     int current_route_idx_ = 3;
//     int map_offset_x_ = 0;
//     int map_offset_y_ = 0;
//     int tick_count_ = 0;
//     std::minstd_rand rng_{0x48504F44};

//     void init_mock_data()
//     {
//         // Keep route/track simple but remove fake GPS/satellite data so UI reflects real parsed data
//         route_points_ = {
//             {34, 106}, {54, 90}, {76, 78}, {98, 72}, {122, 60}, {148, 46}, {172, 40}, {196, 48}, {220, 62}, {246, 70}, {272, 64}};
//         route_line_points_.clear();
//         for (const auto &point : route_points_)
//         {
//             route_line_points_.push_back({(lv_value_precise_t)point.x, (lv_value_precise_t)point.y});
//         }

//         track_points_ = { {34, 106}, {48, 94}, {66, 82}, {85, 76} };
//         rebuild_track_points();

//         // Clear any mocked satellite list so we draw based on real data when available
//         satellites_.clear();

//         // Start with no fix until parser provides real data
//         gps_fixed_ = false;
//         current_lat_ = 0.0f;
//         current_lng_ = 0.0f;
//         current_alt_ = 0.0f;
//         current_speed_ = 0.0f;
//         current_course_ = 0.0f;
//         current_hdop_ = 99.0f;
//     }

//     void rebuild_track_points()
//     {
//         track_line_points_.clear();
//         for (const auto &point : track_points_)
//         {
//             track_line_points_.push_back({(lv_value_precise_t)point.x, (lv_value_precise_t)point.y});
//         }
//     }

//     void creat_UI()
//     {
//         lv_obj_t *bg = lv_obj_create(ui_APP_Container);
//         lv_obj_set_size(bg, 320, 150);
//         lv_obj_set_pos(bg, 0, 0);
//         lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(bg, lv_color_hex(0xF3F6F4), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
//         ui_obj_["bg"] = bg;

//         lv_obj_t *top = lv_obj_create(bg);
//         lv_obj_set_size(top, 320, 20);
//         lv_obj_set_pos(top, 0, 0);
//         lv_obj_set_style_radius(top, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(top, lv_color_hex(0x3F8F6B), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(top, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(top, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_pad_left(top, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

//         lv_obj_t *title = lv_label_create(top);
//         lv_label_set_text(title, "HikePod Pseudo App");
//         lv_obj_set_align(title, LV_ALIGN_LEFT_MID);
//         lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

//         lv_obj_t *hint = lv_label_create(top);
//         lv_label_set_text(hint, "TAB switch  ESC back");
//         lv_obj_set_align(hint, LV_ALIGN_RIGHT_MID);
//         lv_obj_set_x(hint, -5);
//         lv_obj_set_style_text_color(hint, lv_color_hex(0xE8FFF5), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

//         lv_obj_t *nav = lv_obj_create(bg);
//         lv_obj_set_size(nav, 320, 130);
//         lv_obj_set_pos(nav, 0, 20);
//         lv_obj_set_style_radius(nav, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(nav, lv_color_hex(0xF3F6F4), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(nav, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(nav, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_pad_all(nav, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);
//         ui_obj_["nav_view"] = nav;

//         lv_obj_t *map = lv_obj_create(nav);
//         lv_obj_set_size(map, 214, 108);
//         lv_obj_set_pos(map, 8, 8);
//         lv_obj_set_style_radius(map, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(map, lv_color_hex(0xDDEADF), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(map, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(map, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_color(map, lv_color_hex(0x88A097), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_pad_all(map, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(map, LV_OBJ_FLAG_SCROLLABLE);
//         ui_obj_["map"] = map;

//         lv_obj_t *route = lv_line_create(map);
//         lv_obj_set_style_line_color(route, lv_color_hex(0x4A6FA5), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_line_width(route, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_line_rounded(route, true, LV_PART_MAIN | LV_STATE_DEFAULT);
//         ui_obj_["route_line"] = route;

//         lv_obj_t *track = lv_line_create(map);
//         lv_obj_set_style_line_color(track, lv_color_hex(0xCC4B37), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_line_width(track, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_line_rounded(track, true, LV_PART_MAIN | LV_STATE_DEFAULT);
//         ui_obj_["track_line"] = track;

//         lv_obj_t *current = lv_obj_create(map);
//         lv_obj_set_size(current, 8, 8);
//         lv_obj_set_style_radius(current, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(current, lv_color_hex(0x18A558), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(current, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(current, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_color(current, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(current, LV_OBJ_FLAG_SCROLLABLE);
//         ui_obj_["current_dot"] = current;

//         lv_obj_t *start = lv_obj_create(map);
//         lv_obj_set_size(start, 8, 8);
//         lv_obj_set_style_radius(start, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(start, lv_color_hex(0x355C4B), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(start, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(start, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_color(start, lv_color_hex(0xF6FFF9), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(start, LV_OBJ_FLAG_SCROLLABLE);
//         ui_obj_["start_dot"] = start;

//         lv_obj_t *right_panel = lv_obj_create(nav);
//         lv_obj_set_size(right_panel, 86, 108);
//         lv_obj_set_pos(right_panel, 226, 8);
//         lv_obj_set_style_radius(right_panel, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(right_panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(right_panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(right_panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_color(right_panel, lv_color_hex(0xD8E1DC), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_pad_all(right_panel, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

//         const char *card_titles[3] = {"GPS", "Zoom", "Track"};
//         for (int i = 0; i < 3; ++i)
//         {
//             lv_obj_t *card = lv_obj_create(right_panel);
//             lv_obj_set_size(card, 72, 28);
//             lv_obj_set_pos(card, 1, 2 + i * 34);
//             lv_obj_set_style_radius(card, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_bg_color(card, lv_color_hex(0xF8FBF9), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_border_width(card, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_border_color(card, lv_color_hex(0xE2EAE5), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_pad_all(card, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

//             lv_obj_t *lbl_title = lv_label_create(card);
//             lv_label_set_text(lbl_title, card_titles[i]);
//             lv_obj_set_pos(lbl_title, 4, 2);
//             lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x6A7D75), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

//             lv_obj_t *lbl_val = lv_label_create(card);
//             lv_obj_set_pos(lbl_val, 4, 14);
//             lv_obj_set_style_text_color(lbl_val, lv_color_hex(0x17251F), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
//             ui_obj_[std::string("nav_card_") + std::to_string(i)] = lbl_val;
//         }

//         lv_obj_t *footer = lv_obj_create(nav);
//         lv_obj_set_size(footer, 304, 10);
//         lv_obj_set_pos(footer, 8, 119);
//         lv_obj_set_style_radius(footer, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(footer, lv_color_hex(0xEEF4F1), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(footer, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(footer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_pad_all(footer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

//         lv_obj_t *footer_label = lv_label_create(footer);
//         lv_label_set_text(footer_label, "RenderEngine / KMLParser / Tracking / WiFi surfaces");
//         lv_obj_set_align(footer_label, LV_ALIGN_CENTER);
//         lv_obj_set_style_text_color(footer_label, lv_color_hex(0x4F6258), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_text_font(footer_label, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

//         lv_obj_t *gps = lv_obj_create(bg);
//         lv_obj_set_size(gps, 320, 140);
//         lv_obj_set_pos(gps, 0, 20);
//         lv_obj_set_style_radius(gps, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(gps, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(gps, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(gps, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_pad_all(gps, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(gps, LV_OBJ_FLAG_SCROLLABLE);
//         ui_obj_["gps_view"] = gps;

//         lv_obj_t *gps_header = lv_obj_create(gps);
//         lv_obj_set_size(gps_header, 318, 13);
//         lv_obj_set_pos(gps_header, 1, 1);
//         lv_obj_set_style_radius(gps_header, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(gps_header, lv_color_hex(0x00B050), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(gps_header, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(gps_header, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(gps_header, LV_OBJ_FLAG_SCROLLABLE);

//         lv_obj_t *gps_header_lbl = lv_label_create(gps_header);
//         lv_label_set_text(gps_header_lbl, "      -= Cardputer GPS Info =-");
//         lv_obj_set_align(gps_header_lbl, LV_ALIGN_LEFT_MID);
//         lv_obj_set_x(gps_header_lbl, 4);
//         lv_obj_set_style_text_color(gps_header_lbl, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_text_font(gps_header_lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

//         lv_obj_t *gps_sub = lv_obj_create(gps);
//         lv_obj_set_size(gps_sub, 318, 13);
//         lv_obj_set_pos(gps_sub, 1, 13);
//         lv_obj_set_style_radius(gps_sub, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(gps_sub, lv_color_hex(0x00B050), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(gps_sub, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(gps_sub, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(gps_sub, LV_OBJ_FLAG_SCROLLABLE);

//         lv_obj_t *gps_sub_lbl = lv_label_create(gps_sub);
//         lv_label_set_text(gps_sub_lbl, "[s]On/Off [c]Config [h]Help [Tab]Mode");
//         lv_obj_set_align(gps_sub_lbl, LV_ALIGN_LEFT_MID);
//         lv_obj_set_x(gps_sub_lbl, 4);
//         lv_obj_set_style_text_color(gps_sub_lbl, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_text_font(gps_sub_lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

//         const char *c1labels[8] = {"Lat", "Lng", "Alt", "Spd", "Crs", "Date", "Time", "HDOP"};
//         const char *c2labels[8] = {"Seen", "Visb", "Used", "InFx", "GPS", "Gln", "Gal", "BDo"};
//         for (int i = 0; i < 8; ++i)
//         {
//             lv_obj_t *box = lv_obj_create(gps);
//             lv_obj_set_size(box, 100, 13);
//             lv_obj_set_pos(box, 1, 26 + i * 12);
//             lv_obj_set_style_radius(box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_bg_color(box, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_bg_opa(box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_border_width(box, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_border_color(box, lv_color_hex(0x505050), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

//             lv_obj_t *lbl = lv_label_create(box);
//             lv_label_set_text(lbl, c1labels[i]);
//             lv_obj_set_align(lbl, LV_ALIGN_LEFT_MID);
//             lv_obj_set_x(lbl, 4);
//             lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
//             ui_obj_[std::string("gps_col1_") + std::to_string(i)] = lbl;
//         }
//         for (int i = 0; i < 8; ++i)
//         {
//             lv_obj_t *box = lv_obj_create(gps);
//             lv_obj_set_size(box, 80, 13);
//             lv_obj_set_pos(box, 102, 26 + i * 12);
//             lv_obj_set_style_radius(box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_bg_color(box, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_bg_opa(box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_border_width(box, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_border_color(box, lv_color_hex(0x505050), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

//             lv_obj_t *lbl = lv_label_create(box);
//             lv_label_set_text(lbl, c2labels[i]);
//             lv_obj_set_align(lbl, LV_ALIGN_LEFT_MID);
//             lv_obj_set_x(lbl, 4);
//             lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
//             ui_obj_[std::string("gps_col2_") + std::to_string(i)] = lbl;
//         }

//         lv_obj_t *sky = lv_obj_create(gps);
//         lv_obj_set_size(sky, 96, 96);
//         lv_obj_set_pos(sky, 220, 27);
//         lv_obj_set_style_radius(sky, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(sky, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(sky, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(sky, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_color(sky, lv_color_hex(0x505050), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_pad_all(sky, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(sky, LV_OBJ_FLAG_SCROLLABLE);
//         ui_obj_["sky_plot"] = sky;

//         lv_obj_t *status = lv_obj_create(gps);
//         lv_obj_set_size(status, 318, 13);
//         lv_obj_set_pos(status, 1, 126);
//         lv_obj_set_style_radius(status, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(status, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(status, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(status, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_color(status, lv_color_hex(0x505050), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(status, LV_OBJ_FLAG_SCROLLABLE);

//         lv_obj_t *status_lbl = lv_label_create(status);
//         lv_label_set_text(status_lbl, "GP:On Rx:15 Tx:13 Bd:115200");
//         lv_obj_set_align(status_lbl, LV_ALIGN_LEFT_MID);
//         lv_obj_set_x(status_lbl, 4);
//         lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
//         ui_obj_["gps_status"] = status_lbl;

//         update_visibility();
//         sync_nav_view();
//         sync_gps_view();
//     }

//     void update_visibility()
//     {
//         lv_obj_t *nav = ui_obj_["nav_view"];
//         lv_obj_t *gps = ui_obj_["gps_view"];
//         if (view_mode_ == ViewMode::NAV)
//         {
//             lv_obj_clear_flag(nav, LV_OBJ_FLAG_HIDDEN);
//             lv_obj_add_flag(gps, LV_OBJ_FLAG_HIDDEN);
//         }
//         else
//         {
//             lv_obj_add_flag(nav, LV_OBJ_FLAG_HIDDEN);
//             lv_obj_clear_flag(gps, LV_OBJ_FLAG_HIDDEN);
//         }
//     }

//     void event_handler_init()
//     {
//         lv_obj_add_event_cb(ui_root, UIHikePodPage::static_lvgl_handler, LV_EVENT_ALL, this);
//     }

//     static void static_lvgl_handler(lv_event_t *e)
//     {
//         auto *self = static_cast<UIHikePodPage *>(lv_event_get_user_data(e));
//         if (self)
//         {
//             self->event_handler(e);
//         }
//     }

//     void event_handler(lv_event_t *e)
//     {
//         if (!IS_KEY_PRESSED(e) && !IS_KEY_RELEASED(e))
//         {
//             return;
//         }

//         uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
//         if (key == KEY_ESC && IS_KEY_RELEASED(e))
//         {
//             if (go_back_home)
//             {
//                 go_back_home();
//             }
//             return;
//         }

//         if (key == KEY_SWITCH_VIEW && IS_KEY_RELEASED(e))
//         {
//             view_mode_ = (view_mode_ == ViewMode::NAV) ? ViewMode::GPS_INFO : ViewMode::NAV;
//             update_visibility();
//             return;
//         }

//         if (view_mode_ == ViewMode::NAV && IS_KEY_RELEASED(e))
//         {
//             switch (key)
//             {
//             case KEY_LEFT_CODE:
//                 map_offset_x_ = std::max(map_offset_x_ - 4, -20);
//                 break;
//             case KEY_RIGHT_CODE:
//                 map_offset_x_ = std::min(map_offset_x_ + 4, 20);
//                 break;
//             case KEY_UP_CODE:
//                 map_offset_y_ = std::max(map_offset_y_ - 4, -16);
//                 break;
//             case KEY_DOWN_CODE:
//                 map_offset_y_ = std::min(map_offset_y_ + 4, 16);
//                 break;
//             default:
//                 break;
//             }
//             sync_nav_view();
//         }
//     }

//     static void static_tick_cb(lv_timer_t *t)
//     {
//         auto *self = static_cast<UIHikePodPage *>(lv_timer_get_user_data(t));
//         if (self)
//         {
//             self->tick();
//         }
//     }

//     void tick()
//     {
//         ++tick_count_;

//         // Read real GPS data from /dev/ttyS0
//         if (gps_fd_ != -1)
//         {
//             char buffer[256];
//             ssize_t bytes_read = read(gps_fd_, buffer, sizeof(buffer));
//             if (bytes_read > 0)
//             {
//                 for (int i = 0; i < bytes_read; ++i)
//                 {
//                     char c = buffer[i];
//                     // Feed TinyGPS++ parser as before
//                     gps_.encode(c);

//                     // Accumulate into a line buffer and process complete NMEA lines
//                     nmea_line_buffer_.push_back(c);
//                     if (c == '\n')
//                     {
//                         // extract line and trim CR/LF
//                         std::string line = nmea_line_buffer_;
//                         nmea_line_buffer_.clear();
//                         while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
//                             line.pop_back();

//                         // Detect antenna open message
//                         if (line.find("ANTENNA OPEN") != std::string::npos)
//                         {
//                             gps_fixed_ = false;
//                             current_hdop_ = 25.5f;
//                             // mark satellites as not visible/used
//                             for (auto &sat : satellites_)
//                             {
//                                 sat.visible = false;
//                                 sat.used = false;
//                                 sat.snr = 0;
//                             }
//                         }

//                         // Detect GNGGA no-fix pattern: contains ",,0,00,"
//                         if (line.rfind("$GNGGA", 0) == 0)
//                         {
//                             if (line.find(",,0,00,") != std::string::npos)
//                             {
//                                 gps_fixed_ = false;
//                                 current_hdop_ = 25.5f;
//                                 for (auto &sat : satellites_)
//                                 {
//                                     sat.visible = false;
//                                     sat.used = false;
//                                     sat.snr = 0;
//                                 }
//                             }
//                         }
//                     }
//                 }

//                 // After processing raw lines, update TinyGPS++ derived values if available
//                 if (gps_.location.isUpdated())
//                 {
//                     current_lat_ = (float)gps_.location.lat();
//                     current_lng_ = (float)gps_.location.lng();
//                     gps_fixed_ = gps_.location.isValid();
//                 }
//                 if (gps_.altitude.isUpdated())
//                 {
//                     current_alt_ = (float)gps_.altitude.meters();
//                 }
//                 if (gps_.speed.isUpdated())
//                 {
//                     current_speed_ = (float)gps_.speed.kmph();
//                 }
//                 if (gps_.course.isUpdated())
//                 {
//                     current_course_ = (float)gps_.course.deg();
//                 }
//                 if (gps_.hdop.isUpdated())
//                 {
//                     current_hdop_ = (float)gps_.hdop.hdop();
//                 }
//             }
//         }

//         if (current_route_idx_ < (int)route_points_.size() - 1 && (tick_count_ % 3) == 0)
//         {
//             ++current_route_idx_;
//             track_points_.push_back(route_points_[current_route_idx_]);
//             if ((int)track_points_.size() > 9)
//             {
//                 track_points_.erase(track_points_.begin());
//             }
//             rebuild_track_points();
//         }
//         if (current_route_idx_ >= (int)route_points_.size() - 1 && (tick_count_ % 16) == 0)
//         {
//             current_route_idx_ = 3;
//             track_points_.clear();
//             track_points_.push_back(route_points_[0]);
//             track_points_.push_back(route_points_[1]);
//             track_points_.push_back(route_points_[2]);
//             track_points_.push_back(route_points_[3]);
//             rebuild_track_points();
//         }

//         if ((tick_count_ % 20) == 0 && battery_level_ > 48)
//         {
//             --battery_level_;
//         }
//         zoom_level_ = 7 + ((tick_count_ / 12) % 2);

//         for (auto &sat : satellites_)
//         {
//             int snr_delta = (int)(rng_() % 5) - 2;
//             sat.snr = std::max(0, std::min(42, sat.snr + snr_delta));
//             sat.visible = (sat.snr > 0);
//             sat.used = sat.visible && sat.snr >= 25;
//         }

//         sync_nav_view();
//         sync_gps_view();
//     }

//     void sync_nav_view()
//     {
//         if (route_line_points_.size() >= 2)
//         {
//             lv_line_set_points(ui_obj_["route_line"], route_line_points_.data(), (uint16_t)route_line_points_.size());
//         }
//         if (track_line_points_.size() >= 2)
//         {
//             lv_line_set_points(ui_obj_["track_line"], track_line_points_.data(), (uint16_t)track_line_points_.size());
//         }

//         const RoutePoint &start = route_points_.front();
//         lv_obj_set_pos(ui_obj_["start_dot"], start.x - 4 + map_offset_x_, start.y - 4 + map_offset_y_);

//         const RoutePoint &current = route_points_[current_route_idx_];
//         lv_obj_set_pos(ui_obj_["current_dot"], current.x - 4 + map_offset_x_, current.y - 4 + map_offset_y_);

//         char buf[64];
//         lv_snprintf(buf, sizeof(buf), "%s", gps_fixed_ ? "Fixed" : "Search");
//         lv_label_set_text(ui_obj_["nav_card_0"], buf);
//         lv_snprintf(buf, sizeof(buf), "Z%d / 5km", zoom_level_);
//         lv_label_set_text(ui_obj_["nav_card_1"], buf);
//         lv_snprintf(buf, sizeof(buf), "%d pts", (int)track_points_.size());
//         lv_label_set_text(ui_obj_["nav_card_2"], buf);
//     }

//     void sync_gps_view()
//     {
//         char buf[64];
//         const char *c1labels[8] = {"Lat", "Lng", "Alt", "Spd", "Crs", "Date", "Time", "HDOP"};
//         const char *c2labels[8] = {"Seen", "Visb", "Used", "InFx", "GPS", "Gln", "Gal", "BDo"};

//         const char *values1[8] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
//         char storage1[8][32];
//         // Use parsed TinyGPS++ values when available, otherwise show placeholders
//         if (gps_.location.isValid())
//         {
//             lv_snprintf(storage1[0], sizeof(storage1[0]), "%.5f", (double)gps_.location.lat());
//             lv_snprintf(storage1[1], sizeof(storage1[1]), "%.5f", (double)gps_.location.lng());
//         }
//         else
//         {
//             lv_snprintf(storage1[0], sizeof(storage1[0]), "--");
//             lv_snprintf(storage1[1], sizeof(storage1[1]), "--");
//         }

//         if (gps_.altitude.isValid())
//             lv_snprintf(storage1[2], sizeof(storage1[2]), "%.1fm", (double)gps_.altitude.meters());
//         else
//             lv_snprintf(storage1[2], sizeof(storage1[2]), "--");

//         if (gps_.speed.isValid())
//             lv_snprintf(storage1[3], sizeof(storage1[3]), "%.1fkm", (double)gps_.speed.kmph());
//         else
//             lv_snprintf(storage1[3], sizeof(storage1[3]), "--");

//         if (gps_.course.isValid())
//             lv_snprintf(storage1[4], sizeof(storage1[4]), "%.0f", (double)gps_.course.deg());
//         else
//             lv_snprintf(storage1[4], sizeof(storage1[4]), "--");

//         if (gps_.date.isValid())
//             lv_snprintf(storage1[5], sizeof(storage1[5]), "%02u/%02u/%02u", (unsigned)gps_.date.day(), (unsigned)gps_.date.month(), (unsigned)(gps_.date.year() % 100));
//         else
//             lv_snprintf(storage1[5], sizeof(storage1[5]), "--");

//         if (gps_.time.isValid())
//             lv_snprintf(storage1[6], sizeof(storage1[6]), "%02u:%02u:%02u", (unsigned)gps_.time.hour(), (unsigned)gps_.time.minute(), (unsigned)gps_.time.second());
//         else
//             lv_snprintf(storage1[6], sizeof(storage1[6]), "--");

//         if (gps_.hdop.isValid())
//             lv_snprintf(storage1[7], sizeof(storage1[7]), "%.1f", (double)gps_.hdop.hdop());
//         else
//             lv_snprintf(storage1[7], sizeof(storage1[7]), "--");
//         for (int i = 0; i < 8; ++i)
//         {
//             lv_snprintf(buf, sizeof(buf), "%s: %s", c1labels[i], storage1[i]);
//             lv_label_set_text(ui_obj_[std::string("gps_col1_") + std::to_string(i)], buf);
//         }

//         // Use TinyGPS++ parsed satellite counts where possible
//         int total_seen = 0;
//         int total_used = 0;
//         int gps_sys = 0;
//         int gln_sys = 0;
//         int gal_sys = 0;
//         int bdo_sys = 0;

//         // TinyGPSPlus exposes a satellites member (number of satellites). Use that as seen/used proxy.
//         if (gps_.satellites.isValid())
//         {
//             total_seen = gps_.satellites.value();
//             // When we have a fix, assume these are used; otherwise none used
//             total_used = gps_fixed_ ? total_seen : 0;
//         }

//         int vals2[8] = { total_seen, total_seen, total_used, gps_fixed_ ? 1 : 0, gps_sys, gln_sys, gal_sys, bdo_sys };
//         for (int i = 0; i < 8; ++i)
//         {
//             lv_snprintf(buf, sizeof(buf), "%s: %d", c2labels[i], vals2[i]);
//             lv_label_set_text(ui_obj_[std::string("gps_col2_") + std::to_string(i)], buf);
//         }
//         lv_snprintf(buf, sizeof(buf), "GP:%s Rx:15 Tx:13 Bd:115200", gps_fixed_ ? "On" : "Err");
//         lv_label_set_text(ui_obj_["gps_status"], buf);

//         lv_obj_t *sky = ui_obj_["sky_plot"];
//         lv_obj_clean(sky);
//         lv_obj_set_style_bg_color(sky, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(sky, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_color(sky, lv_color_hex(0x505050), LV_PART_MAIN | LV_STATE_DEFAULT);

//         lv_obj_t *ring1 = lv_obj_create(sky);
//         lv_obj_set_size(ring1, 92, 92);
//         lv_obj_set_pos(ring1, 2, 2);
//         lv_obj_set_style_radius(ring1, 46, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(ring1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(ring1, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_color(ring1, lv_color_hex(0xBDBDBD), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(ring1, LV_OBJ_FLAG_SCROLLABLE);

//         lv_obj_t *ring2 = lv_obj_create(sky);
//         lv_obj_set_size(ring2, 60, 60);
//         lv_obj_set_pos(ring2, 18, 18);
//         lv_obj_set_style_radius(ring2, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(ring2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(ring2, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_color(ring2, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(ring2, LV_OBJ_FLAG_SCROLLABLE);

//         lv_obj_t *ring3 = lv_obj_create(sky);
//         lv_obj_set_size(ring3, 30, 30);
//         lv_obj_set_pos(ring3, 33, 33);
//         lv_obj_set_style_radius(ring3, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_opa(ring3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(ring3, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_color(ring3, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(ring3, LV_OBJ_FLAG_SCROLLABLE);

//         lv_obj_t *hline = lv_obj_create(sky);
//         lv_obj_set_size(hline, 92, 1);
//         lv_obj_set_pos(hline, 2, 47);
//         lv_obj_set_style_radius(hline, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(hline, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(hline, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(hline, LV_OBJ_FLAG_SCROLLABLE);

//         lv_obj_t *vline = lv_obj_create(sky);
//         lv_obj_set_size(vline, 1, 92);
//         lv_obj_set_pos(vline, 47, 2);
//         lv_obj_set_style_radius(vline, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_bg_color(vline, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_set_style_border_width(vline, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
//         lv_obj_clear_flag(vline, LV_OBJ_FLAG_SCROLLABLE);

//         const char *cardinals[4] = {"N", "E", "S", "W"};
//         const int cx = 47;
//         const int cy = 47;
//         const int pos[4][2] = {{44, 4}, {84, 43}, {44, 84}, {4, 43}};
//         for (int i = 0; i < 4; ++i)
//         {
//             lv_obj_t *lbl = lv_label_create(sky);
//             lv_label_set_text(lbl, cardinals[i]);
//             lv_obj_set_pos(lbl, pos[i][0], pos[i][1]);
//             lv_obj_set_style_text_color(lbl, lv_color_hex(0xD0D0D0), LV_PART_MAIN | LV_STATE_DEFAULT);
//             lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
//         }

//         // Note: detailed satellite az/el info is not available via TinyGPS++ here,
//         // so we do not render individual satellite dots unless satellites_ is populated elsewhere.
//     }
// };
#endif // !HAL_PLATFORM_SDL
