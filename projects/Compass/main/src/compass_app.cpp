#include "compass_app.h"
#include "bmi270_port.h"
#include "bmm150_aux_port.h"
#include "bmi2.h"
#include "bmi270.h"
#include "bmm150.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <unistd.h>

/* ---------- 内部传感器上下文 ---------- */
struct SensorCtx {
    struct bmi2_dev bmi;
    struct bmm150_dev bmm;
    uint8_t bmm150_addr = 0;
    bool magCalValid = false;
    float magBiasX = 0.0f, magBiasY = 0.0f, magBiasZ = 0.0f;
};

/* ---------- 单位转换 ---------- */
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width)
{
    float half_scale = (float)(1 << bit_width) / 2.0f;
    return (val * g_range * 9.80665f) / half_scale;
}

static float lsb_to_dps(int16_t val, float dps_range, uint8_t bit_width)
{
    float half_scale = (float)(1 << bit_width) / 2.0f;
    return (val * dps_range) / half_scale;
}

/* ---------- BMI270 传感器配置 ---------- */
static int8_t set_accel_config(struct bmi2_dev *dev)
{
    struct bmi2_sens_config config = {0};
    int8_t rslt = bmi2_get_sensor_config(&config, 1, dev);
    if (rslt != BMI2_OK) return rslt;

    config.cfg.acc.odr          = BMI2_ACC_ODR_100HZ;
    config.cfg.acc.range        = BMI2_ACC_RANGE_4G;
    config.cfg.acc.bwp          = BMI2_ACC_NORMAL_AVG4;
    config.cfg.acc.filter_perf  = BMI2_PERF_OPT_MODE;
    return bmi2_set_sensor_config(&config, 1, dev);
}

static int8_t set_gyro_config(struct bmi2_dev *dev)
{
    struct bmi2_sens_config config = {0};
    int8_t rslt = bmi2_get_sensor_config(&config, 1, dev);
    if (rslt != BMI2_OK) return rslt;

    config.cfg.gyr.odr           = BMI2_GYR_ODR_100HZ;
    config.cfg.gyr.range         = BMI2_GYR_RANGE_2000;
    config.cfg.gyr.bwp           = BMI2_GYR_NORMAL_MODE;
    config.cfg.gyr.filter_perf   = BMI2_PERF_OPT_MODE;
    config.cfg.gyr.noise_perf    = BMI2_PERF_OPT_MODE;
    return bmi2_set_sensor_config(&config, 1, dev);
}

/* ---------- AUX 地址穷举扫描 ---------- */
static int8_t aux_scan_for_bmm150(struct bmi2_dev *bmi, uint8_t *found_addr)
{
    int8_t rslt;
    uint8_t test_addrs[] = { 0x10, 0x11, 0x12, 0x13 };

    for (int i = 0; i < 4; i++) {
        uint8_t addr = test_addrs[i];
        struct bmi2_sens_config cfg = {0};

        cfg.type = BMI2_AUX;
        rslt = bmi2_get_sensor_config(&cfg, 1, bmi);
        if (rslt != BMI2_OK) continue;

        cfg.cfg.aux.aux_en          = 1;
        cfg.cfg.aux.manual_en       = 1;
        cfg.cfg.aux.i2c_device_addr = addr;
        cfg.cfg.aux.man_rd_burst    = BMI2_AUX_READ_LEN_3;

        rslt = bmi2_set_sensor_config(&cfg, 1, bmi);
        if (rslt != BMI2_OK) continue;

        uint8_t pwrcntrl = 0x01;
        int8_t w_rslt = bmi2_write_aux_man_mode(BMM150_REG_POWER_CONTROL, &pwrcntrl, 1, bmi);
        if (w_rslt == BMI2_OK) {
            usleep(4000);
        }

        uint8_t chip_id = 0;
        rslt = bmi2_read_aux_man_mode(0x40, &chip_id, 1, bmi);
        if (rslt == BMI2_OK && chip_id == 0x32) {
            *found_addr = addr;
            return BMI2_OK;
        }
    }
    return BMI2_E_DEV_NOT_FOUND;
}

/* ---------- 磁力计 8 字校准 ---------- */
static int mag_calibrate_8figure(struct bmm150_dev *bmm, struct bmi2_dev *bmi,
                                 float *bias_x, float *bias_y, float *bias_z,
                                 std::atomic<bool>* running)
{
    int16_t min_x = INT16_MAX, max_x = INT16_MIN;
    int16_t min_y = INT16_MAX, max_y = INT16_MIN;
    int16_t min_z = INT16_MAX, max_z = INT16_MIN;
    uint32_t samples = 0;
    uint32_t elapsed = 0;
    struct bmm150_mag_data mag;
    int8_t rslt;

    while (elapsed < 5000 && running->load()) {
        rslt = bmm150_read_mag_data(&mag, bmm);
        if (rslt == BMM150_OK) {
            if (mag.x < min_x) min_x = mag.x;
            if (mag.x > max_x) max_x = mag.x;
            if (mag.y < min_y) min_y = mag.y;
            if (mag.y > max_y) max_y = mag.y;
            if (mag.z < min_z) min_z = mag.z;
            if (mag.z > max_z) max_z = mag.z;
            samples++;
        }
        usleep(20000);
        elapsed += 20;
    }

    if (samples < 10) return -1;

    *bias_x = ((float)min_x + (float)max_x) / 2.0f / 16.0f;
    *bias_y = ((float)min_y + (float)max_y) / 2.0f / 16.0f;
    *bias_z = ((float)min_z + (float)max_z) / 2.0f / 16.0f;
    return 0;
}

/* ---------- CompassApp ---------- */

CompassApp::CompassApp() = default;
CompassApp::~CompassApp() { deinit(); }

bool CompassApp::init()
{
    if (running_.load()) return true;

    if (!initSensors()) {
        state_.statusText = "Sensor init failed";
        return false;
    }

    running_ = true;
    sensorThread_ = std::thread(&CompassApp::sensorThreadFunc, this);
    return true;
}

void CompassApp::deinit()
{
    running_ = false;
    if (sensorThread_.joinable()) {
        sensorThread_.join();
    }
    deinitSensors();
}

void CompassApp::setView(ICompassView* view)
{
    view_ = view;
}

void CompassApp::onAction(const std::string& action)
{
    if (action == "calibrate") {
        requestCalibrate_ = true;
    }
}

void CompassApp::poll()
{
    if (stateDirty_.exchange(false)) {
        notifyView();
    }
}

CompassState CompassApp::getState() const
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

void CompassApp::notifyView()
{
    if (!view_) return;
    CompassState s = getState();
    view_->update(s);
}

bool CompassApp::initSensors()
{
    SensorCtx* ctx = new SensorCtx();
    std::memset(ctx, 0, sizeof(SensorCtx));

    int8_t rslt = bmi270_port_init(&ctx->bmi);
    if (rslt != BMI2_OK) {
        delete ctx;
        return false;
    }

    rslt = bmi270_init(&ctx->bmi);
    if (rslt != BMI2_OK) {
        bmi270_port_deinit(&ctx->bmi);
        delete ctx;
        return false;
    }

    rslt = set_accel_config(&ctx->bmi);
    if (rslt != BMI2_OK) {
        bmi270_port_deinit(&ctx->bmi);
        delete ctx;
        return false;
    }

    rslt = set_gyro_config(&ctx->bmi);
    if (rslt != BMI2_OK) {
        bmi270_port_deinit(&ctx->bmi);
        delete ctx;
        return false;
    }

    uint8_t bmi_sens_list[] = { BMI2_ACCEL, BMI2_GYRO };
    bmi2_sensor_enable(bmi_sens_list, sizeof(bmi_sens_list), &ctx->bmi);

    rslt = aux_scan_for_bmm150(&ctx->bmi, &ctx->bmm150_addr);
    if (rslt != BMI2_OK) {
        bmi270_port_deinit(&ctx->bmi);
        delete ctx;
        return false;
    }

    rslt = bmm150_aux_port_init(&ctx->bmm, &ctx->bmi);
    if (rslt != BMM150_OK || ctx->bmm.chip_id != BMM150_CHIP_ID) {
        bmi270_port_deinit(&ctx->bmi);
        delete ctx;
        return false;
    }

    struct bmm150_settings bmm_settings = {0};
    bmm_settings.pwr_mode = BMM150_POWERMODE_NORMAL;
    bmm150_set_op_mode(&bmm_settings, &ctx->bmm);
    bmm_settings.preset_mode = BMM150_PRESETMODE_REGULAR;
    bmm150_set_presetmode(&bmm_settings, &ctx->bmm);

    bmiHandle_ = ctx;
    bmmHandle_ = &ctx->bmm;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_.statusText = "Sensor OK";
        state_.sensorReady = true;
    }
    stateDirty_ = true;
    return true;
}

void CompassApp::deinitSensors()
{
    if (!bmiHandle_) return;
    SensorCtx* ctx = static_cast<SensorCtx*>(bmiHandle_);

    struct bmm150_settings bmm_settings = {0};
    bmm_settings.pwr_mode = BMM150_POWERMODE_SUSPEND;
    bmm150_set_op_mode(&bmm_settings, &ctx->bmm);
    bmm150_aux_port_deinit(&ctx->bmm);

    bmi270_port_deinit(&ctx->bmi);
    delete ctx;
    bmiHandle_ = nullptr;
    bmmHandle_ = nullptr;
}

void CompassApp::sensorThreadFunc()
{
    SensorCtx* ctx = static_cast<SensorCtx*>(bmiHandle_);
    if (!ctx) return;

    struct bmi2_sens_data sens_data = {0};
    struct bmm150_mag_data mag_data = {0};

    while (running_.load()) {
        if (requestCalibrate_.exchange(false)) {
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                state_.calibrating = true;
                state_.statusText = "Calibrating...";
            }
            stateDirty_ = true;

            float bx, by, bz;
            int rc = mag_calibrate_8figure(&ctx->bmm, &ctx->bmi, &bx, &by, &bz, &running_);

            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                state_.calibrating = false;
                if (rc == 0) {
                    ctx->magCalValid = true;
                    ctx->magBiasX = bx;
                    ctx->magBiasY = by;
                    ctx->magBiasZ = bz;
                    state_.statusText = "Calib OK";
                } else {
                    state_.statusText = "Calib failed";
                }
            }
            stateDirty_ = true;
            continue;
        }

        int8_t rslt = bmi2_get_sensor_data(&sens_data, &ctx->bmi);
        if (rslt != BMI2_OK) {
            usleep(20000);
            continue;
        }

        float acc_x = 0, acc_y = 0, acc_z = 0;
        float gyr_x = 0, gyr_y = 0, gyr_z = 0;
        float temp_c = 0;

        if (sens_data.status & BMI2_DRDY_ACC) {
            acc_x = lsb_to_mps2(sens_data.acc.x, 4.0f, ctx->bmi.resolution);
            acc_y = lsb_to_mps2(sens_data.acc.y, 4.0f, ctx->bmi.resolution);
            acc_z = lsb_to_mps2(sens_data.acc.z, 4.0f, ctx->bmi.resolution);
        }

        if (sens_data.status & BMI2_DRDY_GYR) {
            gyr_x = lsb_to_dps(sens_data.gyr.x, 2000.0f, ctx->bmi.resolution);
            gyr_y = lsb_to_dps(sens_data.gyr.y, 2000.0f, ctx->bmi.resolution);
            gyr_z = lsb_to_dps(sens_data.gyr.z, 2000.0f, ctx->bmi.resolution);
        }

        int16_t temp_raw = 0;
        bmi2_get_temperature_data(&temp_raw, &ctx->bmi);
        temp_c = ((float)temp_raw / 512.0f) + 23.0f;

        rslt = bmm150_read_mag_data(&mag_data, &ctx->bmm);
        float mag_x_ut = 0, mag_y_ut = 0, mag_z_ut = 0;
        if (rslt == BMM150_OK) {
            mag_x_ut = mag_data.x / 16.0f;
            mag_y_ut = mag_data.y / 16.0f;
            mag_z_ut = mag_data.z / 16.0f;

            if (ctx->magCalValid) {
                mag_x_ut -= ctx->magBiasX;
                mag_y_ut -= ctx->magBiasY;
                mag_z_ut -= ctx->magBiasZ;
            }
        }

        float pitch = atan2f(-acc_x, sqrtf(acc_y * acc_y + acc_z * acc_z));
        float roll  = atan2f(acc_y, acc_z);

        float sin_p = sinf(pitch), cos_p = cosf(pitch);
        float sin_r = sinf(roll),  cos_r = cosf(roll);

        float mag_x_h = mag_x_ut * cos_p + mag_z_ut * sin_p;
        float mag_y_h = mag_x_ut * sin_r * sin_p + mag_y_ut * cos_r - mag_z_ut * sin_r * cos_p;

        float yaw = atan2f(-mag_y_h, mag_x_h) * 180.0f / (float)M_PI;
        if (yaw < 0.0f) yaw += 360.0f;

        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            state_.yaw   = yaw;
            state_.pitch = pitch * 180.0f / (float)M_PI;
            state_.roll  = roll * 180.0f / (float)M_PI;
            state_.accX = acc_x; state_.accY = acc_y; state_.accZ = acc_z;
            state_.gyrX = gyr_x; state_.gyrY = gyr_y; state_.gyrZ = gyr_z;
            state_.magX = mag_x_ut; state_.magY = mag_y_ut; state_.magZ = mag_z_ut;
            state_.temp = temp_c;
        }
        stateDirty_ = true;

        usleep(50000); /* 50 ms => 20 Hz */
    }
}
