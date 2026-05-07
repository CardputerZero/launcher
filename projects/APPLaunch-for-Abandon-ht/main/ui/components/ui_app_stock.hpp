#pragma once
#include "ui_app_page.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <cstdlib>   // rand()
#include <cmath>

extern "C" {
#include "bme68x.h"
}

// ============================================================
//  I2C Adapter for Linux /dev/i2c-x
// ============================================================
static BME68X_INTF_RET_TYPE bme68x_linux_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    int fd = *(int*)intf_ptr;
    if (write(fd, &reg_addr, 1) != 1) return BME68X_E_COM_FAIL;
    if (read(fd, reg_data, len) != (ssize_t)len) return BME68X_E_COM_FAIL;
    return BME68X_OK;
}

static BME68X_INTF_RET_TYPE bme68x_linux_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    int fd = *(int*)intf_ptr;
    uint8_t buffer[len + 1];
    buffer[0] = reg_addr;
    for (uint32_t i = 0; i < len; i++) buffer[i + 1] = reg_data[i];
    if (write(fd, buffer, len + 1) != (ssize_t)(len + 1)) return BME68X_E_COM_FAIL;
    return BME68X_OK;
}

static void bme68x_linux_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    usleep(period);
}

// ============================================================
//  股票界面  UIStockPage
//  屏幕分辨率: 320 x 170  (顶栏20px, ui_APP_Container 320x150)
//
//  视图状态:
//    VIEW_MAIN    — 股票列表（上下选择，每行|头部有 up icon/down icon， 当股票上涨时为绿色，下跌时为红色 |中部为简单的股票折线图|尾部为当前价格+绿红方框中为涨跌幅%值）
// 先生成4个模拟的股票对象，每个对象有名字、当前价格、涨跌幅、涨跌额、折线图数据。（苹果（AAPL）、特斯拉（TSLA）、谷歌（GOOGL）、亚马逊（AMZN））
// ============================================================
// ============================================================
//  股票界面  UIStockPage  （带 1Hz 模拟数据刷新）
//  屏幕分辨率: 320 x 170
// ============================================================

class UIStockPage : public app_base
{
    enum class ViewState { MAIN, SUB };

    static constexpr uint32_t TOGGLE_KEY_CODE = 15;

    struct StockItem
    {
        const char *ticker;
        const char *name;
        float       price;
        float       change_pct;
        float       change_amt;
        std::vector<float> sparkline_raw;      // 8个原始价格点
        std::vector<lv_point_precise_t> sparkline_pts; // LVGL 坐标
        float       reference_price;           // 【新增】前收盘价(基准)
    };


    // BME688传感器对象（I2C地址0x77）
    static constexpr const char* I2C_DEV = "/dev/i2c-1";
    static constexpr int BME688_ADDR = 0x77;
    int i2c_fd_ = -1;
    bool bme688_ok_ = false;

    struct bme68x_dev bme_;
    struct bme68x_conf bme_conf_;
    struct bme68x_heatr_conf bme_heatr_conf_;

public:
    UIStockPage() : app_base()
    {
        i2c_fd_ = open(I2C_DEV, O_RDWR);
        if (i2c_fd_ >= 0 && ioctl(i2c_fd_, I2C_SLAVE, BME688_ADDR) >= 0) {
            bme_.intf = BME68X_I2C_INTF;
            bme_.read = bme68x_linux_i2c_read;
            bme_.write = bme68x_linux_i2c_write;
            bme_.delay_us = bme68x_linux_delay_us;
            bme_.intf_ptr = &i2c_fd_;
            bme_.amb_temp = 25;

            if (bme68x_init(&bme_) == BME68X_OK) {
                bme_conf_.filter = BME68X_FILTER_OFF;
                bme_conf_.odr = BME68X_ODR_NONE;
                bme_conf_.os_hum = BME68X_OS_16X;
                bme_conf_.os_pres = BME68X_OS_1X;
                bme_conf_.os_temp = BME68X_OS_2X;
                bme68x_set_conf(&bme_conf_, &bme_);

                bme_heatr_conf_.enable = BME68X_ENABLE;
                bme_heatr_conf_.heatr_temp = 300;
                bme_heatr_conf_.heatr_dur = 100;
                bme68x_set_heatr_conf(BME68X_FORCED_MODE, &bme_heatr_conf_, &bme_);

                bme688_ok_ = true;
            }
        }
        stock_init();
        creat_UI();
        event_handler_init();
        sim_timer_ = lv_timer_create(sim_timer_cb, 1000, this);
    }

    ~UIStockPage()
    {
        // 【新增】清理定时器
        if (sim_timer_) lv_timer_del(sim_timer_);
    }

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    std::vector<StockItem> stock_items_;
    int   selected_idx_  = 0;
    ViewState view_state_ = ViewState::MAIN;
    lv_timer_t *sim_timer_ = nullptr;  // 【新增】1秒定时器

    float sensor_temperature_ = 24.6f;
    float sensor_pressure_ = 1008.4f;
    float sensor_humidity_ = 46.2f;
    float sensor_iaq_ = 58.0f;
    float sensor_co2eq_ = 672.0f;
    float sensor_voc_ = 0.62f;
    std::vector<float> sensor_curve_raw_;
    std::vector<lv_point_precise_t> sensor_curve_pts_;

    static constexpr int ITEM_H       = 28;
    static constexpr int VISIBLE_ROWS = 4;
    static constexpr int LIST_Y       = 22;
    static constexpr int LIST_H       = 128;
    static constexpr int SPARK_X      = 74;
    static constexpr int SPARK_Y      = 4;
    static constexpr int SPARK_W      = 120;
    static constexpr int SPARK_H      = 20;

    static constexpr int SENSOR_CURVE_X = 12;
    static constexpr int SENSOR_CURVE_Y = 84;
    static constexpr int SENSOR_CURVE_W = 296;
    static constexpr int SENSOR_CURVE_H = 54;

    // ==================== 股票数据初始化 ====================
    void stock_init()
    {
        // 基准价 = 当前价 - 涨跌额（反推前收盘价）
        stock_items_.push_back({"AAPL","Apple Inc.",      178.50f, +2.30f, +4.01f,
            {172,173,175,174,176,177,178,178.5f}, {}, 174.49f});
        stock_items_.push_back({"TSLA","Tesla Inc.",      245.10f, -1.80f, -4.50f,
            {250,248,249,247,246,244,245,245.1f}, {}, 249.60f});
        stock_items_.push_back({"GOOGL","Alphabet Inc.",  141.20f, +0.90f, +1.26f,
            {139,140,141,140.5f,141,140.8f,141.1f,141.2f}, {}, 139.94f});
        stock_items_.push_back({"AMZN","Amazon.com Inc.", 185.75f, +3.50f, +6.28f,
            {180,181,183,182,184,185,185.5f,185.75f}, {}, 179.47f});
        stock_items_.push_back({"NIHAO","Nihao.com Inc.", 185.75f, +3.50f, +6.28f,
            {180,181,183,182,184,185,185.5f,185.75f}, {}, 179.47f});

        // 预计算折线坐标
        for (auto &stock : stock_items_)
            recalc_sparkline(stock);

        sensor_curve_raw_ = {
            sensor_iaq_ - 4.2f,
            sensor_iaq_ - 3.1f,
            sensor_iaq_ - 2.2f,
            sensor_iaq_ - 1.4f,
            sensor_iaq_ - 0.6f,
            sensor_iaq_ + 0.4f,
            sensor_iaq_ + 1.1f,
            sensor_iaq_ + 1.8f,
            sensor_iaq_ + 0.7f,
            sensor_iaq_
        };
        recalc_sensor_curve();
    }

    // 【新增】重新计算单只股票的折线图 LVGL 坐标
    void recalc_sparkline(StockItem &stock)
    {
        const auto &raw = stock.sparkline_raw;
        int cnt = (int)raw.size();
        if (cnt < 2) return;

        float min_v = raw[0], max_v = raw[0];
        for (float v : raw) {
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
        float range = max_v - min_v;
        if (range < 0.001f) range = 1.0f;

        stock.sparkline_pts.resize(cnt);
        for (int i = 0; i < cnt; ++i) {
            lv_value_precise_t x = (lv_value_precise_t)(SPARK_X + (i * (SPARK_W - 1)) / (cnt - 1));
            float t = (raw[i] - min_v) / range;
            lv_value_precise_t y = (lv_value_precise_t)(SPARK_Y + SPARK_H - 1 - t * (SPARK_H - 1));
            stock.sparkline_pts[i] = {x, y};
        }
    }

    // ==================== 【新增】模拟数据刷新 ====================
    static void sim_timer_cb(lv_timer_t *t)
    {
        UIStockPage *self = static_cast<UIStockPage *>(lv_timer_get_user_data(t));
        if (self) self->simulate_tick();
    }

    void simulate_tick()
    {
        // 每只股票的波动率（价格越高波动越大）
        for (auto &stock : stock_items_)
        {
            // 随机扰动：±0.15% ~ ±0.35%
            float sign = (rand() % 2 == 0) ? 1.0f : -1.0f;
            float mag  = (rand() % 200 + 150) / 100000.0f;  // 0.0015 ~ 0.0035
            float delta = stock.price * sign * mag;

            stock.price      += delta;
            stock.change_amt  = stock.price - stock.reference_price;
            stock.change_pct  = (stock.change_amt / stock.reference_price) * 100.0f;

            // 折线图：移除首点，追加最新价
            stock.sparkline_raw.erase(stock.sparkline_raw.begin());
            stock.sparkline_raw.push_back(stock.price);

            recalc_sparkline(stock);
        }

        simulate_sensor_tick();

        // 刷新界面
        build_stock_rows();
        refresh_sensor_view();
    }

    static float clampf(float value, float min_v, float max_v)
    {
        if (value < min_v) return min_v;
        if (value > max_v) return max_v;
        return value;
    }

    void simulate_sensor_tick()
    {
        bool use_real = false;
        if (bme688_ok_ && i2c_fd_ >= 0) {
            bme68x_set_op_mode(BME68X_FORCED_MODE, &bme_);
            uint32_t del_period = bme68x_get_meas_dur(BME68X_FORCED_MODE, &bme_conf_, &bme_) + (bme_heatr_conf_.heatr_dur * 1000);
            bme_.delay_us(del_period, bme_.intf_ptr);

            struct bme68x_data data;
            uint8_t n_fields;
            if (bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &bme_) == BME68X_OK && n_fields > 0) {
                sensor_temperature_ = data.temperature;
                sensor_pressure_ = data.pressure / 100.0f;
                sensor_humidity_ = data.humidity;
                // IAQ and CO2eq estimation based on gas resistance
                // Note: Real IAQ requires Bosch BSEC library. This is a simplified estimation.
                sensor_iaq_ = 500.0f * (1.0f - (data.gas_resistance / 100000.0f)); 
                if (sensor_iaq_ < 0) sensor_iaq_ = 25.0f;
                sensor_co2eq_ = 400.0f + sensor_iaq_ * 5.0f;
                sensor_voc_ = sensor_iaq_ / 100.0f;
                
                use_real = true;
            }
        }
        if (!use_real) {
            sensor_temperature_ = clampf(sensor_temperature_ + ((rand() % 41) - 20) * 0.03f, 18.0f, 35.0f);
            sensor_pressure_ = clampf(sensor_pressure_ + ((rand() % 31) - 15) * 0.10f, 980.0f, 1040.0f);
            sensor_humidity_ = clampf(sensor_humidity_ + ((rand() % 31) - 15) * 0.15f, 20.0f, 85.0f);
            float iaq_step = ((rand() % 41) - 20) * 0.35f;
            sensor_iaq_ = clampf(sensor_iaq_ + iaq_step, 5.0f, 350.0f);
            sensor_co2eq_ = clampf(sensor_co2eq_ + iaq_step * 8.5f, 400.0f, 2600.0f);
            sensor_voc_ = clampf(sensor_voc_ + ((rand() % 31) - 15) * 0.01f, 0.05f, 4.0f);
        }
        if (!sensor_curve_raw_.empty()) {
            sensor_curve_raw_.erase(sensor_curve_raw_.begin());
        }
        sensor_curve_raw_.push_back(sensor_iaq_);
        recalc_sensor_curve();
    }

    void recalc_sensor_curve()
    {
        int cnt = (int)sensor_curve_raw_.size();
        if (cnt < 2) return;

        float min_v = sensor_curve_raw_[0];
        float max_v = sensor_curve_raw_[0];
        for (float v : sensor_curve_raw_) {
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
        float range = max_v - min_v;
        if (range < 0.001f) range = 1.0f;

        sensor_curve_pts_.resize(cnt);
        for (int i = 0; i < cnt; ++i) {
            lv_value_precise_t x = (lv_value_precise_t)(SENSOR_CURVE_X + (i * (SENSOR_CURVE_W - 1)) / (cnt - 1));
            float t = (sensor_curve_raw_[i] - min_v) / range;
            lv_value_precise_t y = (lv_value_precise_t)(SENSOR_CURVE_Y + SENSOR_CURVE_H - 1 - t * (SENSOR_CURVE_H - 1));
            sensor_curve_pts_[i] = {x, y};
        }
    }

    // ==================== UI 构建（主视图） ====================
    void creat_UI()
    {
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, 320, 150);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        // 标题栏
        lv_obj_t *title_bar = lv_obj_create(bg);
        lv_obj_set_size(title_bar, 320, 22);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title_bar, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_title = lv_label_create(title_bar);
        lv_label_set_text(lbl_title, "\xEF\x80\xB3  Stocks");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, "TAB(15):switch  ESC:back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // 列表容器
        lv_obj_t *list_cont = lv_obj_create(bg);
        lv_obj_set_size(list_cont, 320, LIST_H);
        lv_obj_set_pos(list_cont, 0, LIST_Y);
        lv_obj_set_style_radius(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["list_cont"] = list_cont;

        lv_obj_t *sensor_cont = lv_obj_create(bg);
        lv_obj_set_size(sensor_cont, 320, LIST_H);
        lv_obj_set_pos(sensor_cont, 0, LIST_Y);
        lv_obj_set_style_radius(sensor_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(sensor_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(sensor_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(sensor_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(sensor_cont, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["sensor_cont"] = sensor_cont;

        build_stock_rows();
        build_sensor_view();
        update_view_visibility();
    }

    void update_view_visibility()
    {
        lv_obj_t *list_cont = ui_obj_["list_cont"];
        lv_obj_t *sensor_cont = ui_obj_["sensor_cont"];
        if (!list_cont || !sensor_cont) return;

        if (view_state_ == ViewState::MAIN) {
            lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(sensor_cont, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(list_cont, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(sensor_cont, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void build_sensor_view()
    {
        lv_obj_t *sensor_cont = ui_obj_["sensor_cont"];
        if (!sensor_cont) return;

        lv_obj_t *grid = lv_obj_create(sensor_cont);
        lv_obj_set_size(grid, 312, 76);
        lv_obj_set_pos(grid, 4, 2);
        lv_obj_set_style_radius(grid, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(grid, lv_color_hex(0x161B22), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(grid, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(grid, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(grid, lv_color_hex(0x30363D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(grid, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

        const char *names[6] = {
            "Temperature",
            "Pressure",
            "Humidity",
            "IAQ",
            "CO2 Equivalent",
            "Breath VOC Eq"
        };

        const int cell_w = 102;
        const int cell_h = 36;
        for (int i = 0; i < 6; ++i) {
            int col = i % 3;
            int row = i / 3;
            int x = col * cell_w + 2;
            int y = row * cell_h + 1;

            lv_obj_t *label_name = lv_label_create(grid);
            lv_label_set_text(label_name, names[i]);
            lv_obj_set_pos(label_name, x, y);
            lv_obj_set_style_text_color(label_name, lv_color_hex(0x7D8590), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(label_name, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_t *label_val = lv_label_create(grid);
            lv_obj_set_pos(label_val, x, y + 14);
            lv_obj_set_style_text_color(label_val, lv_color_hex(0xE6EDF3), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(label_val, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

            switch (i) {
            case 0: ui_obj_["sensor_temperature"] = label_val; break;
            case 1: ui_obj_["sensor_pressure"] = label_val; break;
            case 2: ui_obj_["sensor_humidity"] = label_val; break;
            case 3: ui_obj_["sensor_iaq"] = label_val; break;
            case 4: ui_obj_["sensor_co2eq"] = label_val; break;
            case 5: ui_obj_["sensor_voc"] = label_val; break;
            default: break;
            }
        }

        lv_obj_t *curve_box = lv_obj_create(sensor_cont);
        lv_obj_set_size(curve_box, 312, 62);
        lv_obj_set_pos(curve_box, 4, 82);
        lv_obj_set_style_radius(curve_box, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(curve_box, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(curve_box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(curve_box, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(curve_box, lv_color_hex(0x30363D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(curve_box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(curve_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *curve_title = lv_label_create(curve_box);
        lv_label_set_text(curve_title, "IAQ Trend (simulated)");
        lv_obj_set_pos(curve_title, 6, 2);
        lv_obj_set_style_text_color(curve_title, lv_color_hex(0x7D8590), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(curve_title, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *line = lv_line_create(sensor_cont);
        lv_obj_set_style_line_color(line, lv_color_hex(0x2ECC71), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_width(line, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN | LV_STATE_DEFAULT);
        ui_obj_["sensor_curve"] = line;

        refresh_sensor_view();
    }

    void refresh_sensor_view()
    {
        char buf[32];

        lv_snprintf(buf, sizeof(buf), "%.2f C", (double)sensor_temperature_);
        lv_label_set_text(ui_obj_["sensor_temperature"], buf);

        lv_snprintf(buf, sizeof(buf), "%.1f hPa", (double)sensor_pressure_);
        lv_label_set_text(ui_obj_["sensor_pressure"], buf);

        lv_snprintf(buf, sizeof(buf), "%.1f %%", (double)sensor_humidity_);
        lv_label_set_text(ui_obj_["sensor_humidity"], buf);

        lv_snprintf(buf, sizeof(buf), "%.1f", (double)sensor_iaq_);
        lv_label_set_text(ui_obj_["sensor_iaq"], buf);

        lv_snprintf(buf, sizeof(buf), "%.0f ppm", (double)sensor_co2eq_);
        lv_label_set_text(ui_obj_["sensor_co2eq"], buf);

        lv_snprintf(buf, sizeof(buf), "%.2f", (double)sensor_voc_);
        lv_label_set_text(ui_obj_["sensor_voc"], buf);

        lv_obj_t *line = ui_obj_["sensor_curve"];
        if (line && !sensor_curve_pts_.empty()) {
            lv_line_set_points(line, sensor_curve_pts_.data(), (uint16_t)sensor_curve_pts_.size());
        }
    }

    // ==================== 构建股票行 ====================
    void build_stock_rows()
    {
        lv_obj_t *list_cont = ui_obj_["list_cont"];
        lv_obj_clean(list_cont);

        int item_count = (int)stock_items_.size();
        int visible = LIST_H / ITEM_H;
        int offset_idx = selected_idx_ - visible / 2;
        if (offset_idx < 0) offset_idx = 0;
        if (offset_idx > item_count - visible) offset_idx = item_count - visible;
        if (offset_idx < 0) offset_idx = 0;

        for (int vi = 0; vi < visible && (vi + offset_idx) < item_count; ++vi)
        {
            int mi = vi + offset_idx;
            bool is_sel = (mi == selected_idx_);
            create_stock_row(list_cont, vi, mi, is_sel);
        }
    }

    void create_stock_row(lv_obj_t *parent, int visual_row, int stock_idx, bool selected)
    {
        const StockItem &stock = stock_items_[stock_idx];
        bool is_up = (stock.change_pct >= 0.0f);

        lv_color_t green    = lv_color_hex(0x2ECC71);
        lv_color_t red      = lv_color_hex(0xE74C3C);
        lv_color_t trend_clr = is_up ? green : red;
        const char *arrow   = is_up ? "\xE2\x96\xB2" : "\xE2\x96\xBC";

        // 行背景
        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_size(row, 318, ITEM_H - 2);
        lv_obj_set_pos(row, 1, visual_row * ITEM_H + 1);
        lv_obj_set_style_radius(row, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        if (selected) {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_t *sel_bar = lv_obj_create(row);
            lv_obj_set_size(sel_bar, 3, ITEM_H - 6);
            lv_obj_set_pos(sel_bar, 2, 2);
            lv_obj_set_style_radius(sel_bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(sel_bar, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(sel_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(sel_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(sel_bar, LV_OBJ_FLAG_SCROLLABLE);
        } else {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x161B22), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        // 分割线
        if (stock_idx < (int)stock_items_.size() - 1) {
            lv_obj_t *div = lv_obj_create(parent);
            lv_obj_set_size(div, 310, 1);
            lv_obj_set_pos(div, 5, (visual_row + 1) * ITEM_H);
            lv_obj_set_style_radius(div, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(div, lv_color_hex(0x21262D), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(div, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(div, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
        }

        // 箭头
        lv_obj_t *lbl_arrow = lv_label_create(row);
        lv_label_set_text(lbl_arrow, arrow);
        lv_obj_set_pos(lbl_arrow, 8, (ITEM_H - 16) / 2);
        lv_obj_set_style_text_color(lbl_arrow, trend_clr, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_arrow, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        // 代码
        lv_obj_t *lbl_ticker = lv_label_create(row);
        lv_label_set_text(lbl_ticker, stock.ticker);
        lv_obj_set_pos(lbl_ticker, 22, (ITEM_H - 14) / 2);
        lv_obj_set_style_text_color(lbl_ticker,
            selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xCCCCCC),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_ticker, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        // 折线基线
        lv_obj_t *baseline = lv_obj_create(row);
        lv_obj_set_size(baseline, SPARK_W, 1);
        lv_obj_set_pos(baseline, SPARK_X, SPARK_Y + SPARK_H / 2);
        lv_obj_set_style_radius(baseline, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(baseline, lv_color_hex(0x21262D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(baseline, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(baseline, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(baseline, LV_OBJ_FLAG_SCROLLABLE);

        // 折线
        if (!stock.sparkline_pts.empty()) {
            lv_obj_t *line = lv_line_create(row);
            lv_line_set_points(line, stock.sparkline_pts.data(),
                               (uint16_t)stock.sparkline_pts.size());
            lv_obj_set_style_line_color(line, trend_clr, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_line_width(line, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        // 价格
        char price_buf[16];
        snprintf(price_buf, sizeof(price_buf), "$%.2f", (double)stock.price);
        lv_obj_t *lbl_price = lv_label_create(row);
        lv_label_set_text(lbl_price, price_buf);
        lv_obj_set_pos(lbl_price, 202, (ITEM_H - 14) / 2);
        lv_obj_set_style_text_color(lbl_price,
            selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xE6EDF3),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_price, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        // 涨跌幅方框
        lv_obj_t *pct_box = lv_obj_create(row);
        lv_obj_set_size(pct_box, 54, 18);
        lv_obj_set_pos(pct_box, 258, (ITEM_H - 18) / 2);
        lv_obj_set_style_radius(pct_box, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(pct_box, trend_clr, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(pct_box, 220, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(pct_box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(pct_box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(pct_box, LV_OBJ_FLAG_SCROLLABLE);

        char pct_buf[16];
        snprintf(pct_buf, sizeof(pct_buf), "%+.2f%%", (double)stock.change_pct);
        lv_obj_t *lbl_pct = lv_label_create(pct_box);
        lv_label_set_text(lbl_pct, pct_buf);
        lv_obj_set_align(lbl_pct, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(lbl_pct, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_pct, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // ==================== 事件绑定 ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIStockPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }
    static void static_lvgl_handler(lv_event_t *e)
    {
        UIStockPage *self = static_cast<UIStockPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }
    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_PRESSED(e))
        {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            if (key == TOGGLE_KEY_CODE)
            {
                view_state_ = (view_state_ == ViewState::MAIN) ? ViewState::SUB : ViewState::MAIN;
                update_view_visibility();
                return;
            }
        }

        if (IS_KEY_RELEASED(e))
        {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            switch (view_state_) {
            case ViewState::MAIN: handle_main_key(key); break;
            case ViewState::SUB:  handle_sub_key(key);  break;
            }
        }
    }

    void handle_main_key(uint32_t key)
    {
        int count = (int)stock_items_.size();
        switch (key) {
        case KEY_UP:
        case KEY_F:
            if (selected_idx_ > 0) { --selected_idx_; build_stock_rows(); }
            break;
        case KEY_DOWN:
        case KEY_X:
            if (selected_idx_ < count - 1) { ++selected_idx_; build_stock_rows(); }
            break;
        case KEY_ESC:
            if (go_back_home) go_back_home();
            break;
        default: break;
        }
    }

    void handle_sub_key(uint32_t key)
    {
        switch (key) {
        case KEY_ESC:
            if (go_back_home) go_back_home();
            break;
        default: break;
        }
    }
};
