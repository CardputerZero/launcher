#include "cp0_lora_gpio.hpp"
#include "cp0_lora_gpio_offset_policy.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#if __has_include(<linux/gpio.h>)
#include <linux/gpio.h>
#include <sys/ioctl.h>
#define CP0_LORA_GPIO_HAS_CDEV 1
#else
#define CP0_LORA_GPIO_HAS_CDEV 0
#endif

#ifndef SLOGI
#define SLOGI(fmt, ...) std::printf("[cp0_lora] " fmt "\n", ##__VA_ARGS__)
#endif

namespace cp0_lora_backend {
namespace {

int write_text_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    const ssize_t result = write(fd, value, strlen(value));
    close(fd);
    return result < 0 ? -1 : 0;
}

int export_if_needed(int gpio)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    if (access(path, F_OK) == 0) return 0;
    char gpio_text[16];
    snprintf(gpio_text, sizeof(gpio_text), "%d", gpio);
    if (write_text_file("/sys/class/gpio/export", gpio_text) < 0 && errno != EBUSY) return -1;
    usleep(100000);
    return 0;
}

int set_direction(int gpio, const char *direction)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);
    return write_text_file(path, direction);
}

int init_input(int gpio)
{
    return export_if_needed(gpio) < 0 || set_direction(gpio, "in") < 0 ? -1 : 0;
}

int set_value(int gpio, int value)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    return write_text_file(path, value ? "1" : "0");
}

int init_input_irq_sysfs(int gpio, int *line_fd)
{
    if (line_fd == nullptr || init_input(gpio) < 0) return -1;
    char edge_path[64];
    snprintf(edge_path, sizeof(edge_path), "/sys/class/gpio/gpio%d/edge", gpio);
    if (write_text_file(edge_path, "rising") < 0) return -1;
    char value_path[64];
    snprintf(value_path, sizeof(value_path), "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(value_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return -1;
    char dummy = 0;
    lseek(fd, 0, SEEK_SET);
    (void)read(fd, &dummy, 1);
    *line_fd = fd;
    return 0;
}

#if CP0_LORA_GPIO_HAS_CDEV
bool open_input_line(const char *chip_path, int offset, int *line_fd)
{
    if (chip_path == nullptr || line_fd == nullptr) return false;
    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) return false;
    gpiohandle_request request{};
    request.lines = 1;
    request.lineoffsets[0] = (uint32_t)offset;
    request.flags = GPIOHANDLE_REQUEST_INPUT;
    snprintf(request.consumer_label, sizeof(request.consumer_label), "applaunch-lora-in");
    const bool ok = ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &request) == 0;
    close(chip_fd);
    if (ok) *line_fd = request.fd;
    return ok;
}

bool get_input_line_value(int line_fd, int *value)
{
    if (line_fd < 0 || value == nullptr) return false;
    gpiohandle_data data{};
    if (ioctl(line_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) return false;
    *value = data.values[0] ? 1 : 0;
    return true;
}

bool open_input_event_line(const char *chip_path, int offset, int *line_fd)
{
    if (chip_path == nullptr || line_fd == nullptr) return false;
    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) return false;
    gpioevent_request request{};
    request.lineoffset = (uint32_t)offset;
    request.handleflags = GPIOHANDLE_REQUEST_INPUT;
    request.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
    snprintf(request.consumer_label, sizeof(request.consumer_label), "applaunch-lora-irq");
    const bool ok = ioctl(chip_fd, GPIO_GET_LINEEVENT_IOCTL, &request) == 0;
    close(chip_fd);
    if (!ok) return false;
    *line_fd = request.fd;
    (void)fcntl(*line_fd, F_SETFL, fcntl(*line_fd, F_GETFL, 0) | O_NONBLOCK);
    return true;
}

bool line_name_matches(const char *name)
{
    static const char *names[] = {"G5_HAT_5VOUT_EN", "HAT_5VOUT_EN", "PG5", "G5"};
    if (name == nullptr || name[0] == '\0') return false;
    for (const char *candidate : names) {
        if (strcmp(name, candidate) == 0) return true;
    }
    return false;
}
#endif

} // namespace

int gpio_init_output(int gpio, int value)
{
    if (export_if_needed(gpio) < 0) return -1;
    if (set_direction(gpio, value ? "high" : "low") == 0) return 0;
    if (set_direction(gpio, "out") < 0) return -1;
    return set_value(gpio, value);
}

int gpio_get_value(int gpio)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char value = '0';
    const ssize_t result = read(fd, &value, 1);
    close(fd);
    return result <= 0 ? -1 : (value == '0' ? 0 : 1);
}

bool gpio_find_named_line(char *chip_path, size_t chip_path_size, int *offset)
{
#if CP0_LORA_GPIO_HAS_CDEV
    if (chip_path == nullptr || chip_path_size == 0 || offset == nullptr) return false;
    for (int chip_index = 0; chip_index < 8; ++chip_index) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/gpiochip%d", chip_index);
        int chip_fd = open(path, O_RDONLY);
        if (chip_fd < 0) continue;
        gpiochip_info chip_info{};
        if (ioctl(chip_fd, GPIO_GET_CHIPINFO_IOCTL, &chip_info) == 0) {
            for (int line = 0; line < (int)chip_info.lines; ++line) {
                gpioline_info line_info{};
                line_info.line_offset = line;
                if (ioctl(chip_fd, GPIO_GET_LINEINFO_IOCTL, &line_info) < 0) continue;
                if (line_name_matches(line_info.name) || line_name_matches(line_info.consumer)) {
                    snprintf(chip_path, chip_path_size, "%s", path);
                    *offset = line;
                    close(chip_fd);
                    return true;
                }
            }
        }
        close(chip_fd);
    }
#else
    (void)chip_path;
    (void)chip_path_size;
    (void)offset;
#endif
    return false;
}

bool gpio_open_output_line(const char *chip_path, int offset, int value, int *line_fd)
{
#if CP0_LORA_GPIO_HAS_CDEV
    if (chip_path == nullptr || line_fd == nullptr) return false;
    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) return false;
    gpiohandle_request request{};
    request.lines = 1;
    request.lineoffsets[0] = (uint32_t)offset;
    request.flags = GPIOHANDLE_REQUEST_OUTPUT;
    request.default_values[0] = (uint8_t)(value ? 1 : 0);
    snprintf(request.consumer_label, sizeof(request.consumer_label), "applaunch-lora-5v");
    const bool ok = ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &request) == 0;
    close(chip_fd);
    if (ok) *line_fd = request.fd;
    return ok;
#else
    (void)chip_path; (void)offset; (void)value; (void)line_fd;
    return false;
#endif
}

bool gpio_set_output_line_value(int line_fd, int value)
{
#if CP0_LORA_GPIO_HAS_CDEV
    if (line_fd < 0) return false;
    gpiohandle_data data{};
    data.values[0] = (uint8_t)(value ? 1 : 0);
    return ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) == 0;
#else
    (void)line_fd; (void)value;
    return false;
#endif
}

int gpio_init_output_any(const char *chip_env_name, const char *offset_env_name,
                         int gpio, int value, int *line_fd, const char *line_name)
{
    const auto offset_resolution = cp0_lora_gpio_offset_policy::resolve(
        offset_env_name ? getenv(offset_env_name) : nullptr, gpio);
    if (!offset_resolution.valid()) return -1;
    if (line_fd && *line_fd >= 0) return 0;
#if CP0_LORA_GPIO_HAS_CDEV
    const char *chip_env = chip_env_name ? getenv(chip_env_name) : nullptr;
    char chip_path[64] = "/dev/gpiochip0";
    const int offset = offset_resolution.offset;
    if (chip_env && chip_env[0]) snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    if (line_fd && gpio_open_output_line(chip_path, offset, value, line_fd)) {
        SLOGI("LoRa GPIO %s via cdev: %s[%d]=%d", line_name ? line_name : "out", chip_path, offset, value);
        return 0;
    }
#endif
    if (gpio_init_output(gpio, value) == 0) return 0;
    SLOGI("LoRa GPIO %s init failed: gpio=%d errno=%d", line_name ? line_name : "out", gpio, errno);
    return -1;
}

int gpio_init_input_any(const char *chip_env_name, const char *offset_env_name,
                        int gpio, int *line_fd, const char *line_name)
{
    const auto offset_resolution = cp0_lora_gpio_offset_policy::resolve(
        offset_env_name ? getenv(offset_env_name) : nullptr, gpio);
    if (!offset_resolution.valid()) return -1;
    if (line_fd && *line_fd >= 0) return 0;
#if CP0_LORA_GPIO_HAS_CDEV
    const char *chip_env = chip_env_name ? getenv(chip_env_name) : nullptr;
    char chip_path[64] = "/dev/gpiochip0";
    const int offset = offset_resolution.offset;
    if (chip_env && chip_env[0]) snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    if (line_fd && open_input_line(chip_path, offset, line_fd)) {
        SLOGI("LoRa GPIO %s via cdev: %s[%d]", line_name ? line_name : "in", chip_path, offset);
        return 0;
    }
#endif
    if (init_input(gpio) == 0) return 0;
    SLOGI("LoRa GPIO %s input init failed: gpio=%d errno=%d", line_name ? line_name : "in", gpio, errno);
    return -1;
}

int gpio_init_input_irq_any(const char *chip_env_name, const char *offset_env_name,
                            int gpio, int *line_fd, const char *line_name)
{
    const auto offset_resolution = cp0_lora_gpio_offset_policy::resolve(
        offset_env_name ? getenv(offset_env_name) : nullptr, gpio);
    if (!offset_resolution.valid()) return -1;
    if (line_fd && *line_fd >= 0) return 0;
#if CP0_LORA_GPIO_HAS_CDEV
    const char *chip_env = chip_env_name ? getenv(chip_env_name) : nullptr;
    char chip_path[64] = "/dev/gpiochip0";
    const int offset = offset_resolution.offset;
    if (chip_env && chip_env[0]) snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    if (line_fd && open_input_event_line(chip_path, offset, line_fd)) {
        SLOGI("LoRa GPIO %s irq-event via cdev: %s[%d]", line_name ? line_name : "irq", chip_path, offset);
        return 0;
    }
#endif
    if (line_fd && init_input_irq_sysfs(gpio, line_fd) == 0) {
        SLOGI("LoRa GPIO %s irq-event via sysfs: gpio%d rising", line_name ? line_name : "irq", gpio);
        return 0;
    }
    return -1;
}

int gpio_get_value_any(int gpio, int line_fd)
{
#if CP0_LORA_GPIO_HAS_CDEV
    int value = 0;
    if (line_fd >= 0 && get_input_line_value(line_fd, &value)) return value;
#endif
    return gpio_get_value(gpio);
}

int gpio_set_value_any(int gpio, int line_fd, int value)
{
#if CP0_LORA_GPIO_HAS_CDEV
    if (line_fd >= 0) return gpio_set_output_line_value(line_fd, value) ? 0 : -1;
#endif
    return set_value(gpio, value);
}

} // namespace cp0_lora_backend
