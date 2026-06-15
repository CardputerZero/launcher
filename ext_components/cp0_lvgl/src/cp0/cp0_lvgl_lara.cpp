#include "cp0_lvgl_app.h"
#include "../cp0_app_internal_utils.h"
#include "hal_lvgl_bsp.h"

#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <iterator>
#include <memory>
#include <poll.h>
#include <unistd.h>

#ifndef SLOGI
#define SLOGI(fmt, ...) std::printf("[cp0_lora] " fmt "\n", ##__VA_ARGS__)
#endif

#if __has_include(<linux/gpio.h>)
#include <linux/gpio.h>
#define HAS_LINUX_GPIO_CDEV 1
#else
#define HAS_LINUX_GPIO_CDEV 0
#endif

#if __has_include(<sys/ioctl.h>) && __has_include(<linux/spi/spidev.h>)
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#else
extern "C" int ioctl(int fd, unsigned long request, ...);
struct spi_ioc_transfer {
    unsigned long tx_buf;
    unsigned long rx_buf;
    uint32_t len;
    uint32_t speed_hz;
    uint16_t delay_usecs;
    uint8_t bits_per_word;
    uint8_t cs_change;
    uint32_t pad;
};
#ifndef SPI_MODE_0
#define SPI_MODE_0 0
#endif
#ifndef SPI_IOC_WR_MODE
#define SPI_IOC_WR_MODE 0
#endif
#ifndef SPI_IOC_WR_BITS_PER_WORD
#define SPI_IOC_WR_BITS_PER_WORD 0
#endif
#ifndef SPI_IOC_WR_MAX_SPEED_HZ
#define SPI_IOC_WR_MAX_SPEED_HZ 0
#endif
#ifndef SPI_IOC_MESSAGE
#define SPI_IOC_MESSAGE(N) 0
#endif
#endif

#if __has_include(<linux/i2c-dev.h>)
#include <linux/i2c-dev.h>
#define CP0_LORA_HAS_LINUX_I2CDEV 1
#else
#define CP0_LORA_HAS_LINUX_I2CDEV 0
#ifndef I2C_SLAVE
#define I2C_SLAVE 0x0703
#endif
#endif

#include "RadioLib.h"
#if __has_include(<lgpio.h>)
#include "hal/RPi/PiHal.h"
#else
class PiHal : public RadioLibHal {
  public:
    PiHal(uint8_t spiChannel, uint32_t spiSpeed = 2000000, uint8_t spiDevice = 0, uint8_t gpioDevice = 0)
      : RadioLibHal(0, 1, 0, 1, 1, 2),
        _gpioDevice(gpioDevice),
        _spiDevice(spiDevice),
        _spiSpeed(spiSpeed),
        _spiChannel(spiChannel) {}

  protected:
    uint8_t _gpioDevice;
    uint8_t _spiDevice;
    uint32_t _spiSpeed;
    uint8_t _spiChannel;
};
#endif

namespace cp0_lora_backend {
// ============================================================
//  Hardware configuration and state
// ============================================================
static int g_spi_fd = -1;
static bool g_lora_tx_mode = false;
static bool g_lora_selected_tx_mode = false;
static bool g_lora_tx_in_progress = false;
static bool g_lora_pending_rx_after_tx = false;
static uint64_t g_lora_last_auto_tx_ms = 0;
static char g_spi_device[64] = "/dev/spidev0.1";
static unsigned int g_spi_speed = 1000000;
static int g_lora_sck_gpio = 11;
static int g_lora_mosi_gpio = 10;
static int g_lora_miso_gpio = 9;
static int g_lora_power_gpio = 5;
static int g_lora_nss_gpio = 7;
static bool g_lora_nss_manual = false;
static int g_lora_rst_gpio = 26;
static int g_lora_irq_gpio = 23;
static int g_lora_busy_gpio = 22;
static int g_lora_rst_fd = -1;
static int g_lora_busy_fd = -1;
static int g_lora_irq_fd = -1;
static int g_lora_nss_fd = -1;
static volatile bool g_lora_initialized = false;
static bool g_lora_hw_ready = false;
static bool g_lora_irq_poll_fallback = true;
static volatile bool g_lora_rx_done = false;
static volatile bool g_lora_tx_done = false;
static uint32_t g_lora_tx_counter = 0;
static uint64_t g_lora_tx_start_ms = 0;
static uint64_t g_lora_sent_popup_until_ms = 0;
static char g_lora_last_rx[128] = {0};
static char g_lora_last_tx[128] = "Hello from M5 LoRa-1262";
static char g_lora_tx_input[128] = "";
static bool g_lora_has_sent_message = false;
static float g_lora_last_rssi = 0.0f;
static float g_lora_last_snr = 0.0f;
static const char *g_lora_cfg_freq = "869.525024MHz";
static const char *g_lora_cfg_bw = "250kHz";
static const char *g_lora_cfg_sf = "SF7";
static const char *g_lora_cfg_cr = "4/5";
static const char *g_lora_cfg_sync = "0x34";
static const char *g_lora_cfg_preamble = "20";
static const char *g_lora_cfg_power = "10dBm";
static const char *g_lora_cfg_tcxo = "0.0V(disabled)";
static char g_lora_last_diag[256] = "idle";
static char g_lora_probe_summary[256] = "probe not started";
static char g_lora_probe_display[128] = "SPI: probing...";
static const int g_pi4io_i2c_bus = 1;
static const int g_pi4io_sda_gpio = 2;
static const int g_pi4io_scl_gpio = 3;
static const uint8_t g_pi4io_i2c_addr = 0x43;
static bool g_pi4io_found = false;
static bool g_pi4io_initialized = false;
static char g_pi4io_status[160] = "I2C 0x43 not checked";
static uint8_t g_pi4io_output_cache = 0x00;
static uint8_t g_pi4io_config_cache = 0xFF;
static uint8_t g_pi4io_polarity_cache = 0x00;
static int g_hat_5vout_fd = -1;
static int g_hat_5vout_offset = 5;
static char g_hat_5vout_chip[64] = "";
static int g_hat_5vout_last_sysfs_ret = -999;
static int g_hat_5vout_last_value = -1;
static bool g_hat_5vout_last_cdev_ok = false;

// Forward declarations
static uint64_t get_monotonic_ms(void);
static bool lora_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len);
static int gpio_set_value(int gpio, int value);
#if HAS_LINUX_GPIO_CDEV
static bool gpio_open_output_line(const char *chip_path, int offset, int value, int *line_fd);
static bool gpio_set_output_line_value(int line_fd, int value);
#endif
static int gpio_init_output_any(const char *chip_env_name, const char *offset_env_name, int gpio, int value, int *line_fd, const char *line_name);
static int gpio_init_input_any(const char *chip_env_name, const char *offset_env_name, int gpio, int *line_fd, const char *line_name);
static int gpio_get_value_any(int gpio, int line_fd);
static int gpio_set_value_any(int gpio, int line_fd, int value);
static size_t collect_spi_candidates(char out[][64], size_t max_count, const char *preferred);
static void resolve_lora_spi_device(void);
static bool probe_lora_spi_device(void);
static bool hat_5vout_enable(void);
static bool hat_5vout_prepare_line(void);
static void lora_update_power_debug(const char *stage, int sysfs_ret, int gpio_value, bool cdev_ok);
static bool pi4io_scan_and_init_before_lora(void);
static bool pi4io_open_bus(int *fd);
static bool pi4io_select_device(int fd);
static bool pi4io_write_reg(int fd, uint8_t reg, uint8_t value);
static bool pi4io_probe_device(int fd);
static bool pi4io_init_device(int fd);
static void lora_apply_mode(bool tx_mode);
static void lora_start_receive_mode(void);
static void lora_send_demo_packet(void);
static void lora_service_irq_once(void);
static void lora_check_tx_fallback(void);
static void lora_set_diag_step(const char *step, int code, const char *detail);
static void lora_refresh_status(const char *prefix);
static const char *lora_radiolib_status_text(int16_t state);
static bool lora_send_text_packet(const char *payload);
static void lora_init_hardware(void);


// ============================================================
//  GPIO / SPI / I2C low level (ported from UserDemo)
// ============================================================

static int write_text_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t ret = write(fd, value, strlen(value));
    close(fd);
    return ret < 0 ? -1 : 0;
}

static int gpio_export_if_needed(int gpio)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    if (access(path, F_OK) == 0) return 0;
    char gpio_str[16];
    snprintf(gpio_str, sizeof(gpio_str), "%d", gpio);
    if (write_text_file("/sys/class/gpio/export", gpio_str) < 0 && errno != EBUSY) {
        return -1;
    }
    usleep(100000);
    return 0;
}

static int gpio_set_direction(int gpio, const char *direction)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);
    return write_text_file(path, direction);
}

static int gpio_init_input(int gpio)
{
    return gpio_export_if_needed(gpio) < 0 || gpio_set_direction(gpio, "in") < 0 ? -1 : 0;
}

static int gpio_open_value_fd(int gpio)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    return open(path, O_RDONLY | O_NONBLOCK);
}

static int gpio_init_input_irq_sysfs(int gpio, int *line_fd)
{
    if (line_fd == NULL) return -1;
    if (gpio_init_input(gpio) < 0) return -1;
    char edge_path[64];
    snprintf(edge_path, sizeof(edge_path), "/sys/class/gpio/gpio%d/edge", gpio);
    if (write_text_file(edge_path, "rising") < 0) return -1;
    int fd = gpio_open_value_fd(gpio);
    if (fd < 0) return -1;
    char dummy = 0;
    lseek(fd, 0, SEEK_SET);
    (void)read(fd, &dummy, 1);
    *line_fd = fd;
    return 0;
}

static int gpio_init_output(int gpio, int value)
{
    if (gpio_export_if_needed(gpio) < 0) return -1;
    if (value) {
        if (gpio_set_direction(gpio, "high") == 0) return 0;
    } else {
        if (gpio_set_direction(gpio, "low") == 0) return 0;
    }
    if (gpio_set_direction(gpio, "out") < 0) return -1;
    return gpio_set_value(gpio, value);
}

static int gpio_get_value(int gpio)
{
    char path[64];
    char value = '0';
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t ret = read(fd, &value, 1);
    close(fd);
    if (ret <= 0) return -1;
    return value == '0' ? 0 : 1;
}

static int gpio_set_value(int gpio, int value)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    return write_text_file(path, value ? "1" : "0");
}

#if HAS_LINUX_GPIO_CDEV
static bool gpio_open_input_line(const char *chip_path, int offset, int *line_fd)
{
    if (chip_path == NULL || line_fd == NULL) return false;
    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) return false;
    struct gpiohandle_request req;
    memset(&req, 0, sizeof(req));
    req.lines = 1;
    req.lineoffsets[0] = (uint32_t)offset;
    req.flags = GPIOHANDLE_REQUEST_INPUT;
    snprintf(req.consumer_label, sizeof(req.consumer_label), "applaunch-lora-in");
    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        close(chip_fd);
        return false;
    }
    close(chip_fd);
    *line_fd = req.fd;
    return true;
}

static bool gpio_get_input_line_value(int line_fd, int *value)
{
    if (line_fd < 0 || value == NULL) return false;
    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));
    if (ioctl(line_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) return false;
    *value = data.values[0] ? 1 : 0;
    return true;
}

static bool gpio_open_input_event_line(const char *chip_path, int offset, int *line_fd)
{
    if (chip_path == NULL || line_fd == NULL) return false;
    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) return false;
    struct gpioevent_request req;
    memset(&req, 0, sizeof(req));
    req.lineoffset = (uint32_t)offset;
    req.handleflags = GPIOHANDLE_REQUEST_INPUT;
    req.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
    snprintf(req.consumer_label, sizeof(req.consumer_label), "applaunch-lora-irq");
    if (ioctl(chip_fd, GPIO_GET_LINEEVENT_IOCTL, &req) < 0) {
        close(chip_fd);
        return false;
    }
    close(chip_fd);
    *line_fd = req.fd;
    (void)fcntl(*line_fd, F_SETFL, fcntl(*line_fd, F_GETFL, 0) | O_NONBLOCK);
    return true;
}

static bool gpio_line_name_matches(const char *name)
{
    static const char *candidates[] = {
        "G5_HAT_5VOUT_EN", "HAT_5VOUT_EN", "PG5", "G5",
    };
    if (name == NULL || name[0] == '\0') return false;
    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
        if (strcmp(name, candidates[i]) == 0) return true;
    }
    return false;
}

static bool gpio_find_named_line(char *chip_path, size_t chip_path_size, int *offset)
{
    if (chip_path == NULL || chip_path_size == 0 || offset == NULL) return false;
    for (int chip_index = 0; chip_index < 8; ++chip_index) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/gpiochip%d", chip_index);
        int chip_fd = open(path, O_RDONLY);
        if (chip_fd < 0) continue;
        struct gpiochip_info chip_info;
        memset(&chip_info, 0, sizeof(chip_info));
        if (ioctl(chip_fd, GPIO_GET_CHIPINFO_IOCTL, &chip_info) < 0) {
            close(chip_fd); continue;
        }
        for (int line = 0; line < (int)chip_info.lines; ++line) {
            struct gpioline_info line_info;
            memset(&line_info, 0, sizeof(line_info));
            line_info.line_offset = line;
            if (ioctl(chip_fd, GPIO_GET_LINEINFO_IOCTL, &line_info) < 0) continue;
            if (gpio_line_name_matches(line_info.name) || gpio_line_name_matches(line_info.consumer)) {
                snprintf(chip_path, chip_path_size, "%s", path);
                *offset = line;
                close(chip_fd);
                return true;
            }
        }
        close(chip_fd);
    }
    return false;
}

static bool gpio_open_output_line(const char *chip_path, int offset, int value, int *line_fd)
{
    if (chip_path == NULL || line_fd == NULL) return false;
    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) return false;
    struct gpiohandle_request req;
    memset(&req, 0, sizeof(req));
    req.lines = 1;
    req.lineoffsets[0] = (uint32_t)offset;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req.default_values[0] = (uint8_t)(value ? 1 : 0);
    snprintf(req.consumer_label, sizeof(req.consumer_label), "applaunch-lora-5v");
    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        close(chip_fd);
        return false;
    }
    close(chip_fd);
    *line_fd = req.fd;
    return true;
}

static bool gpio_set_output_line_value(int line_fd, int value)
{
    if (line_fd < 0) return false;
    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));
    data.values[0] = (uint8_t)(value ? 1 : 0);
    return ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) == 0;
}
#endif

static int gpio_init_output_any(const char *chip_env_name, const char *offset_env_name, int gpio, int value, int *line_fd, const char *line_name)
{
    if (line_fd && *line_fd >= 0) return 0;
#if HAS_LINUX_GPIO_CDEV
    const char *chip_env = getenv(chip_env_name);
    const char *offset_env = getenv(offset_env_name);
    char chip_path[64] = "/dev/gpiochip0";
    int offset = gpio;
    if (chip_env && chip_env[0]) snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    if (offset_env && offset_env[0]) offset = atoi(offset_env);
    if (line_fd && gpio_open_output_line(chip_path, offset, value, line_fd)) {
        SLOGI("LoRa GPIO %s via cdev: %s[%d]=%d", line_name ? line_name : "out", chip_path, offset, value);
        return 0;
    }
#endif
    if (gpio_init_output(gpio, value) == 0) return 0;
    SLOGI("LoRa GPIO %s init failed: gpio=%d errno=%d", line_name ? line_name : "out", gpio, errno);
    return -1;
}

static int gpio_init_input_any(const char *chip_env_name, const char *offset_env_name, int gpio, int *line_fd, const char *line_name)
{
    if (line_fd && *line_fd >= 0) return 0;
#if HAS_LINUX_GPIO_CDEV
    const char *chip_env = getenv(chip_env_name);
    const char *offset_env = getenv(offset_env_name);
    char chip_path[64] = "/dev/gpiochip0";
    int offset = gpio;
    if (chip_env && chip_env[0]) snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    if (offset_env && offset_env[0]) offset = atoi(offset_env);
    if (line_fd && gpio_open_input_line(chip_path, offset, line_fd)) {
        SLOGI("LoRa GPIO %s via cdev: %s[%d]", line_name ? line_name : "in", chip_path, offset);
        return 0;
    }
#endif
    if (gpio_init_input(gpio) == 0) return 0;
    SLOGI("LoRa GPIO %s input init failed: gpio=%d errno=%d", line_name ? line_name : "in", gpio, errno);
    return -1;
}

static int gpio_init_input_irq_any(const char *chip_env_name, const char *offset_env_name, int gpio, int *line_fd, const char *line_name)
{
    if (line_fd && *line_fd >= 0) return 0;
#if HAS_LINUX_GPIO_CDEV
    const char *chip_env = getenv(chip_env_name);
    const char *offset_env = getenv(offset_env_name);
    char chip_path[64] = "/dev/gpiochip0";
    int offset = gpio;
    if (chip_env && chip_env[0]) snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    if (offset_env && offset_env[0]) offset = atoi(offset_env);
    if (line_fd && gpio_open_input_event_line(chip_path, offset, line_fd)) {
        SLOGI("LoRa GPIO %s irq-event via cdev: %s[%d]", line_name ? line_name : "irq", chip_path, offset);
        return 0;
    }
#endif
    if (line_fd && gpio_init_input_irq_sysfs(gpio, line_fd) == 0) {
        SLOGI("LoRa GPIO %s irq-event via sysfs: gpio%d rising", line_name ? line_name : "irq", gpio);
        return 0;
    }
    return -1;
}

static int gpio_get_value_any(int gpio, int line_fd)
{
#if HAS_LINUX_GPIO_CDEV
    int value = 0;
    if (line_fd >= 0 && gpio_get_input_line_value(line_fd, &value)) return value;
#endif
    return gpio_get_value(gpio);
}

static int gpio_set_value_any(int gpio, int line_fd, int value)
{
#if HAS_LINUX_GPIO_CDEV
    if (line_fd >= 0) return gpio_set_output_line_value(line_fd, value) ? 0 : -1;
#endif
    return gpio_set_value(gpio, value);
}

static size_t collect_spi_candidates(char out[][64], size_t max_count, const char *preferred)
{
    if (out == NULL || max_count == 0) return 0;
    size_t count = 0;
    auto append_candidate = [&](const char *path) {
        if (path == NULL || path[0] == '\0') return;
        for (size_t i = 0; i < count; ++i) if (strcmp(out[i], path) == 0) return;
        if (count < max_count) { snprintf(out[count], 64, "%s", path); ++count; }
    };
    append_candidate(preferred);
    append_candidate("/dev/spidev0.1");
    append_candidate("/dev/spidev0.0");
    DIR *dir = opendir("/dev");
    if (dir != NULL) {
        struct dirent *entry = NULL;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "spidev", 6) != 0) continue;
            if (strlen(entry->d_name) > sizeof("/dev/") + 57) continue;
            char full_path[64];
            snprintf(full_path, sizeof(full_path), "/dev/%s", entry->d_name);
            append_candidate(full_path);
        }
        closedir(dir);
    }
    const char *fallbacks[] = {
        "/dev/spidev0.1", "/dev/spidev0.0", "/dev/spidev1.0", "/dev/spidev1.1",
        "/dev/spidev2.0", "/dev/spidev2.1", "/dev/spidev3.0", "/dev/spidev3.1",
        "/dev/spidev4.0", "/dev/spidev4.1",
    };
    for (size_t i = 0; i < sizeof(fallbacks)/sizeof(fallbacks[0]); ++i) append_candidate(fallbacks[i]);
    return count;
}

static void lora_update_power_debug(const char *stage, int sysfs_ret, int gpio_value, bool cdev_ok)
{
    char text[256];
    const char *chip_text = g_hat_5vout_chip[0] ? g_hat_5vout_chip : "sysfs";
    const char *value_text = gpio_value < 0 ? "read_fail" : (gpio_value ? "HIGH" : "LOW");
    snprintf(text, sizeof(text), "5VDBG %s cdev=%s chip=%s[%d] sysfs_ret=%d gpio5=%s",
             stage ? stage : "?", cdev_ok ? "ok" : "fail", chip_text, g_hat_5vout_offset, sysfs_ret, value_text);
    SLOGI("%s", text);
}

static bool hat_5vout_prepare_line(void)
{
#if HAS_LINUX_GPIO_CDEV
    const char *chip_env = getenv("HAT_5VOUT_CHIP");
    const char *offset_env = getenv("HAT_5VOUT_OFFSET");
    if (chip_env && chip_env[0]) {
        snprintf(g_hat_5vout_chip, sizeof(g_hat_5vout_chip), "%s", chip_env);
        g_hat_5vout_offset = offset_env && offset_env[0] ? atoi(offset_env) : 5;
    } else if (!gpio_find_named_line(g_hat_5vout_chip, sizeof(g_hat_5vout_chip), &g_hat_5vout_offset)) {
        snprintf(g_hat_5vout_chip, sizeof(g_hat_5vout_chip), "/dev/gpiochip0");
        g_hat_5vout_offset = 5;
    }
    if (g_hat_5vout_fd >= 0) { g_hat_5vout_last_cdev_ok = true; return true; }
    if (gpio_open_output_line(g_hat_5vout_chip, g_hat_5vout_offset, 1, &g_hat_5vout_fd)) {
        g_hat_5vout_last_cdev_ok = true; return true;
    }
    g_hat_5vout_last_cdev_ok = false;
#endif
    return false;
}

static bool lora_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (g_spi_fd < 0) return false;
    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = (uint32_t)len;
    tr.speed_hz = g_spi_speed;
    tr.bits_per_word = 8;
    int ret = ioctl(g_spi_fd, SPI_IOC_MESSAGE(1), &tr);
    return ret >= 0;
}

static bool lora_open_runtime_spi(void)
{
    if (g_spi_fd >= 0) return true;
    g_spi_fd = open(g_spi_device, O_RDWR);
    if (g_spi_fd < 0) {
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "runtime SPI open failed on %s", g_spi_device);
        return false;
    }
    uint8_t mode = (uint8_t)SPI_MODE_0;
    uint8_t bits = 8;
    if (ioctl(g_spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(g_spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(g_spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &g_spi_speed) < 0) {
        close(g_spi_fd); g_spi_fd = -1;
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "runtime SPI config failed on %s", g_spi_device);
        return false;
    }
    return true;
}

static bool sx1262_wait_while_busy(unsigned int timeout_ms)
{
    const unsigned int sleep_us = 1000;
    unsigned int waited_ms = 0;
    while (waited_ms < timeout_ms) {
        int busy = gpio_get_value_any(g_lora_busy_gpio, g_lora_busy_fd);
        if (busy < 0) return false;
        if (busy == 0) return true;
        usleep(sleep_us);
        waited_ms += 1;
    }
    return false;
}

static bool sx1262_reset(void)
{
    if (gpio_set_value_any(g_lora_rst_gpio, g_lora_rst_fd, 0) < 0) return false;
    usleep(20000);
    if (gpio_set_value_any(g_lora_rst_gpio, g_lora_rst_fd, 1) < 0) return false;
    usleep(10000);
    return sx1262_wait_while_busy(200);
}

static bool sx1262_get_status_raw(uint8_t *status)
{
    uint8_t tx[2] = {0xC0, 0x00};
    uint8_t rx[2] = {0};
    if (!status) return false;
    if (!lora_spi_transfer(tx, rx, sizeof(tx))) return false;
    *status = rx[1];
    return true;
}

static bool hat_5vout_enable(void)
{
    bool cdev_ok = false;
#if HAS_LINUX_GPIO_CDEV
    if (hat_5vout_prepare_line()) {
        if (gpio_set_output_line_value(g_hat_5vout_fd, 0)) {
            cdev_ok = true;
            g_hat_5vout_last_sysfs_ret = 0;
            g_hat_5vout_last_value = gpio_get_value(g_lora_power_gpio);
            lora_update_power_debug("cdev_set", g_hat_5vout_last_sysfs_ret, g_hat_5vout_last_value, cdev_ok);
            usleep(50000);
            return true;
        }
    }
#endif
    g_hat_5vout_last_sysfs_ret = gpio_init_output(g_lora_power_gpio, 0);
    g_hat_5vout_last_value = gpio_get_value(g_lora_power_gpio);
    lora_update_power_debug("sysfs_set", g_hat_5vout_last_sysfs_ret, g_hat_5vout_last_value, cdev_ok);
    if (g_hat_5vout_last_sysfs_ret == 0) { usleep(50000); return true; }
    lora_update_power_debug("enable_fail", g_hat_5vout_last_sysfs_ret, g_hat_5vout_last_value, cdev_ok);
    return false;
}

static bool pi4io_open_bus(int *fd)
{
#if !CP0_LORA_HAS_LINUX_I2CDEV
    if (fd) *fd = -1;
    snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C dev header missing, cannot access 0x%02X", g_pi4io_i2c_addr);
    return false;
#else
    if (fd == NULL) { snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C fd pointer invalid"); return false; }
    char dev_path[64];
    snprintf(dev_path, sizeof(dev_path), "/dev/i2c-%d", g_pi4io_i2c_bus);
    *fd = open(dev_path, O_RDWR);
    if (*fd < 0) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status), "open %s failed, SDA:%d SCL:%d errno=%d",
                 dev_path, g_pi4io_sda_gpio, g_pi4io_scl_gpio, errno);
        return false;
    }
    return true;
#endif
}

static bool pi4io_select_device(int fd)
{
    if (fd < 0) { snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C fd invalid for 0x%02X", g_pi4io_i2c_addr); return false; }
    if (ioctl(fd, I2C_SLAVE, g_pi4io_i2c_addr) < 0) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status), "select 0x%02X failed on /dev/i2c-%d errno=%d",
                 g_pi4io_i2c_addr, g_pi4io_i2c_bus, errno);
        return false;
    }
    return true;
}

static bool pi4io_write_reg(int fd, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return write(fd, buf, sizeof(buf)) == (ssize_t)sizeof(buf);
}

static bool pi4io_probe_device(int fd)
{
    uint8_t reg = 0x00;
    if (write(fd, &reg, 1) != 1) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C 0x%02X not found on /dev/i2c-%d (SDA:%d SCL:%d)",
                 g_pi4io_i2c_addr, g_pi4io_i2c_bus, g_pi4io_sda_gpio, g_pi4io_scl_gpio);
        return false;
    }
    snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C 0x%02X found on /dev/i2c-%d (SDA:%d SCL:%d)",
             g_pi4io_i2c_addr, g_pi4io_i2c_bus, g_pi4io_sda_gpio, g_pi4io_scl_gpio);
    return true;
}

static bool pi4io_init_device(int fd)
{
    if (fd < 0) { snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C IO init invalid fd for 0x%02X", g_pi4io_i2c_addr); return false; }
    g_pi4io_polarity_cache = 0x00;
    g_pi4io_output_cache = 0x01;
    g_pi4io_config_cache = 0xFE;
    errno = 0;
    if (!pi4io_write_reg(fd, 0x02, g_pi4io_polarity_cache)) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C IO write POL failed at 0x%02X errno=%d", g_pi4io_i2c_addr, errno);
        return false;
    }
    errno = 0;
    if (!pi4io_write_reg(fd, 0x01, g_pi4io_output_cache)) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C IO write OUT failed at 0x%02X errno=%d", g_pi4io_i2c_addr, errno);
        return false;
    }
    errno = 0;
    if (!pi4io_write_reg(fd, 0x03, g_pi4io_config_cache)) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C IO write CFG failed at 0x%02X errno=%d", g_pi4io_i2c_addr, errno);
        return false;
    }
    snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C IO init ok OUT=0x%02X POL=0x%02X CFG=0x%02X P0=HIGH",
             g_pi4io_output_cache, g_pi4io_polarity_cache, g_pi4io_config_cache);
    return true;
}

static bool pi4io_scan_and_init_before_lora(void)
{
    int fd = -1;
    bool ok = false;
    g_pi4io_found = false;
    g_pi4io_initialized = false;
    if (!pi4io_open_bus(&fd)) return false;
    do {
        if (!pi4io_select_device(fd)) break;
        if (!pi4io_probe_device(fd)) break;
        g_pi4io_found = true;
        if (!pi4io_init_device(fd)) break;
        g_pi4io_initialized = true;
        ok = true;
    } while (0);
    if (fd >= 0) close(fd);
    return ok;
}

static bool probe_lora_spi_device(void)
{
    const char *spi_env = getenv("LORA_SPI_DEV");
    char candidates[16][64] = {{0}};
    const size_t candidate_count = collect_spi_candidates(candidates, 16, spi_env);
    char summary[256] = {0};

    if (access("/dev", F_OK) != 0) {
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "Linux /dev not available; LoRa SPI HAL requires Raspberry Pi Linux runtime");
        snprintf(g_lora_probe_summary, sizeof(g_lora_probe_summary), "no /dev directory visible");
        snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "SPI: /dev unavailable");
        return false;
    }
    if (candidate_count == 0) {
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "no /dev/spidev* found; enable SPI on Raspberry Pi OS");
        snprintf(g_lora_probe_summary, sizeof(g_lora_probe_summary), "probe aborted: no spidev nodes");
        snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "SPI: no spidev found");
        return false;
    }
    SLOGI("LoRa SPI probe policy: prefer SPI0 only, CE1 then CE0");
    summary[0] = '\0';
    for (size_t i = 0; i < candidate_count; ++i) {
        const char *dev = candidates[i];
        if (spi_env && spi_env[0] && strcmp(spi_env, dev) == 0) continue;
        if (summary[0]) strncat(summary, ", ", sizeof(summary) - strlen(summary) - 1);
        strncat(summary, dev, sizeof(summary) - strlen(summary) - 1);
    }
    if (spi_env && spi_env[0]) {
        snprintf(g_lora_probe_summary, sizeof(g_lora_probe_summary), "probe order: %.96s%s%.128s",
                 spi_env, summary[0] ? ", " : "", summary);
        snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "Try: %.96s -> 0.1 -> 0.0", spi_env);
    } else {
        snprintf(g_lora_probe_summary, sizeof(g_lora_probe_summary), "probe order: %.224s", summary);
        snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "Try: /dev/spidev0.1 -> /dev/spidev0.0");
    }

    auto try_probe = [](const char *dev) -> bool {
        if (dev == NULL || dev[0] == '\0' || access(dev, F_OK) != 0) return false;
        snprintf(g_spi_device, sizeof(g_spi_device), "%s", dev);
        g_lora_nss_manual = false;
        const char *cs_name = strstr(g_spi_device, "spidev0.1") ? "SPI0-CE1" : (strstr(g_spi_device, "spidev0.0") ? "SPI0-CE0" : "non-SPI0");
        SLOGI("LoRa probe: trying %s [%s] (cs=hw-auto)", g_spi_device, cs_name);
        g_lora_initialized = false;
        if (g_spi_fd >= 0) { close(g_spi_fd); g_spi_fd = -1; }
        if (gpio_init_output_any("LORA_RST_CHIP", "LORA_RST_OFFSET", g_lora_rst_gpio, 1, &g_lora_rst_fd, "RST") < 0) {
            snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "RST gpio init failed on %s", g_spi_device);
            return false;
        }
        if (!sx1262_reset()) {
            snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "RST/BUSY handshake failed on %s", g_spi_device);
            return false;
        }
        uint8_t status = 0;
        g_spi_fd = open(g_spi_device, O_RDWR);
        if (g_spi_fd < 0) {
            snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "SPI open failed on %s", g_spi_device);
            return false;
        }
        uint8_t mode = (uint8_t)SPI_MODE_0;
        uint8_t bits = 8;
        if (ioctl(g_spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
            ioctl(g_spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
            ioctl(g_spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &g_spi_speed) < 0) {
            close(g_spi_fd); g_spi_fd = -1;
            snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "SPI config failed on %s", g_spi_device);
            return false;
        }
        bool ok = sx1262_get_status_raw(&status);
        close(g_spi_fd); g_spi_fd = -1;
        if (!ok) {
            snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "status read failed on %s", g_spi_device);
            return false;
        }
        SLOGI("LoRa probe: %s [%s] (cs=hw-auto) status=0x%02X", g_spi_device, cs_name, status);
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "probe ok on %s[%s] cs=hw-auto status=0x%02X", g_spi_device, cs_name, status);
        snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "FOUND: %s (%s)", g_spi_device, cs_name);
        return true;
    };

    if (spi_env && spi_env[0] && try_probe(spi_env)) return true;
    for (size_t i = 0; i < candidate_count; ++i) {
        if (try_probe(candidates[i])) return true;
    }
    snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "all SPI buses probed, no SX1262 response (%.192s)", g_lora_probe_summary);
    snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "NOT FOUND: tried 0.1 and 0.0");
    return false;
}

static void resolve_lora_spi_device(void)
{
    const char *spi_env = getenv("LORA_SPI_DEV");
    char candidates[16][64] = {{0}};
    const size_t candidate_count = collect_spi_candidates(candidates, 16, spi_env);
    if (spi_env != NULL && spi_env[0] != '\0' && access(spi_env, F_OK) == 0) {
        snprintf(g_spi_device, sizeof(g_spi_device), "%s", spi_env); return;
    }
    for (size_t i = 0; i < candidate_count; ++i) {
        if (access(candidates[i], F_OK) == 0) {
            snprintf(g_spi_device, sizeof(g_spi_device), "%s", candidates[i]); return;
        }
    }
    snprintf(g_spi_device, sizeof(g_spi_device), "%s", spi_env && spi_env[0] ? spi_env : "/dev/spidev0.1");
}


// ============================================================
//  RadioLib HAL / Module / TX/RX logic
// ============================================================

class LinuxRadioLibHal : public PiHal {
  public:
    LinuxRadioLibHal() : PiHal(0, 2000000, 0, 0) {}

    void pinMode(uint32_t pin, uint32_t mode) override {
        if (pin == RADIOLIB_NC) return;
        if (mode == GpioModeOutput) {
            if (pin == (uint32_t)g_lora_rst_gpio) {
                (void)gpio_init_output_any("LORA_RST_CHIP", "LORA_RST_OFFSET", (int)pin, 1, &g_lora_rst_fd, "RST");
            }
        } else {
            if (pin == (uint32_t)g_lora_busy_gpio) {
                (void)gpio_init_input_any("LORA_BUSY_CHIP", "LORA_BUSY_OFFSET", (int)pin, &g_lora_busy_fd, "BUSY");
            }
        }
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        if (pin == RADIOLIB_NC) return;
        int line_fd = -1;
        if (pin == (uint32_t)g_lora_rst_gpio) line_fd = g_lora_rst_fd;
        (void)gpio_set_value_any((int)pin, line_fd, value ? 1 : 0);
    }

    uint32_t digitalRead(uint32_t pin) override {
        if (pin == RADIOLIB_NC) return 0;
        int line_fd = -1;
        if (pin == (uint32_t)g_lora_busy_gpio) line_fd = g_lora_busy_fd;
        int value = gpio_get_value_any((int)pin, line_fd);
        return value > 0 ? 1U : 0U;
    }

    void attachInterrupt(uint32_t, void (*)(void), uint32_t) override {}
    void detachInterrupt(uint32_t) override {}
    void delay(RadioLibTime_t ms) override { usleep((useconds_t)(ms * 1000)); }
    void delayMicroseconds(RadioLibTime_t us) override { usleep((useconds_t)us); }
    RadioLibTime_t millis() override { return (RadioLibTime_t)get_monotonic_ms(); }
    RadioLibTime_t micros() override {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (RadioLibTime_t)ts.tv_sec * 1000000ULL + (RadioLibTime_t)ts.tv_nsec / 1000ULL;
    }
    long pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) override {
        RadioLibTime_t start = micros();
        while (micros() - start < timeout) {
            if (digitalRead(pin) == state) {
                RadioLibTime_t pulse_start = micros();
                while (micros() - start < timeout && digitalRead(pin) == state) {}
                return (long)(micros() - pulse_start);
            }
        }
        return 0;
    }
    void spiBegin() override {}
    void spiBeginTransaction() override {}
    void spiTransfer(uint8_t *out, size_t len, uint8_t *in) override {
        uint8_t dummy[512] = {0};
        uint8_t *tx = out ? out : dummy;
        uint8_t *rx = in ? in : dummy;
        if (len > sizeof(dummy)) len = sizeof(dummy);
        (void)lora_spi_transfer(tx, rx, len);
    }
    void spiEndTransaction() override {}
    void spiEnd() override {}
};

static LinuxRadioLibHal g_lora_radio_hal;
static Module *g_lora_radio_module = NULL;
static SX1262 *g_lora_radio = NULL;

static uint64_t get_monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void lora_set_diag_step(const char *step, int code, const char *detail)
{
    snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "%s%s%s | rc=%d",
             step ? step : "diag", (detail && detail[0]) ? " | " : "", (detail && detail[0]) ? detail : "", code);
    SLOGI("LoRa diag: %s", g_lora_last_diag);
}

static const char *lora_radiolib_status_text(int16_t state)
{
    switch (state) {
    case RADIOLIB_ERR_NONE: return "ok";
    case RADIOLIB_ERR_CHIP_NOT_FOUND: return "chip_not_found";
    case RADIOLIB_ERR_TX_TIMEOUT: return "tx_timeout";
    case RADIOLIB_ERR_RX_TIMEOUT: return "rx_timeout";
    case RADIOLIB_ERR_CRC_MISMATCH: return "crc_mismatch";
    case RADIOLIB_ERR_SPI_WRITE_FAILED: return "spi_write_failed";
    case RADIOLIB_ERR_SPI_CMD_TIMEOUT: return "spi_cmd_timeout";
    case RADIOLIB_ERR_SPI_CMD_INVALID: return "spi_cmd_invalid";
    case RADIOLIB_ERR_SPI_CMD_FAILED: return "spi_cmd_failed";
    default: return "radiolib_err";
    }
}

static void lora_capture_device_errors(const char *stage, uint16_t irq_status)
{
    if (!g_lora_initialized || g_lora_radio == NULL) return;
    SLOGI("LoRa error: %s irq=0x%04X", stage ? stage : "radio_err", irq_status);
    snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "%s irq=0x%04X", stage ? stage : "radio_err", irq_status);
}

static bool lora_send_text_packet(const char *payload)
{
    if (!g_lora_initialized || g_lora_radio == NULL) {
        SLOGI("LoRa TX: not initialized");
        return false;
    }
    if (payload == NULL || payload[0] == '\0') return false;
    if (g_lora_tx_in_progress) return false;
    snprintf(g_lora_last_tx, sizeof(g_lora_last_tx), "%s", payload);
    g_lora_has_sent_message = true;
    g_lora_tx_done = false;
    g_lora_rx_done = false;
    g_lora_pending_rx_after_tx = true;
    g_lora_tx_mode = false;
    g_lora_selected_tx_mode = false;
    (void)g_lora_radio->standby();
    int16_t state = g_lora_radio->startTransmit((uint8_t *)g_lora_last_tx, strlen(g_lora_last_tx));
    if (state != RADIOLIB_ERR_NONE) {
        g_lora_tx_in_progress = false;
        g_lora_pending_rx_after_tx = false;
        SLOGI("LoRa TX: startTransmit failed rc=%d(%s)", (int)state, lora_radiolib_status_text(state));
        return false;
    }
    g_lora_tx_in_progress = true;
    g_lora_tx_start_ms = g_lora_last_auto_tx_ms = get_monotonic_ms();
    SLOGI("LoRa TX: sending '%s'", g_lora_last_tx);
    return true;
}

static void lora_send_demo_packet(void)
{
    if (!g_lora_initialized || g_lora_radio == NULL) return;
    if (!g_lora_tx_mode) return;
    snprintf(g_lora_last_tx, sizeof(g_lora_last_tx), "Hello from M5 LoRa-1262 #%lu", (unsigned long)g_lora_tx_counter);
    g_lora_has_sent_message = true;
    g_lora_pending_rx_after_tx = false;
    g_lora_tx_done = false;
    g_lora_rx_done = false;
    int16_t state = g_lora_radio->startTransmit((uint8_t *)g_lora_last_tx, strlen(g_lora_last_tx));
    if (state != RADIOLIB_ERR_NONE) {
        g_lora_tx_in_progress = false;
        SLOGI("LoRa TX: demo startTransmit failed rc=%d(%s)", (int)state, lora_radiolib_status_text(state));
        return;
    }
    g_lora_tx_in_progress = true;
    g_lora_tx_start_ms = g_lora_last_auto_tx_ms = get_monotonic_ms();
    SLOGI("LoRa TX: demo sending '%s'", g_lora_last_tx);
    ++g_lora_tx_counter;
}

static void lora_start_receive_mode(void)
{
    if (!g_lora_initialized || g_lora_radio == NULL) {
        SLOGI("LoRa RX: startReceive skipped, not initialized");
        return;
    }
    if (g_lora_tx_in_progress) {
        SLOGI("LoRa RX: startReceive skipped, TX in progress");
        g_lora_pending_rx_after_tx = true;
        return;
    }
    g_lora_tx_mode = false;
    g_lora_selected_tx_mode = false;
    g_lora_pending_rx_after_tx = false;
    SLOGI("LoRa RX: startReceive()");
    int16_t state = g_lora_radio->startReceive();
    SLOGI("LoRa RX: startReceive rc=%d(%s)", (int)state, lora_radiolib_status_text(state));
    if (state != RADIOLIB_ERR_NONE) {
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "startReceive rc=%d(%s)", (int)state, lora_radiolib_status_text(state));
    }
}

static void lora_apply_mode(bool tx_mode)
{
    g_lora_selected_tx_mode = tx_mode;
    if (!g_lora_initialized || g_lora_radio == NULL) {
        SLOGI("LoRa mode: not initialized");
        return;
    }
    if (tx_mode) {
        g_lora_pending_rx_after_tx = false;
        g_lora_tx_mode = true;
        g_lora_last_auto_tx_ms = get_monotonic_ms();
        if (g_lora_tx_in_progress) {
            SLOGI("LoRa mode: TX already in progress");
            return;
        }
        int16_t state = g_lora_radio->standby();
        if (state == RADIOLIB_ERR_NONE) {
            SLOGI("LoRa mode: TX ready");
        } else {
            SLOGI("LoRa mode: set TX failed rc=%d(%s)", (int)state, lora_radiolib_status_text(state));
        }
    } else {
        if (g_lora_tx_in_progress) {
            g_lora_pending_rx_after_tx = true;
            SLOGI("LoRa mode: TX in progress, will RX after done");
            return;
        }
        g_lora_pending_rx_after_tx = false;
        g_lora_tx_mode = false;
        g_lora_last_auto_tx_ms = get_monotonic_ms();
        lora_start_receive_mode();
    }
}

static void lora_service_irq_once(void)
{
    if (!g_lora_initialized || g_lora_radio == NULL) return;

    bool irq_event = false;
    if (!g_lora_irq_poll_fallback && g_lora_irq_fd >= 0) {
        struct pollfd pfd;
        memset(&pfd, 0, sizeof(pfd));
        pfd.fd = g_lora_irq_fd;
        pfd.events = POLLIN | POLLPRI;
        if (poll(&pfd, 1, 0) > 0 && (pfd.revents & (POLLIN | POLLPRI))) {
            irq_event = true;
#if HAS_LINUX_GPIO_CDEV
            struct gpioevent_data event_data;
            while (read(g_lora_irq_fd, &event_data, sizeof(event_data)) == (ssize_t)sizeof(event_data)) {}
#else
            char value_buf[8];
            lseek(g_lora_irq_fd, 0, SEEK_SET);
            while (read(g_lora_irq_fd, value_buf, sizeof(value_buf)) > 0) { lseek(g_lora_irq_fd, 0, SEEK_SET); break; }
#endif
        }
    }

    uint32_t irq_flags = g_lora_radio->getIrqFlags();
    if (irq_flags != RADIOLIB_SX126X_IRQ_NONE || irq_event) {
        SLOGI("LoRa IRQ: event=%d flags=0x%08lX tx_in_progress=%d tx_mode=%d",
               irq_event ? 1 : 0, (unsigned long)irq_flags, g_lora_tx_in_progress ? 1 : 0, g_lora_tx_mode ? 1 : 0);
    }
    if (!irq_event && irq_flags == RADIOLIB_SX126X_IRQ_NONE) return;

    if (g_lora_tx_in_progress) {
        if (irq_flags & RADIOLIB_SX126X_IRQ_TX_DONE) {
            int16_t state = g_lora_radio->finishTransmit();
            if (state == RADIOLIB_ERR_NONE) {
                g_lora_tx_done = true;
            } else {
                g_lora_tx_in_progress = false;
                SLOGI("LoRa TX: finishTransmit failed rc=%d(%s)", (int)state, lora_radiolib_status_text(state));
            }
        } else if (irq_flags & RADIOLIB_SX126X_IRQ_TIMEOUT) {
            g_lora_tx_in_progress = false;
            g_lora_tx_start_ms = 0;
            lora_capture_device_errors("TX irq timeout", 0);
            if (g_lora_pending_rx_after_tx || !g_lora_tx_mode) lora_start_receive_mode();
        }
        return;
    }

    if (irq_flags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        uint8_t rx_buf[sizeof(g_lora_last_rx)] = {0};
        int16_t state = g_lora_radio->readData(rx_buf, sizeof(g_lora_last_rx) - 1);
        SLOGI("LoRa RX: readData rc=%d(%s)", (int)state, lora_radiolib_status_text(state));
        if (state == RADIOLIB_ERR_NONE) {
            memcpy(g_lora_last_rx, rx_buf, sizeof(g_lora_last_rx));
            g_lora_last_rx[sizeof(g_lora_last_rx) - 1] = '\0';
            g_lora_last_rssi = g_lora_radio->getRSSI();
            g_lora_last_snr = g_lora_radio->getSNR();
            g_lora_rx_done = true;
            SLOGI("LoRa RX OK: '%s' RSSI=%.1f SNR=%.1f", g_lora_last_rx, g_lora_last_rssi, g_lora_last_snr);
        } else if (state != RADIOLIB_ERR_CRC_MISMATCH) {
            snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "readData rc=%d(%s)", (int)state, lora_radiolib_status_text(state));
        }
        if (!g_lora_tx_mode) lora_start_receive_mode();
    } else if (irq_flags & (RADIOLIB_SX126X_IRQ_CRC_ERR | RADIOLIB_SX126X_IRQ_HEADER_ERR)) {
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "RX crc/header error irq=0x%04lX", (unsigned long)irq_flags);
        SLOGI("LoRa RX error: %s", g_lora_last_diag);
        if (!g_lora_tx_mode) lora_start_receive_mode();
    } else if (irq_flags & RADIOLIB_SX126X_IRQ_TIMEOUT) {
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "RX timeout irq=0x%04lX", (unsigned long)irq_flags);
        SLOGI("LoRa RX timeout: %s", g_lora_last_diag);
    }
}

static void lora_check_tx_fallback(void)
{
    if (!g_lora_initialized || !g_lora_tx_in_progress || g_lora_radio == NULL) return;
    uint64_t now_ms = get_monotonic_ms();
    if (g_lora_tx_start_ms != 0 && now_ms - g_lora_tx_start_ms >= 4000ULL) {
        g_lora_tx_in_progress = false;
        g_lora_tx_start_ms = 0;
        g_lora_last_auto_tx_ms = now_ms;
        lora_capture_device_errors("TX timeout", 0);
        (void)g_lora_radio->standby();
        if (g_lora_pending_rx_after_tx || !g_lora_tx_mode) lora_start_receive_mode();
    }
}

static bool g_lora_rx_event = false;
static bool g_lora_tx_event = false;

static void lora_poll_hardware(void)
{
    if (!g_lora_initialized) return;
    lora_service_irq_once();
    lora_check_tx_fallback();

    if (g_lora_tx_done) {
        g_lora_tx_done = false;
        g_lora_tx_event = true;
        g_lora_tx_in_progress = false;
        g_lora_tx_start_ms = 0;
        if (g_lora_pending_rx_after_tx || !g_lora_tx_mode) {
            lora_start_receive_mode();
        }
    }

    if (g_lora_rx_done) {
        g_lora_rx_done = false;
        g_lora_rx_event = true;
    }

    if (g_lora_initialized && g_lora_tx_mode && !g_lora_tx_in_progress) {
        uint64_t now_ms = get_monotonic_ms();
        if (now_ms - g_lora_last_auto_tx_ms >= 2000ULL) {
            lora_send_demo_packet();
        }
    }
}


// ============================================================
//  Hardware initialization
// ============================================================

static void lora_init_hardware(void)
{
    delete g_lora_radio; g_lora_radio = NULL;
    delete g_lora_radio_module; g_lora_radio_module = NULL;

    lora_set_diag_step("i2c_scan", 0, "scan 0x43 before LoRa init");
    if (pi4io_scan_and_init_before_lora()) {
        lora_set_diag_step("i2c_scan", 0, g_pi4io_status);
    } else {
        lora_set_diag_step("i2c_scan", 1, g_pi4io_status);
    }

    lora_set_diag_step("power_enable", 0, "start");
    if (!hat_5vout_enable()) {
        SLOGI("Status: GPIO5 low set failed");
        lora_set_diag_step("power_enable", 1, "GPIO5 low set failed");
    }
    usleep(100000);

    lora_set_diag_step("reset_gpio_init", 0, "prepare rst pin");
    if (gpio_init_output_any("LORA_RST_CHIP", "LORA_RST_OFFSET", g_lora_rst_gpio, 1, &g_lora_rst_fd, "RST") < 0) {
        g_lora_initialized = false; g_lora_hw_ready = false;
        lora_set_diag_step("reset_gpio_init", 1, "rst gpio init failed");
        return;
    }

    if (gpio_init_input_any("LORA_BUSY_CHIP", "LORA_BUSY_OFFSET", g_lora_busy_gpio, &g_lora_busy_fd, "BUSY") < 0) {
        g_lora_initialized = false; g_lora_hw_ready = false;
        lora_set_diag_step("busy_gpio_init", 1, "busy gpio init failed");
        return;
    }

    lora_set_diag_step("hard_reset", 0, "toggle rst before probe");
    if (!sx1262_reset()) {
        g_lora_initialized = false; g_lora_hw_ready = false;
        lora_set_diag_step("hard_reset", 1, "rst/busy handshake failed");
        return;
    }

    lora_set_diag_step("resolve_spi", 0, "detect device");
    resolve_lora_spi_device();

    if (!probe_lora_spi_device()) {
        g_lora_initialized = false; g_lora_hw_ready = false;
        lora_set_diag_step("probe_spi", 1, g_lora_last_diag);
        return;
    }

    lora_set_diag_step("pre_begin_prepare", 0, "reset again before RadioLib begin");
    if (!sx1262_reset()) {
        g_lora_initialized = false; g_lora_hw_ready = false;
        lora_set_diag_step("pre_begin_prepare", 1, "rst/busy handshake failed before RadioLib begin");
        return;
    }

    lora_set_diag_step("prepare_irq", 0, "init irq pin");
    if (gpio_init_input_irq_any("LORA_IRQ_CHIP", "LORA_IRQ_OFFSET", g_lora_irq_gpio, &g_lora_irq_fd, "IRQ") < 0) {
        g_lora_irq_poll_fallback = true;
        lora_set_diag_step("prepare_irq", 1, "irq gpio init failed, fallback=poll");
    } else {
        g_lora_irq_poll_fallback = false;
        lora_set_diag_step("prepare_irq", 0, "irq gpio ok");
    }

    lora_set_diag_step("runtime_spi", 0, "open SPI for RadioLib runtime");
    if (!lora_open_runtime_spi()) {
        g_lora_initialized = false; g_lora_hw_ready = false;
        lora_set_diag_step("runtime_spi", 1, g_lora_last_diag);
        return;
    }

    lora_set_diag_step("radiolib_setup", 0, "create module");
    g_lora_nss_manual = false;
    g_lora_radio_module = new Module(&g_lora_radio_hal, RADIOLIB_NC,
                                     (uint32_t)g_lora_irq_gpio, (uint32_t)g_lora_rst_gpio, (uint32_t)g_lora_busy_gpio);
    g_lora_radio = new SX1262(g_lora_radio_module);

    if (g_lora_radio_module == NULL || g_lora_radio == NULL) {
        g_lora_initialized = false; g_lora_hw_ready = false;
        lora_set_diag_step("radiolib_setup", 1, "allocation failed");
        return;
    }

    lora_set_diag_step("radiolib_begin", 0, "configure sx1262 via RadioLib");
    int16_t state = g_lora_radio->begin(
        868.0f,   // frequency MHz
        125.0f,   // bandwidth kHz
        12,       // spreading factor
        5,        // coding rate 4/5
        0x34,     // sync word
        22,       // output power dBm
        20,       // preamble length
        3.0f,     // TCXO voltage
        false
    );

    if (state != RADIOLIB_ERR_NONE) {
        g_lora_initialized = false; g_lora_hw_ready = false;
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "RadioLib begin rc=%d(%s)", (int)state, lora_radiolib_status_text(state));
        SLOGI("LoRa init failed: rc=%d (%s)", (int)state, lora_radiolib_status_text(state));
        lora_set_diag_step("radiolib_begin", state, g_lora_last_diag);
        return;
    }

    (void)g_lora_radio->setCurrentLimit(140);
    (void)g_lora_radio->setDio2AsRfSwitch(true);

    g_lora_initialized = true;
    g_lora_hw_ready = true;
    g_lora_tx_mode = false;
    g_lora_selected_tx_mode = false;
    g_lora_tx_in_progress = false;
    g_lora_pending_rx_after_tx = false;
    g_lora_tx_start_ms = 0;
    g_lora_last_auto_tx_ms = get_monotonic_ms();

    lora_set_diag_step("ready", 0, "LoRa init finished");
    SLOGI("LoRa: init done, auto enter RX");
    lora_start_receive_mode();
}




static void fill_info(cp0_lora_info_t *info, bool drain_events)
{
    if (!info) return;
    memset(info, 0, sizeof(*info));
    info->initialized = g_lora_initialized ? 1 : 0;
    info->hw_ready = g_lora_hw_ready ? 1 : 0;
    info->tx_mode = g_lora_tx_mode ? 1 : 0;
    info->tx_in_progress = g_lora_tx_in_progress ? 1 : 0;
    info->has_sent_message = g_lora_has_sent_message ? 1 : 0;
    info->rx_event = g_lora_rx_event ? 1 : 0;
    info->tx_event = g_lora_tx_event ? 1 : 0;
    cp0_copy_cstr(info->spi_device, sizeof(info->spi_device), g_spi_device);
    cp0_copy_cstr(info->last_rx, sizeof(info->last_rx), g_lora_last_rx);
    cp0_copy_cstr(info->last_tx, sizeof(info->last_tx), g_lora_last_tx);
    cp0_copy_cstr(info->diag, sizeof(info->diag), g_lora_last_diag);
    cp0_copy_cstr(info->probe_summary, sizeof(info->probe_summary), g_lora_probe_summary);
    cp0_copy_cstr(info->probe_display, sizeof(info->probe_display), g_lora_probe_display);
    cp0_copy_cstr(info->pi4io_status, sizeof(info->pi4io_status), g_pi4io_status);
    info->rssi = g_lora_last_rssi;
    info->snr = g_lora_last_snr;
    if (drain_events) {
        g_lora_rx_event = false;
        g_lora_tx_event = false;
    }
}

} // namespace cp0_lora_backend

class LoraSystem
{
public:
    void api_call(std::list<std::string> arg, std::function<void(int, std::string)> callback)
    {
        if (arg.empty()) {
            report(callback, -1, "missing lora api command\n");
            return;
        }

        const std::string command = arg.front();
        if (command == "Init") {
            if (!cp0_lora_backend::g_lora_initialized && !cp0_lora_backend::g_lora_hw_ready)
                cp0_lora_backend::lora_init_hardware();
            report(callback, cp0_lora_backend::g_lora_hw_ready ? 0 : -1, "");
            return;
        }
        if (command == "Poll" || command == "Info") {
            cp0_lora_info_t info{};
            if (command == "Poll") cp0_lora_backend::lora_poll_hardware();
            cp0_lora_backend::fill_info(&info, command == "Poll");
            report(callback, 0, std::string(reinterpret_cast<const char *>(&info), sizeof(info)));
            return;
        }
        if (command == "SendText") {
            std::string payload = first_arg_after_command(arg);
            report(callback, cp0_lora_backend::lora_send_text_packet(payload.c_str()) ? 0 : -1, "");
            return;
        }
        if (command == "StartReceive") {
            cp0_lora_backend::lora_start_receive_mode();
            report(callback, 0, "");
            return;
        }
        if (command == "SetTxMode") {
            cp0_lora_backend::lora_apply_mode(std::atoi(first_arg_after_command(arg).c_str()) != 0);
            report(callback, 0, "");
            return;
        }
        if (command == "Shutdown") {
            shutdown();
            report(callback, 0, "");
            return;
        }

        report(callback, -1, "unknown lora api\n");
    }

private:
    static std::string first_arg_after_command(const std::list<std::string>& arg)
    {
        if (arg.size() < 2) return "";
        return *std::next(arg.begin());
    }

    static void report(std::function<void(int, std::string)> callback, int code, const std::string& data)
    {
        if (callback) callback(code, data);
    }

    static void shutdown()
    {
        using namespace cp0_lora_backend;
        delete g_lora_radio;
        g_lora_radio = NULL;
        delete g_lora_radio_module;
        g_lora_radio_module = NULL;
        if (g_spi_fd >= 0) { close(g_spi_fd); g_spi_fd = -1; }
        if (g_lora_rst_fd >= 0) { close(g_lora_rst_fd); g_lora_rst_fd = -1; }
        if (g_lora_busy_fd >= 0) { close(g_lora_busy_fd); g_lora_busy_fd = -1; }
        if (g_lora_irq_fd >= 0) { close(g_lora_irq_fd); g_lora_irq_fd = -1; }
        if (g_lora_nss_fd >= 0) { close(g_lora_nss_fd); g_lora_nss_fd = -1; }
        if (g_hat_5vout_fd >= 0) { close(g_hat_5vout_fd); g_hat_5vout_fd = -1; }
        g_lora_initialized = false;
        g_lora_hw_ready = false;
    }
};

extern "C" void init_lora(void)
{
    std::shared_ptr<LoraSystem> lora = std::make_shared<LoraSystem>();
    cp0_signal_lora_api.append([lora](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        lora->api_call(arg, callback);
    });
}
