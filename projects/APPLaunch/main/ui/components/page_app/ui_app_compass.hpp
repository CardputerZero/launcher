#pragma once

#include "../../ui.h"
#include "../ui_app_page.hpp"
#include "compat/input_keys.h"
#include "hal_lvgl_bsp.h"
#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <functional>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <keyboard_input.h>

/*
 * ============================================================
 *  UICompassPage
 *
 *  Compass + IMU dashboard
 *  Screen: 320 x 170
 *
 *  按键：
 *    F4  预留：校准接口
 *    F6/ESC 返回主页
 * ============================================================
 */
class UICompassPage : public AppPageRoot
{
    static constexpr int kScreenW = 320;
    static constexpr int kScreenH = 170;
    static constexpr int kStatusH = 30;
    static constexpr int kBottomH = 25;
    static constexpr int kBtnW = kScreenW / 5;
    static constexpr int kCompassDia = 116;
    static constexpr int kLevelDia = 100;

    static constexpr uint32_t kColorBg = 0x000000;
    static constexpr uint32_t kColorText = 0xFFFFFF;
    static constexpr uint32_t kColorTextGray = 0xAAAAAA;
    static constexpr uint32_t kColorLevelDisc = 0x222222;
    static constexpr uint32_t kColorLevelBorder = 0x555555;
    static constexpr uint32_t kColorCenterDot = 0x888888;
    static constexpr uint32_t kColorLevelMove = 0xE74C3C;
    static constexpr uint32_t kColorIconList = 0x33CC33;
    static constexpr uint32_t kColorIconExit = 0xFF0000;
    static constexpr uint32_t kColorSensorWarn = 0xFF0000;

    static constexpr const char* ICON_EXIT = "\uEA01"; // .svgfont-exit
    static constexpr const char* ICON_LIST = "\uEA04"; // .svgfont-list

public:
    UICompassPage() : AppPageRoot()
    {
        page_title_ = "Compass";
        svg_font_ = lv_freetype_font_create(
            cp0_file_path("svgfont.ttf").c_str(), LV_FREETYPE_FONT_RENDER_MODE_BITMAP, 16,
            LV_FREETYPE_FONT_STYLE_NORMAL);
        creat_UI();
        event_handler_init();
    }

    ~UICompassPage()
    {
        stop_sensor_thread();
        if (sensor_timer_) {
            lv_timer_delete(sensor_timer_);
            sensor_timer_ = nullptr;
        }
        lv_freetype_font_delete(svg_font_);
    }

private:
    struct IioDevicePaths {
        std::string accel;
        std::string magn;
        bool hasGyro = false;

        bool ready() const
        {
            return !accel.empty() && !magn.empty();
        }
    };

    struct CompassUiState {
        std::string statusText = "Compass";
        float yaw = 0.0f;
        float pitch = 0.0f;
        float roll = 0.0f;
        float accX = 0.0f;
        float accY = 0.0f;
        float accZ = 0.0f;
        float gyrX = 0.0f;
        float gyrY = 0.0f;
        float gyrZ = 0.0f;
        float magX = 0.0f;
        float magY = 0.0f;
        float magZ = 0.0f;
        bool sensorReady = false;
    };

    lv_font_t* svg_font_ = nullptr;

    lv_obj_t* lbl_status_text_ = nullptr;
    lv_obj_t* compass_disc_ = nullptr;
    lv_obj_t* lbl_compass_title_ = nullptr;
    lv_obj_t* lbl_yaw_ = nullptr;
    lv_obj_t* lbl_imu_title_ = nullptr;
    lv_obj_t* level_disc_ = nullptr;
    lv_obj_t* center_dot_ = nullptr;
    lv_obj_t* level_dot_ = nullptr;
    lv_obj_t* lbl_acc_ = nullptr;
    lv_obj_t* lbl_gyr_ = nullptr;
    lv_obj_t* sensor_missing_box_ = nullptr;
    lv_obj_t* sensor_missing_label_ = nullptr;
    std::array<lv_obj_t*, 5> lbl_bottom_btns_{};
    std::array<lv_obj_t*, 5> lbl_bottom_indicators_{};

    lv_timer_t* sensor_timer_ = nullptr;
    std::thread sensor_thread_;
    std::atomic<bool> sensor_running_{false};
    std::mutex sensor_mutex_;
    CompassUiState sensor_state_{};
    bool sensor_state_dirty_ = false;
    CompassUiState last_state_{};

    static lv_color_t color(uint32_t hex)
    {
        return lv_color_hex(hex);
    }

    /*
     * ============================================================
     * UI 构建
     * ============================================================
     */
    void creat_UI()
    {
        lv_obj_set_size(root_screen_, kScreenW, kScreenH);
        lv_obj_clear_flag(root_screen_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(root_screen_, color(kColorBg), 0);
        lv_obj_set_style_bg_opa(root_screen_, LV_OPA_COVER, 0);

        create_status_bar(root_screen_);
        create_compass_panel(root_screen_);
        create_imu_panel(root_screen_);
        create_bottom_bar(root_screen_);
        create_sensor_missing_overlay(root_screen_);

        CompassUiState initial_state;
        IioDevicePaths initial_paths = enumerate_iio_devices();
        initial_state.statusText = initial_paths.ready() ? "Sensor starting" : "IIO sensor missing";
        initial_state.sensorReady = initial_paths.ready();
        update_from_state(initial_state);
        start_sensor_thread();
        sensor_timer_ = lv_timer_create(&UICompassPage::sensor_timer_cb, 50, this);
        poll_sensor_once();
    }

    void create_status_bar(lv_obj_t* parent)
    {
        lbl_status_text_ = lv_label_create(parent);
        lv_obj_set_pos(lbl_status_text_, 0, 6);
        lv_obj_set_width(lbl_status_text_, kScreenW);
        lv_obj_set_style_text_font(lbl_status_text_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_status_text_, color(kColorText), 0);
        lv_obj_set_style_text_align(lbl_status_text_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(lbl_status_text_, "Compass");
    }

    void create_compass_panel(lv_obj_t* parent)
    {
        lbl_compass_title_ = lv_label_create(parent);
        lv_label_set_text(lbl_compass_title_, "Compass");
        lv_obj_set_style_text_font(lbl_compass_title_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_compass_title_, color(kColorText), 0);
        lv_obj_set_size(lbl_compass_title_, 160, 16);
        lv_obj_set_style_text_align(lbl_compass_title_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lbl_compass_title_, 0, kStatusH - 20);

        compass_disc_ = lv_img_create(parent);
        lv_img_set_src(compass_disc_, cp0_file_path("compass_disc_transparent.png").c_str());
        lv_obj_set_pos(compass_disc_, 12, kStatusH + 1);
        lv_obj_set_size(compass_disc_, kCompassDia, kCompassDia);
        lv_img_set_pivot(compass_disc_, kCompassDia / 2, kCompassDia / 2);
        lv_obj_clear_flag(compass_disc_, LV_OBJ_FLAG_SCROLLABLE);

        lbl_yaw_ = lv_label_create(parent);
        lv_label_set_text(lbl_yaw_, "---");
        lv_obj_set_style_text_font(lbl_yaw_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_yaw_, color(kColorText), 0);
        lv_obj_set_size(lbl_yaw_, 160, 14);
        lv_obj_set_style_text_align(lbl_yaw_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lbl_yaw_, 0, kScreenH - kBottomH - 24);
    }

    void create_imu_panel(lv_obj_t* parent)
    {
        lbl_imu_title_ = lv_label_create(parent);
        lv_label_set_text(lbl_imu_title_, "IMU");
        lv_obj_set_style_text_font(lbl_imu_title_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_imu_title_, color(kColorText), 0);
        lv_obj_set_size(lbl_imu_title_, 160, 16);
        lv_obj_set_style_text_align(lbl_imu_title_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lbl_imu_title_, 160, kStatusH - 20);

        level_disc_ = lv_obj_create(parent);
        lv_obj_remove_style_all(level_disc_);
        lv_obj_set_size(level_disc_, kLevelDia, kLevelDia);
        lv_obj_set_pos(level_disc_, 190, kStatusH + 2);
        lv_obj_set_style_radius(level_disc_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(level_disc_, color(kColorLevelDisc), 0);
        lv_obj_set_style_bg_opa(level_disc_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(level_disc_, 2, 0);
        lv_obj_set_style_border_color(level_disc_, color(kColorLevelBorder), 0);
        lv_obj_set_style_border_opa(level_disc_, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(level_disc_, 0, 0);
        lv_obj_clear_flag(level_disc_, LV_OBJ_FLAG_SCROLLABLE);

        center_dot_ = lv_obj_create(level_disc_);
        lv_obj_remove_style_all(center_dot_);
        lv_obj_set_size(center_dot_, 8, 8);
        lv_obj_set_pos(center_dot_, 46, 46);
        lv_obj_set_style_radius(center_dot_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(center_dot_, color(kColorCenterDot), 0);
        lv_obj_set_style_bg_opa(center_dot_, LV_OPA_COVER, 0);
        lv_obj_clear_flag(center_dot_, LV_OBJ_FLAG_SCROLLABLE);

        level_dot_ = lv_obj_create(level_disc_);
        lv_obj_remove_style_all(level_dot_);
        lv_obj_set_size(level_dot_, 16, 16);
        lv_obj_set_pos(level_dot_, 42, 42);
        lv_obj_set_style_radius(level_dot_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(level_dot_, color(kColorLevelMove), 0);
        lv_obj_set_style_bg_opa(level_dot_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(level_dot_, 2, 0);
        lv_obj_set_style_border_color(level_dot_, color(kColorText), 0);
        lv_obj_set_style_border_opa(level_dot_, LV_OPA_COVER, 0);
        lv_obj_clear_flag(level_dot_, LV_OBJ_FLAG_SCROLLABLE);

        lbl_acc_ = lv_label_create(parent);
        lv_label_set_text(lbl_acc_, "ACC: --- --- ---");
        lv_obj_set_style_text_font(lbl_acc_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_acc_, color(kColorTextGray), 0);
        lv_obj_set_size(lbl_acc_, 160, 12);
        lv_obj_set_style_text_align(lbl_acc_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lbl_acc_, 160, kScreenH - kBottomH - 36);

        lbl_gyr_ = lv_label_create(parent);
        lv_label_set_text(lbl_gyr_, "GYR: --- --- ---");
        lv_obj_set_style_text_font(lbl_gyr_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_gyr_, color(kColorTextGray), 0);
        lv_obj_set_size(lbl_gyr_, 160, 12);
        lv_obj_set_style_text_align(lbl_gyr_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lbl_gyr_, 160, kScreenH - kBottomH - 24);
    }

    void create_bottom_bar(lv_obj_t* parent)
    {
        for (int i = 0; i < 5; i++) {
            lbl_bottom_btns_[i] = lv_label_create(parent);
            lv_obj_set_pos(lbl_bottom_btns_[i], i * kBtnW, kScreenH - kBottomH - 4);
            lv_obj_set_size(lbl_bottom_btns_[i], kBtnW, kBottomH);
            lv_obj_set_style_text_font(lbl_bottom_btns_[i], &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(lbl_bottom_btns_[i], color(kColorText), 0);
            lv_obj_set_style_text_align(lbl_bottom_btns_[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(lbl_bottom_btns_[i], "--");
            lv_obj_set_style_pad_top(lbl_bottom_btns_[i], 0, 0);
            lv_obj_add_flag(lbl_bottom_btns_[i], LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        }

        for (int i = 0; i < 5; i++) {
            lbl_bottom_indicators_[i] = lv_label_create(parent);
            lv_obj_set_pos(lbl_bottom_indicators_[i], i * kBtnW, kScreenH - 12);
            lv_obj_set_size(lbl_bottom_indicators_[i], kBtnW, 12);
            lv_obj_set_style_text_font(lbl_bottom_indicators_[i], &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(lbl_bottom_indicators_[i], color(kColorText), 0);
            lv_obj_set_style_text_align(lbl_bottom_indicators_[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(lbl_bottom_indicators_[i], "|");
        }

        set_bottom_btn(0, ICON_LIST, true, kColorIconList);
        set_bottom_btn(2, ICON_EXIT, true, kColorIconExit);
    }

    void create_sensor_missing_overlay(lv_obj_t* parent)
    {
        sensor_missing_box_ = lv_obj_create(parent);
        lv_obj_remove_style_all(sensor_missing_box_);
        lv_obj_set_size(sensor_missing_box_, 210, 32);
        lv_obj_set_pos(sensor_missing_box_, 55, 4);
        lv_obj_set_style_bg_color(sensor_missing_box_, color(0x220000), 0);
        lv_obj_set_style_bg_opa(sensor_missing_box_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sensor_missing_box_, 2, 0);
        lv_obj_set_style_border_color(sensor_missing_box_, color(kColorSensorWarn), 0);
        lv_obj_set_style_radius(sensor_missing_box_, 0, 0);
        lv_obj_clear_flag(sensor_missing_box_, LV_OBJ_FLAG_SCROLLABLE);

        sensor_missing_label_ = lv_label_create(sensor_missing_box_);
        lv_label_set_text(sensor_missing_label_, "No sensor device found");
        lv_obj_set_size(sensor_missing_label_, 206, 28);
        lv_obj_set_pos(sensor_missing_label_, 2, 6);
        lv_obj_set_style_text_font(sensor_missing_label_, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(sensor_missing_label_, color(kColorSensorWarn), 0);
        lv_obj_set_style_text_align(sensor_missing_label_, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_add_flag(sensor_missing_box_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(sensor_missing_box_);
    }

    void set_bottom_btn(int idx, const char* text, bool icon, uint32_t hex)
    {
        lv_obj_set_style_text_font(lbl_bottom_btns_[idx],
                                   (icon && svg_font_) ? svg_font_ : &lv_font_montserrat_12,
                                   0);
        lv_obj_set_style_text_color(lbl_bottom_btns_[idx], color(hex), 0);
        lv_label_set_text(lbl_bottom_btns_[idx], text);
    }

    /*
     * ============================================================
     * IIO 驱动枚举与数据读取
     * ============================================================
     */
    static bool file_exists(const std::string& path)
    {
        struct stat st;
        return stat(path.c_str(), &st) == 0;
    }

    static bool read_text_file(const std::string& path, std::string& out)
    {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return false;
        std::getline(ifs, out);
        return true;
    }

    static bool read_float_file(const std::string& path, float& out)
    {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return false;
        ifs >> out;
        return !ifs.fail();
    }

    static float read_float_file_or(const std::string& path, float fallback)
    {
        float v = fallback;
        return read_float_file(path, v) ? v : fallback;
    }

    static bool has_accel_files(const std::string& dir)
    {
        return file_exists(dir + "/in_accel_x_raw") &&
               file_exists(dir + "/in_accel_y_raw") &&
               file_exists(dir + "/in_accel_z_raw");
    }

    static bool has_magn_files(const std::string& dir)
    {
        return file_exists(dir + "/in_magn_x_raw") &&
               file_exists(dir + "/in_magn_y_raw") &&
               file_exists(dir + "/in_magn_z_raw");
    }

    static bool has_gyro_files(const std::string& dir)
    {
        return file_exists(dir + "/in_anglvel_x_raw") &&
               file_exists(dir + "/in_anglvel_y_raw") &&
               file_exists(dir + "/in_anglvel_z_raw");
    }

    static IioDevicePaths enumerate_iio_devices()
    {
        static constexpr const char* kIioRoot = "/sys/bus/iio/devices";
        IioDevicePaths paths;

        DIR* dp = opendir(kIioRoot);
        if (!dp) return paths;

        while (dirent* ent = readdir(dp)) {
            if (std::strncmp(ent->d_name, "iio:device", 10) != 0) continue;

            std::string dir = std::string(kIioRoot) + "/" + ent->d_name;
            if (paths.accel.empty() && has_accel_files(dir)) {
                paths.accel = dir;
                paths.hasGyro = has_gyro_files(dir);
            }
            if (paths.magn.empty() && has_magn_files(dir)) {
                paths.magn = dir;
            }
        }

        closedir(dp);
        return paths;
    }

    bool read_axis_triplet(const std::string& dir, const char* prefix,
                           float scale, float& x, float& y, float& z) const
    {
        float rx = 0.0f;
        float ry = 0.0f;
        float rz = 0.0f;
        if (!read_float_file(dir + "/" + prefix + "_x_raw", rx)) return false;
        if (!read_float_file(dir + "/" + prefix + "_y_raw", ry)) return false;
        if (!read_float_file(dir + "/" + prefix + "_z_raw", rz)) return false;

        x = rx * scale;
        y = ry * scale;
        z = rz * scale;
        return true;
    }

    bool read_iio_state(IioDevicePaths& paths, CompassUiState& state)
    {
        if (!paths.ready()) {
            paths = enumerate_iio_devices();
        }

        if (!paths.ready()) {
            state.statusText = "IIO sensor missing";
            state.sensorReady = false;
            return false;
        }

        const float acc_scale = read_float_file_or(paths.accel + "/in_accel_scale", 1.0f);
        const float gyr_scale = read_float_file_or(paths.accel + "/in_anglvel_scale", 1.0f);
        const float mag_scale = read_float_file_or(paths.magn + "/in_magn_scale", 1.0f);

        float acc_x = 0.0f, acc_y = 0.0f, acc_z = 0.0f;
        float mag_x = 0.0f, mag_y = 0.0f, mag_z = 0.0f;
        float gyr_x = 0.0f, gyr_y = 0.0f, gyr_z = 0.0f;

        if (!read_axis_triplet(paths.accel, "in_accel", acc_scale, acc_x, acc_y, acc_z) ||
            !read_axis_triplet(paths.magn, "in_magn", mag_scale, mag_x, mag_y, mag_z)) {
            state.statusText = "IIO read failed";
            state.sensorReady = false;
            return false;
        }

        if (paths.hasGyro) {
            read_axis_triplet(paths.accel, "in_anglvel", gyr_scale, gyr_x, gyr_y, gyr_z);
        }

        float pitch = std::atan2(-acc_x, std::sqrt(acc_y * acc_y + acc_z * acc_z));
        float roll = std::atan2(acc_y, acc_z);
        float sin_p = std::sin(pitch);
        float cos_p = std::cos(pitch);
        float sin_r = std::sin(roll);
        float cos_r = std::cos(roll);

        float mag_x_h = mag_x * cos_p + mag_z * sin_p;
        float mag_y_h = mag_x * sin_r * sin_p + mag_y * cos_r - mag_z * sin_r * cos_p;
        float yaw = std::atan2(-mag_y_h, mag_x_h) * 180.0f / 3.1415926f;
        if (yaw < 0.0f) yaw += 360.0f;

        state.statusText = "Sensor OK";
        state.sensorReady = true;
        state.yaw = yaw;
        state.pitch = pitch * 180.0f / 3.1415926f;
        state.roll = roll * 180.0f / 3.1415926f;
        state.accX = acc_x;
        state.accY = acc_y;
        state.accZ = acc_z;
        state.gyrX = gyr_x;
        state.gyrY = gyr_y;
        state.gyrZ = gyr_z;
        state.magX = mag_x;
        state.magY = mag_y;
        state.magZ = mag_z;
        return true;
    }

    void start_sensor_thread()
    {
        if (sensor_running_.load()) return;
        sensor_running_ = true;
        sensor_thread_ = std::thread(&UICompassPage::sensor_thread_func, this);
    }

    void stop_sensor_thread()
    {
        sensor_running_ = false;
        if (sensor_thread_.joinable()) {
            sensor_thread_.join();
        }
    }

    void publish_sensor_state(const CompassUiState& state)
    {
        std::lock_guard<std::mutex> lock(sensor_mutex_);
        sensor_state_ = state;
        sensor_state_dirty_ = true;
    }

    void sensor_thread_func()
    {
        IioDevicePaths paths = enumerate_iio_devices();
        CompassUiState state;

        while (sensor_running_.load()) {
            read_iio_state(paths, state);
            publish_sensor_state(state);
            usleep(50000); /* 50 ms => 20 Hz, matching the original Compass app */
        }
    }

    void poll_sensor_once()
    {
        CompassUiState state;
        bool dirty = false;
        {
            std::lock_guard<std::mutex> lock(sensor_mutex_);
            dirty = sensor_state_dirty_;
            if (dirty) {
                state = sensor_state_;
                sensor_state_dirty_ = false;
            }
        }

        if (dirty) {
            update_from_state(state);
        }
    }

    static void sensor_timer_cb(lv_timer_t* t)
    {
        auto* self = static_cast<UICompassPage*>(lv_timer_get_user_data(t));
        if (!self) return;
        self->poll_sensor_once();
    }

    /*
     * ============================================================
     * UI 状态刷新
     * ============================================================
     */
    void update_from_state(const CompassUiState& state)
    {
        char buf[128];

        if (lbl_status_text_) {
            lv_label_set_text(lbl_status_text_, state.statusText.c_str());
        }

        if (sensor_missing_box_) {
            if (state.sensorReady) {
                lv_obj_add_flag(sensor_missing_box_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(sensor_missing_box_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(sensor_missing_box_);
            }
        }

        if (lbl_yaw_) {
            std::snprintf(buf, sizeof(buf), "%.0f deg %s", state.yaw, direction_text(state.yaw));
            lv_label_set_text(lbl_yaw_, state.sensorReady ? buf : "---");
        }

        if (lbl_acc_) {
            std::snprintf(buf, sizeof(buf), "ACC:%6.2f %6.2f %6.2f",
                          state.accX, state.accY, state.accZ);
            lv_label_set_text(lbl_acc_, state.sensorReady ? buf : "ACC: --- --- ---");
        }

        if (lbl_gyr_) {
            std::snprintf(buf, sizeof(buf), "GYR:%6.1f %6.1f %6.1f",
                          state.gyrX, state.gyrY, state.gyrZ);
            lv_label_set_text(lbl_gyr_, state.sensorReady ? buf : "GYR: --- --- ---");
        }

        if (compass_disc_) {
            lv_img_set_angle(compass_disc_, state.sensorReady ? -(int16_t)(state.yaw * 10.0f) : 0);
        }

        update_level_dot(state);
        last_state_ = state;
    }

    const char* direction_text(float yaw) const
    {
        float y = yaw;
        while (y < 0.0f) y += 360.0f;
        while (y >= 360.0f) y -= 360.0f;

        if (y >= 337.5f || y < 22.5f) return "N";
        if (y < 67.5f) return "NE";
        if (y < 112.5f) return "E";
        if (y < 157.5f) return "SE";
        if (y < 202.5f) return "S";
        if (y < 247.5f) return "SW";
        if (y < 292.5f) return "W";
        return "NW";
    }

    void update_level_dot(const CompassUiState& state)
    {
        if (!level_dot_) return;

        constexpr float maxOff = 30.0f;
        float dx = state.sensorReady ? (state.accY / 9.80665f * maxOff) : 0.0f;
        float dy = state.sensorReady ? (state.accX / 9.80665f * maxOff) : 0.0f;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > maxOff) {
            dx = dx / dist * maxOff;
            dy = dy / dist * maxOff;
        }

        lv_obj_set_pos(level_dot_, 50 + static_cast<int>(dx) - 8, 50 + static_cast<int>(dy) - 8);

        bool stable = false;
        if (state.sensorReady && last_state_.sensorReady) {
            stable = (std::fabs(state.accX - last_state_.accX) < 0.2f) &&
                     (std::fabs(state.accY - last_state_.accY) < 0.2f) &&
                     (std::fabs(state.accZ - last_state_.accZ) < 0.2f);
        }
        lv_obj_set_style_bg_color(level_dot_, color(stable ? 0x2ECC71 : kColorLevelMove), 0);
    }

private:
    /*
     * ============================================================
     * 按键事件
     * ============================================================
     */
    void event_handler_init()
    {
        lv_obj_add_event_cb(root_screen_, UICompassPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }

    static void static_lvgl_handler(lv_event_t* e)
    {
        UICompassPage* self = static_cast<UICompassPage*>(lv_event_get_user_data(e));
        if (self) {
            self->event_handler(e);
        }
    }

    void event_handler(lv_event_t* e)
    {
        if (launcher_ui::events::is_key_released(e)) {
            uint32_t key = launcher_ui::events::keyboard_key(e);
            handle_key(key);
        }
    }

    void handle_key(uint32_t key)
    {
        switch (key) {
        case KEY_F4:
            // TODO(compass): 接入接口后触发磁力计/IMU 校准。
            break;

        case KEY_F6:
        case KEY_ESC:
            if (navigate_home) {
                navigate_home();
            }
            break;

        default:
            break;
        }
    }
};
