#include "cp0_lora_hat_power_controller.hpp"

#include "cp0_lora_gpio.hpp"
#include "cp0_lora_gpio_offset_policy.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#ifndef SLOGI
#define SLOGI(fmt, ...) std::printf("[cp0_lora] " fmt "\n", ##__VA_ARGS__)
#endif

#if __has_include(<linux/gpio.h>)
#define CP0_HAT_POWER_HAS_GPIO_CDEV 1
#else
#define CP0_HAT_POWER_HAS_GPIO_CDEV 0
#endif

namespace cp0_lora_hat_power_controller {
namespace {

int line_fd = -1;
int line_offset = 5;
char chip_path[64] = "";

void log_result(const char *stage, int sysfs_result, int gpio_value, bool cdev_ok)
{
    const char *chip = chip_path[0] ? chip_path : "sysfs";
    const char *value = gpio_value < 0 ? "read_fail" : (gpio_value ? "HIGH" : "LOW");
    SLOGI("5VDBG %s cdev=%s chip=%s[%d] sysfs_ret=%d gpio5=%s",
          stage ? stage : "?", cdev_ok ? "ok" : "fail", chip, line_offset,
          sysfs_result, value);
}

enum class PrepareResult
{
    READY,
    UNAVAILABLE,
    INVALID_OFFSET,
};

PrepareResult prepare_line()
{
#if CP0_HAT_POWER_HAS_GPIO_CDEV
    const char *chip_env = getenv("HAT_5VOUT_CHIP");
    const char *offset_env = getenv("HAT_5VOUT_OFFSET");
    const auto offset_resolution = cp0_lora_gpio_offset_policy::resolve(offset_env, 5);
    if (!offset_resolution.valid()) return PrepareResult::INVALID_OFFSET;
    if ((chip_env && chip_env[0]) || offset_resolution.overridden()) {
        snprintf(chip_path, sizeof(chip_path), "%s",
                 chip_env && chip_env[0] ? chip_env : "/dev/gpiochip0");
        line_offset = offset_resolution.offset;
    } else if (!cp0_lora_backend::gpio_find_named_line(
                   chip_path, sizeof(chip_path), &line_offset)) {
        snprintf(chip_path, sizeof(chip_path), "/dev/gpiochip0");
        line_offset = 5;
    }
    if (line_fd >= 0) return PrepareResult::READY;
    return cp0_lora_backend::gpio_open_output_line(
        chip_path, line_offset, 1, &line_fd) ? PrepareResult::READY
                                             : PrepareResult::UNAVAILABLE;
#else
    const auto offset_resolution = cp0_lora_gpio_offset_policy::resolve(
        getenv("HAT_5VOUT_OFFSET"), 5);
    return offset_resolution.valid() ? PrepareResult::UNAVAILABLE
                                     : PrepareResult::INVALID_OFFSET;
#endif
}

} // namespace

bool enable(int fallback_gpio)
{
#if CP0_HAT_POWER_HAS_GPIO_CDEV
    const PrepareResult prepared = prepare_line();
    if (prepared == PrepareResult::INVALID_OFFSET) {
        SLOGI("5VDBG invalid HAT_5VOUT_OFFSET; refusing GPIO fallback");
        return false;
    }
    if (prepared == PrepareResult::READY &&
        cp0_lora_backend::gpio_set_output_line_value(line_fd, 0)) {
        const int value = cp0_lora_backend::gpio_get_value(fallback_gpio);
        log_result("cdev_set", 0, value, true);
        usleep(50000);
        return true;
    }
#else
    if (prepare_line() == PrepareResult::INVALID_OFFSET) {
        SLOGI("5VDBG invalid HAT_5VOUT_OFFSET; refusing GPIO fallback");
        return false;
    }
#endif
    const int result = cp0_lora_backend::gpio_init_output(fallback_gpio, 0);
    const int value = cp0_lora_backend::gpio_get_value(fallback_gpio);
    log_result("sysfs_set", result, value, false);
    if (result == 0) {
        usleep(50000);
        return true;
    }
    log_result("enable_fail", result, value, false);
    return false;
}

void shutdown()
{
    if (line_fd >= 0) {
        close(line_fd);
        line_fd = -1;
    }
}

} // namespace cp0_lora_hat_power_controller
