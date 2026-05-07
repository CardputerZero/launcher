#include "lvgl/lvgl.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <dirent.h>
#include <cstdarg>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#if __has_include(<linux/input.h>)
#include <linux/input.h>
#else
struct input_event {
    struct timeval time;
    unsigned short type;
    unsigned short code;
    int value;
};
#ifndef EV_KEY
#define EV_KEY 0x01
#endif
#ifndef KEY_ESC
#define KEY_ESC 1
#endif
#ifndef KEY_BACKSPACE
#define KEY_BACKSPACE 14
#endif
#ifndef KEY_DELETE
#define KEY_DELETE 111
#endif
#ifndef KEY_ENTER
#define KEY_ENTER 28
#endif
#ifndef KEY_KPENTER
#define KEY_KPENTER 96
#endif
#ifndef KEY_OK
#define KEY_OK 352
#endif
#ifndef KEY_SELECT
#define KEY_SELECT 353
#endif
#ifndef KEY_MENU
#define KEY_MENU 139
#endif
#ifndef KEY_HOME
#define KEY_HOME 102
#endif
#ifndef KEY_END
#define KEY_END 107
#endif
#ifndef KEY_PAGEUP
#define KEY_PAGEUP 104
#endif
#ifndef KEY_PAGEDOWN
#define KEY_PAGEDOWN 109
#endif
#ifndef KEY_A
#define KEY_A 30
#endif
#ifndef KEY_Q
#define KEY_Q 16
#endif
#ifndef KEY_E
#define KEY_E 18
#endif
#ifndef KEY_R
#define KEY_R 19
#endif
#ifndef KEY_T
#define KEY_T 20
#endif
#ifndef KEY_Y
#define KEY_Y 21
#endif
#ifndef KEY_U
#define KEY_U 22
#endif
#ifndef KEY_I
#define KEY_I 23
#endif
#ifndef KEY_O
#define KEY_O 24
#endif
#ifndef KEY_P
#define KEY_P 25
#endif
#ifndef KEY_B
#define KEY_B 48
#endif
#ifndef KEY_SPACE
#define KEY_SPACE 57
#endif
#ifndef KEY_C
#define KEY_C 46
#endif
#ifndef KEY_D
#define KEY_D 32
#endif
#ifndef KEY_H
#define KEY_H 35
#endif
#ifndef KEY_J
#define KEY_J 36
#endif
#ifndef KEY_K
#define KEY_K 37
#endif
#ifndef KEY_L
#define KEY_L 38
#endif
#ifndef KEY_S
#define KEY_S 31
#endif
#ifndef KEY_W
#define KEY_W 17
#endif
#ifndef KEY_Z
#define KEY_Z 44
#endif
#ifndef KEY_X
#define KEY_X 45
#endif
#ifndef KEY_C
#define KEY_C 46
#endif
#ifndef KEY_V
#define KEY_V 47
#endif
#ifndef KEY_N
#define KEY_N 49
#endif
#ifndef KEY_M
#define KEY_M 50
#endif
#ifndef KEY_1
#define KEY_1 2
#endif
#ifndef KEY_2
#define KEY_2 3
#endif
#ifndef KEY_3
#define KEY_3 4
#endif
#ifndef KEY_4
#define KEY_4 5
#endif
#ifndef KEY_5
#define KEY_5 6
#endif
#ifndef KEY_6
#define KEY_6 7
#endif
#ifndef KEY_7
#define KEY_7 8
#endif
#ifndef KEY_8
#define KEY_8 9
#endif
#ifndef KEY_9
#define KEY_9 10
#endif
#ifndef KEY_0
#define KEY_0 11
#endif
#ifndef KEY_UP
#define KEY_UP 103
#endif
#ifndef KEY_LEFT
#define KEY_LEFT 105
#endif
#ifndef KEY_RIGHT
#define KEY_RIGHT 106
#endif
#ifndef KEY_DOWN
#define KEY_DOWN 108
#endif
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
#ifndef SPI_NO_CS
#define SPI_NO_CS 0x40
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
#if __has_include(<linux/i2c.h>)
#include <linux/i2c.h>
#define USERDEMO_HAS_LINUX_I2C_RDWR 1
#else
#define USERDEMO_HAS_LINUX_I2C_RDWR 0
#endif
#define USERDEMO_HAS_LINUX_I2CDEV 1
#else
#define USERDEMO_HAS_LINUX_I2CDEV 0
#define USERDEMO_HAS_LINUX_I2C_RDWR 0
#ifndef I2C_SLAVE
#define I2C_SLAVE 0x0703
#endif
#endif
#include "ui/ui.h"
#include "main.h"
#include <cstring>
#include "RadioLib.h"
#if __has_include(<lgpio.h>)
#include "hal/RPi/PiHal.h"
#define USERDEMO_HAS_PIHAL 1
#else
#define USERDEMO_HAS_PIHAL 0

class PiHal : public RadioLibHal {
  public:
    PiHal(uint8_t spiChannel, uint32_t spiSpeed = 2000000, uint8_t spiDevice = 0, uint8_t gpioDevice = 0)
      : RadioLibHal(0, 1, 0, 1, 1, 2),
        _gpioDevice(gpioDevice),
        _spiDevice(spiDevice),
        _spiSpeed(spiSpeed),
        _spiChannel(spiChannel) {
    }

  protected:
    uint8_t _gpioDevice;
    uint8_t _spiDevice;
    uint32_t _spiSpeed;
    uint8_t _spiChannel;
};
#endif

static const char *getenv_default(const char *name, const char *dflt);
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
static void show_host_ip(void);
static void render_home_menu(void);
static void enter_selected_app(void);
static void return_to_home_menu(void);
static void render_cc1101_page(void);
static void render_nfc_page(void);
static void home_card_event_cb(lv_event_t *e);
static void app_page_click_event_cb(lv_event_t *e);
static void ensure_home_widgets(void);
static void set_detail_widgets_hidden(bool hidden);
static void set_home_widgets_hidden(bool hidden);
static bool is_menu_prev_key(uint32_t key);
static bool is_menu_next_key(uint32_t key);
static bool handle_app_key(uint32_t key);
static bool raw_keyboard_init(void);
static void raw_keyboard_poll(void);
static void update_home_carousel(void);
static bool open_raw_keyboard_device(const char *device);
static void raw_keyboard_close_all(void);
static void raw_keyboard_probe_directory(const char *dir_path, const char *name_filter);
static const char *linux_key_code_name(unsigned short code);

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
static bool g_lora_irq_poll_fallback = true;
static volatile bool g_lora_rx_done = false;
static volatile bool g_lora_tx_done = false;
static uint32_t g_lora_tx_counter = 0;
static uint64_t g_lora_tx_start_ms = 0;
static uint64_t g_lora_sent_popup_until_ms = 0;
static char g_lora_last_rx[128] = {0};
static char g_lora_last_tx[128] = "Hello from M5 LoRa-1262";
static char g_lora_tx_input[128] = "";
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
static int g_keyboard_fds[16] = {-1};
static char g_keyboard_devices[16][160] = {{0}};
static int g_keyboard_fd_count = 0;
static char g_keyboard_device[160] = "/dev/input/by-path/platform-3f804000.i2c-event";
static bool g_keyboard_ready_reported = false;
static bool g_keyboard_shift_pressed = false;

enum AppPage {
    APP_PAGE_HOME = 0,
    APP_PAGE_LORA,
    APP_PAGE_CC1101,
    APP_PAGE_NFC,
};

enum LoraView {
    LORA_VIEW_MESSAGES = 0,
    LORA_VIEW_INFO,
    LORA_VIEW_SEND,
};

static const char *g_app_names[] = {
    "LoRa",
    "CC1101",
    "NFC",
};

static int g_menu_index = 0;
static AppPage g_current_page = APP_PAGE_HOME;
static LoraView g_lora_view = LORA_VIEW_MESSAGES;
static bool g_lora_hw_ready = false;
static lv_obj_t *g_home_icon_card[3] = {NULL, NULL, NULL};
static lv_obj_t *g_home_icon_label[3] = {NULL, NULL, NULL};
static lv_obj_t *g_home_name_label[3] = {NULL, NULL, NULL};
static lv_obj_t *g_home_page_indicator[3] = {NULL, NULL, NULL};
static lv_obj_t *g_home_hint_label = NULL;
static lv_obj_t *g_lora_topbar_bg = NULL;
static lv_obj_t *g_lora_topbar_zero = NULL;
static lv_obj_t *g_lora_topbar_time = NULL;
static lv_obj_t *g_lora_topbar_power = NULL;

static void lora_apply_mode(bool tx_mode);
static void lora_start_receive_mode(void);
static void lora_send_demo_packet(void);
static void lora_render_page(void);
static void lora_init_hardware(void);
static void lora_service_irq_once(void);
static void lora_check_tx_fallback(void);
static void lora_set_diag_step(const char *step, int code, const char *detail);
static void update_lora_mode_ui(void);
static void lora_refresh_status(const char *prefix);
static uint64_t get_monotonic_ms(void);
static const char *get_cached_host_ip(void);
static void lora_capture_device_errors(const char *stage, uint16_t irq_status);
static void lora_show_pin_info(void);
static void lora_show_radio_config(void);
static void lora_render_current_view(void);
static void lora_render_messages_view(void);
static void lora_render_info_view(void);
static void lora_render_send_view(void);
static void lora_render_sent_popup(void);
static void lora_open_send_view(uint32_t first_key);
static bool lora_send_text_packet(const char *payload);
static const char *lora_radiolib_status_text(int16_t state);
static bool is_lora_text_key(uint32_t key);
static char lora_key_to_char(uint32_t key);
static void lora_set_topbar_bg(bool visible);

class LinuxRadioLibHal : public PiHal {
  public:
    LinuxRadioLibHal()
        : PiHal(0, 2000000, 0, 0) {
    }

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

static char g_cached_host_ip[INET_ADDRSTRLEN] = "";
static uint64_t g_cached_host_ip_ms = 0;

static void ensure_home_widgets(void)
{
    if (g_home_icon_card[0] != NULL || ui_Screen1 == NULL) {
        return;
    }

    for (int i = 0; i < 3; ++i) {
        g_home_icon_card[i] = lv_obj_create(ui_Screen1);
        lv_obj_remove_style_all(g_home_icon_card[i]);
        lv_obj_add_flag(g_home_icon_card[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(g_home_icon_card[i], 16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(g_home_icon_card[i], LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_home_icon_card[i], 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(g_home_icon_card[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(g_home_icon_card[i], home_card_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(g_home_icon_card[i], home_card_event_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(g_home_icon_card[i], home_card_event_cb, LV_EVENT_PRESSED, (void *)(intptr_t)i);

        g_home_icon_label[i] = lv_label_create(g_home_icon_card[i]);
        lv_obj_center(g_home_icon_label[i]);
        lv_obj_set_style_text_font(g_home_icon_label[i], &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(g_home_icon_label[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

        g_home_name_label[i] = lv_label_create(ui_Screen1);
        lv_obj_set_style_text_font(g_home_name_label[i], &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(g_home_name_label[i], lv_color_hex(0xBFBFBF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    for (int i = 0; i < 3; ++i) {
        g_home_page_indicator[i] = lv_obj_create(ui_Screen1);
        lv_obj_remove_style_all(g_home_page_indicator[i]);
        lv_obj_set_size(g_home_page_indicator[i], 8, 8);
        lv_obj_set_align(g_home_page_indicator[i], LV_ALIGN_CENTER);
        lv_obj_set_x(g_home_page_indicator[i], (i - 1) * 16);
        lv_obj_set_y(g_home_page_indicator[i], 65);
        lv_obj_set_style_radius(g_home_page_indicator[i], LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(g_home_page_indicator[i], 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(g_home_page_indicator[i], lv_color_hex(0xD8D8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    g_home_hint_label = lv_label_create(ui_Screen1);
    lv_obj_set_align(g_home_hint_label, LV_ALIGN_CENTER);
    lv_obj_set_y(g_home_hint_label, 76);
    lv_obj_set_style_text_font(g_home_hint_label, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_home_hint_label, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);

    if (ui_apppage != NULL) {
        lv_obj_add_flag(ui_apppage, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui_apppage, app_page_click_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ui_apppage, app_page_click_event_cb, LV_EVENT_SHORT_CLICKED, NULL);
        lv_obj_add_event_cb(ui_apppage, app_page_click_event_cb, LV_EVENT_PRESSED, NULL);
    }
}

static void set_detail_widgets_hidden(bool hidden)
{
    if (ui_Label2) hidden ? lv_obj_add_flag(ui_Label2, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(ui_Label2, LV_OBJ_FLAG_HIDDEN);
    if (ui_loraPins) hidden ? lv_obj_add_flag(ui_loraPins, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(ui_loraPins, LV_OBJ_FLAG_HIDDEN);
    if (ui_loraDevice) hidden ? lv_obj_add_flag(ui_loraDevice, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(ui_loraDevice, LV_OBJ_FLAG_HIDDEN);
    if (ui_loraMode) hidden ? lv_obj_add_flag(ui_loraMode, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(ui_loraMode, LV_OBJ_FLAG_HIDDEN);
    if (ui_loraStatus) hidden ? lv_obj_add_flag(ui_loraStatus, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(ui_loraStatus, LV_OBJ_FLAG_HIDDEN);
    if (ui_loraHint) hidden ? lv_obj_add_flag(ui_loraHint, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(ui_loraHint, LV_OBJ_FLAG_HIDDEN);
}

static void set_home_widgets_hidden(bool hidden)
{
    ensure_home_widgets();
    for (int i = 0; i < 3; ++i) {
        if (g_home_icon_card[i]) hidden ? lv_obj_add_flag(g_home_icon_card[i], LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(g_home_icon_card[i], LV_OBJ_FLAG_HIDDEN);
        if (g_home_name_label[i]) hidden ? lv_obj_add_flag(g_home_name_label[i], LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(g_home_name_label[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < 3; ++i) {
        if (g_home_page_indicator[i]) hidden ? lv_obj_add_flag(g_home_page_indicator[i], LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(g_home_page_indicator[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (g_home_hint_label) hidden ? lv_obj_add_flag(g_home_hint_label, LV_OBJ_FLAG_HIDDEN) : lv_obj_clear_flag(g_home_hint_label, LV_OBJ_FLAG_HIDDEN);
}

static bool is_menu_prev_key(uint32_t key)
{
    return key == LV_KEY_LEFT || key == LV_KEY_PREV || key == 'z' || key == 'Z';
}

static bool is_menu_next_key(uint32_t key)
{
    return key == LV_KEY_RIGHT || key == LV_KEY_NEXT || key == 'c' || key == 'C';
}

static bool is_lora_text_key(uint32_t key)
{
    return (key >= 'A' && key <= 'Z') ||
           (key >= 'a' && key <= 'z') ||
           (key >= '0' && key <= '9') ||
           key == ' ' || key == '-' || key == '_' || key == '.' || key == ',' ||
           key == '!' || key == '?' || key == '#';
}

static char lora_key_to_char(uint32_t key)
{
    if (key >= 'A' && key <= 'Z') {
        return (char)key;
    }
    if (key >= 'a' && key <= 'z') {
        return (char)key;
    }
    if ((key >= '0' && key <= '9') || key == ' ' || key == '-' || key == '_' ||
        key == '.' || key == ',' || key == '!' || key == '?' || key == '#') {
        return (char)key;
    }
    return '\0';
}

static uint32_t linux_key_to_app_key(unsigned short code, bool shift_pressed)
{
    switch (code) {
    case KEY_Q: return shift_pressed ? 'Q' : 'q';
    case KEY_W: return shift_pressed ? 'W' : 'w';
    case KEY_E: return shift_pressed ? 'E' : 'e';
    case KEY_R: return shift_pressed ? 'R' : 'r';
    case KEY_T: return shift_pressed ? 'T' : 't';
    case KEY_Y: return shift_pressed ? 'Y' : 'y';
    case KEY_U: return shift_pressed ? 'U' : 'u';
    case KEY_I: return shift_pressed ? 'I' : 'i';
    case KEY_O: return shift_pressed ? 'O' : 'o';
    case KEY_P: return shift_pressed ? 'P' : 'p';
    case KEY_A: return shift_pressed ? 'A' : 'a';
    case KEY_S: return shift_pressed ? 'S' : 's';
    case KEY_D: return shift_pressed ? 'D' : 'd';
    case KEY_H: return shift_pressed ? 'H' : 'h';
    case KEY_J: return shift_pressed ? 'J' : 'j';
    case KEY_K: return shift_pressed ? 'K' : 'k';
    case KEY_L: return shift_pressed ? 'L' : 'l';
    case KEY_Z: return shift_pressed ? 'Z' : 'z';
    case KEY_X: return shift_pressed ? 'X' : 'x';
    case KEY_C: return shift_pressed ? 'C' : 'c';
    case KEY_V: return shift_pressed ? 'V' : 'v';
    case KEY_B: return shift_pressed ? 'B' : 'b';
    case KEY_N: return shift_pressed ? 'N' : 'n';
    case KEY_M: return shift_pressed ? 'M' : 'm';
    case KEY_1: return '1';
    case KEY_2: return '2';
    case KEY_3: return '3';
    case KEY_4: return '4';
    case KEY_5: return '5';
    case KEY_6: return '6';
    case KEY_7: return '7';
    case KEY_8: return '8';
    case KEY_9: return '9';
    case KEY_0: return '0';
    case KEY_SPACE: return ' ';
    case KEY_LEFT:
    case KEY_HOME:
        return LV_KEY_LEFT;
    case KEY_UP:
    case KEY_PAGEUP:
        return LV_KEY_UP;
    case KEY_RIGHT:
    case KEY_END:
        return LV_KEY_RIGHT;
    case KEY_DOWN:
    case KEY_PAGEDOWN:
        return LV_KEY_DOWN;
    case KEY_ENTER:
    case KEY_KPENTER:
    case KEY_OK:
    case KEY_SELECT:
    case KEY_MENU:
        return LV_KEY_ENTER;
    case KEY_ESC:
        return LV_KEY_ESC;
    case KEY_DELETE:
        return LV_KEY_DEL;
    case KEY_BACKSPACE:
        return LV_KEY_BACKSPACE;
    default:
        return 0;
    }
#if 0
    switch (code) {
    case KEY_Z:
    case KEY_A:
    case KEY_H:
    case KEY_LEFT:
    case KEY_HOME:
        return LV_KEY_LEFT;
    case KEY_W:
    case KEY_K:
    case KEY_UP:
    case KEY_PAGEUP:
        return LV_KEY_UP;
    case KEY_C:
    case KEY_D:
    case KEY_L:
    case KEY_RIGHT:
    case KEY_END:
        return LV_KEY_RIGHT;
    case KEY_S:
    case KEY_J:
    case KEY_DOWN:
    case KEY_PAGEDOWN:
        return LV_KEY_DOWN;
    case KEY_ENTER:
    case KEY_KPENTER:
    case KEY_OK:
    case KEY_SELECT:
    case KEY_MENU:
    case KEY_B:
        return LV_KEY_ENTER;
    case KEY_SPACE:
        return LV_KEY_ENTER;
    case KEY_ESC:
        return LV_KEY_ESC;
    case KEY_DELETE:
        return LV_KEY_DEL;
    case KEY_BACKSPACE:
        return LV_KEY_BACKSPACE;
    default:
        return 0;
    }
#endif
}

static const char *linux_key_code_name(unsigned short code)
{
    switch (code) {
    case KEY_Z: return "KEY_Z";
    case KEY_A: return "KEY_A";
    case KEY_H: return "KEY_H";
    case KEY_LEFT: return "KEY_LEFT";
    case KEY_HOME: return "KEY_HOME";
    case KEY_W: return "KEY_W";
    case KEY_K: return "KEY_K";
    case KEY_UP: return "KEY_UP";
    case KEY_PAGEUP: return "KEY_PAGEUP";
    case KEY_C: return "KEY_C";
    case KEY_D: return "KEY_D";
    case KEY_L: return "KEY_L";
    case KEY_RIGHT: return "KEY_RIGHT";
    case KEY_END: return "KEY_END";
    case KEY_S: return "KEY_S";
    case KEY_J: return "KEY_J";
    case KEY_DOWN: return "KEY_DOWN";
    case KEY_PAGEDOWN: return "KEY_PAGEDOWN";
    case KEY_ENTER: return "KEY_ENTER";
    case KEY_KPENTER: return "KEY_KPENTER";
    case KEY_OK: return "KEY_OK";
    case KEY_SELECT: return "KEY_SELECT";
    case KEY_MENU: return "KEY_MENU";
    case KEY_B: return "KEY_B";
    case KEY_SPACE: return "KEY_SPACE";
    case KEY_ESC: return "KEY_ESC";
    case KEY_DELETE: return "KEY_DELETE";
    case KEY_BACKSPACE: return "KEY_BACKSPACE";
    default: return "UNKNOWN";
    }
}

static void home_card_event_cb(lv_event_t *e)
{
    if (e == NULL) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_PRESSED) {
        return;
    }

    intptr_t slot = (intptr_t)lv_event_get_user_data(e);
    if (slot < 0 || slot > 2) {
        return;
    }

    g_menu_index = (g_menu_index + (int)slot + 2) % 3;
    render_home_menu();
    enter_selected_app();
}

static void app_page_click_event_cb(lv_event_t *e)
{
    if (e == NULL) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_PRESSED) {
        return;
    }

    if (g_current_page == APP_PAGE_HOME) {
        enter_selected_app();
    } else if (g_current_page == APP_PAGE_LORA) {
        lora_apply_mode(g_lora_selected_tx_mode);
    }
}

static bool open_raw_keyboard_device(const char *device)
{
    if (device == NULL || device[0] == '\0') {
        return false;
    }

    for (int i = 0; i < g_keyboard_fd_count; ++i) {
        if (strcmp(g_keyboard_devices[i], device) == 0) {
            return true;
        }
    }

    if (g_keyboard_fd_count >= (int)(sizeof(g_keyboard_fds) / sizeof(g_keyboard_fds[0]))) {
        return false;
    }

    int fd = open(device, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    g_keyboard_fds[g_keyboard_fd_count] = fd;
    snprintf(g_keyboard_devices[g_keyboard_fd_count], sizeof(g_keyboard_devices[g_keyboard_fd_count]), "%s", device);
    ++g_keyboard_fd_count;
    snprintf(g_keyboard_device, sizeof(g_keyboard_device), "%s", device);
    printf("Raw keyboard enabled: %s\n", device);
    g_keyboard_ready_reported = true;
    return true;
}

static void raw_keyboard_close_all(void)
{
    for (int i = 0; i < g_keyboard_fd_count; ++i) {
        if (g_keyboard_fds[i] >= 0) {
            close(g_keyboard_fds[i]);
            g_keyboard_fds[i] = -1;
        }
        g_keyboard_devices[i][0] = '\0';
    }
    g_keyboard_fd_count = 0;
}

static void raw_keyboard_probe_directory(const char *dir_path, const char *name_filter)
{
    if (dir_path == NULL || dir_path[0] == '\0') {
        return;
    }

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        return;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (name_filter != NULL && name_filter[0] != '\0' && strstr(entry->d_name, name_filter) == NULL) {
            continue;
        }

        char full_path[256];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        (void)open_raw_keyboard_device(full_path);
    }

    closedir(dir);
}

static bool raw_keyboard_init(void)
{
    if (g_keyboard_fd_count > 0) {
        return true;
    }

    raw_keyboard_close_all();

    const char *keyboard_device = getenv_default("LV_LINUX_KEYBOARD_DEVICE", g_keyboard_device);
    if (keyboard_device != NULL && keyboard_device[0] != '\0') {
        (void)open_raw_keyboard_device(keyboard_device);
    }

    const char *fallback_devices[] = {
        "/dev/input/by-path/platform-3f804000.i2c-event",
        "/dev/input/by-path/platform-fe204000.i2c-event",
        "/dev/input/by-path/platform-3f804000.i2c-platform-tca8418-event",
        "/dev/input/by-path/platform-fe204000.i2c-platform-tca8418-event",
        "/dev/input/by-path/platform-i2c-event",
        "/dev/input/event0",
        "/dev/input/event1",
        "/dev/input/event2",
        "/dev/input/event3",
        "/dev/input/event4",
        "/dev/input/event5",
        "/dev/input/event6",
        "/dev/input/event7",
        "/dev/input/event8",
    };

    for (size_t i = 0; i < sizeof(fallback_devices) / sizeof(fallback_devices[0]); ++i) {
        if (keyboard_device != NULL && strcmp(keyboard_device, fallback_devices[i]) == 0) {
            continue;
        }
        (void)open_raw_keyboard_device(fallback_devices[i]);
    }

    raw_keyboard_probe_directory("/dev/input/by-path", "event");
    raw_keyboard_probe_directory("/dev/input/by-id", "event");
    raw_keyboard_probe_directory("/dev/input", "event");

    if (g_keyboard_fd_count > 0) {
        printf("Raw keyboard ready: %d device(s) opened\n", g_keyboard_fd_count);
        return true;
    }

    if (!g_keyboard_ready_reported) {
        printf("Raw keyboard open failed, last tried: %s errno=%d\n",
               keyboard_device ? keyboard_device : "<null>", errno);
        g_keyboard_ready_reported = true;
    }

    return false;
}

static void raw_keyboard_poll(void)
{
    if (g_keyboard_fd_count <= 0) {
        static uint64_t last_retry_ms = 0;
        uint64_t now_ms = get_monotonic_ms();
        if (now_ms - last_retry_ms >= 1000ULL) {
            last_retry_ms = now_ms;
            (void)raw_keyboard_init();
        }
        return;
    }

    for (int i = 0; i < g_keyboard_fd_count; ++i) {
        if (g_keyboard_fds[i] < 0) {
            continue;
        }

        struct input_event ev;
        ssize_t read_ret = 0;
        while ((read_ret = read(g_keyboard_fds[i], &ev, sizeof(ev))) == (ssize_t)sizeof(ev)) {
            if (ev.type != EV_KEY) {
                continue;
            }

            if (ev.code == 42 || ev.code == 54) {
                g_keyboard_shift_pressed = (ev.value != 0);
                continue;
            }

            if (ev.value == 0) {
                continue;
            }

            uint32_t app_key = linux_key_to_app_key(ev.code, g_keyboard_shift_pressed);
            if (app_key != 0) {
                printf("Raw key mapped: dev=%s code=%u(%s) value=%d -> lv=0x%X\n",
                       g_keyboard_devices[i],
                       (unsigned int)ev.code,
                       linux_key_code_name(ev.code),
                       ev.value,
                       (unsigned int)app_key);
                bool handled = handle_app_key(app_key);

                lv_group_t *group = lv_group_get_default();
                if (!handled && group != NULL) {
                    (void)lv_group_send_data(group, app_key);
                }
            } else {
                printf("Raw key ignored: dev=%s code=%u(%s) value=%d\n",
                       g_keyboard_devices[i],
                       (unsigned int)ev.code,
                       linux_key_code_name(ev.code),
                       ev.value);
            }
        }

        if (read_ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            printf("Raw keyboard read failed: %s errno=%d, closing...\n", g_keyboard_devices[i], errno);
            close(g_keyboard_fds[i]);
            g_keyboard_fds[i] = -1;
            g_keyboard_devices[i][0] = '\0';
        }
    }

    int compact_index = 0;
    for (int i = 0; i < g_keyboard_fd_count; ++i) {
        if (g_keyboard_fds[i] >= 0) {
            if (compact_index != i) {
                g_keyboard_fds[compact_index] = g_keyboard_fds[i];
                snprintf(g_keyboard_devices[compact_index], sizeof(g_keyboard_devices[compact_index]), "%s", g_keyboard_devices[i]);
            }
            ++compact_index;
        }
    }
    for (int i = compact_index; i < g_keyboard_fd_count; ++i) {
        g_keyboard_fds[i] = -1;
        g_keyboard_devices[i][0] = '\0';
    }
    g_keyboard_fd_count = compact_index;

    if (g_keyboard_fd_count <= 0) {
        g_keyboard_ready_reported = false;
    }
}

static void update_home_carousel(void)
{
    static const char *icon_texts[3] = {"Lo", "RF", "NF"};
    static const lv_color_t colors[3] = {
        LV_COLOR_MAKE(0x3B, 0x82, 0xF6),
        LV_COLOR_MAKE(0xF5, 0x9E, 0x0B),
        LV_COLOR_MAKE(0xA8, 0x55, 0xF7),
    };
    static const lv_coord_t card_x[3] = {-92, 0, 92};
    static const lv_coord_t card_y[3] = {-2, -10, -2};
    static const lv_coord_t card_w[3] = {54, 82, 54};
    static const lv_coord_t card_h[3] = {54, 82, 54};

    for (int slot = 0; slot < 3; ++slot) {
        int app_index = (g_menu_index + slot + 2) % 3;
        bool selected = (slot == 1);

        if (g_home_icon_card[slot]) {
            lv_obj_set_size(g_home_icon_card[slot], card_w[slot], card_h[slot]);
            lv_obj_set_align(g_home_icon_card[slot], LV_ALIGN_CENTER);
            lv_obj_set_x(g_home_icon_card[slot], card_x[slot]);
            lv_obj_set_y(g_home_icon_card[slot], card_y[slot]);
            lv_obj_set_style_bg_color(g_home_icon_card[slot], colors[app_index], LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(g_home_icon_card[slot], selected ? LV_OPA_COVER : LV_OPA_70, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(g_home_icon_card[slot], selected ? lv_color_hex(0xFFD24A) : lv_color_hex(0x666666), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(g_home_icon_card[slot], selected ? 3 : 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        if (g_home_icon_label[slot]) {
            lv_label_set_text(g_home_icon_label[slot], icon_texts[app_index]);
            lv_obj_set_style_text_font(g_home_icon_label[slot], selected ? &lv_font_montserrat_24 : &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        if (g_home_name_label[slot]) {
            lv_label_set_text(g_home_name_label[slot], g_app_names[app_index]);
            lv_obj_align_to(g_home_name_label[slot], g_home_icon_card[slot], LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
            lv_obj_set_style_text_color(g_home_name_label[slot], selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x8F8F8F), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(g_home_name_label[slot], selected ? &lv_font_montserrat_16 : &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    for (int i = 0; i < 3; ++i) {
        if (g_home_page_indicator[i]) {
            bool selected = (g_menu_index == i);
            lv_obj_set_style_bg_opa(g_home_page_indicator[i], selected ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(g_home_page_indicator[i], lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(g_home_page_indicator[i], selected ? lv_color_hex(0xFFD24A) : lv_color_hex(0xD8D8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
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

static int write_text_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t ret = write(fd, value, strlen(value));
    close(fd);
    return ret < 0 ? -1 : 0;
}

static void lora_set_status(const char *text, lv_color_t color)
{
    if (ui_loraStatus) {
        lv_label_set_text(ui_loraStatus, text);
        lv_obj_set_style_text_color(ui_loraStatus, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void lora_set_statusf(lv_color_t color, const char *fmt, ...)
{
    char text[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    lora_set_status(text, color);
}

static void lora_set_diag_step(const char *step, int code, const char *detail)
{
    snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
             "%s%s%s | rc=%d",
             step ? step : "diag",
             (detail && detail[0]) ? " | " : "",
             (detail && detail[0]) ? detail : "",
             code);

    printf("LoRa diag: %s\n", g_lora_last_diag);

    lora_set_statusf(code == 0 ? lv_color_hex(0xFFB020) : lv_color_hex(0xFF4D4F),
                     "Diag: %s", g_lora_last_diag);
}

static void show_host_ip(void)
{
    if (!ui_Label2) {
        return;
    }

    char text[64];
    snprintf(text, sizeof(text), "IP: %s", get_cached_host_ip());
    lv_label_set_text(ui_Label2, text);
}

static const char *get_cached_host_ip(void)
{
    uint64_t now_ms = get_monotonic_ms();
    if (g_cached_host_ip_ms != 0 && now_ms - g_cached_host_ip_ms < 5000ULL) {
        return g_cached_host_ip[0] ? g_cached_host_ip : "unavailable";
    }

    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;
    char ip[INET_ADDRSTRLEN] = {0};

    if (getifaddrs(&ifaddr) == 0) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
                continue;
            }

            if ((ifa->ifa_flags & IFF_UP) == 0 || (ifa->ifa_flags & IFF_LOOPBACK) != 0) {
                continue;
            }

            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) != NULL) {
                break;
            }
        }
        freeifaddrs(ifaddr);
    }

    if (ip[0] != '\0') {
        snprintf(g_cached_host_ip, sizeof(g_cached_host_ip), "%s", ip);
    } else if (g_cached_host_ip[0] == '\0') {
        snprintf(g_cached_host_ip, sizeof(g_cached_host_ip), "unavailable");
    }

    g_cached_host_ip_ms = now_ms;
    return g_cached_host_ip;
}

static void render_home_menu(void)
{
    g_current_page = APP_PAGE_HOME;
    lora_set_topbar_bg(false);
    ensure_home_widgets();
    set_home_widgets_hidden(false);
    set_detail_widgets_hidden(true);

    if (ui_appname) {
        lv_label_set_text(ui_appname, "Communication");
        lv_obj_set_x(ui_appname, 0);
        lv_obj_set_y(ui_appname, -74);
    }

    show_host_ip();
    update_home_carousel();
    if (g_home_hint_label) {
        lv_label_set_text(g_home_hint_label, "Z/A/H: Left   C/D/L: Right   OK/Enter: Open");
    }
}

static void render_cc1101_page(void)
{
    g_current_page = APP_PAGE_CC1101;
    lora_set_topbar_bg(false);
    set_home_widgets_hidden(true);
    set_detail_widgets_hidden(false);
    if (ui_appname) lv_label_set_text(ui_appname, "CC1101 Test");
    show_host_ip();
    if (ui_loraPins) lv_label_set_text(ui_loraPins, "SPI device reserved for CC1101 communication test");
    if (ui_loraDevice) lv_label_set_text(ui_loraDevice, "Bus: SPI   Status: UI ready");
    if (ui_loraMode) {
        lv_label_set_text(ui_loraMode, "Mode: Placeholder");
        lv_obj_set_style_text_color(ui_loraMode, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_loraStatus) {
        lv_label_set_text(ui_loraStatus, "CC1101 test UI ready, driver logic can be added next");
        lv_obj_set_style_text_color(ui_loraStatus, lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_loraHint) lv_label_set_text(ui_loraHint, "ESC/BSP: Back to menu");
}

static void render_nfc_page(void)
{
    g_current_page = APP_PAGE_NFC;
    lora_set_topbar_bg(false);
    set_home_widgets_hidden(true);
    set_detail_widgets_hidden(false);
    if (ui_appname) lv_label_set_text(ui_appname, "NFC Test");
    show_host_ip();
    if (ui_loraPins) lv_label_set_text(ui_loraPins, "SPI device reserved for NFC communication test");
    if (ui_loraDevice) lv_label_set_text(ui_loraDevice, "Bus: SPI   Status: UI ready");
    if (ui_loraMode) {
        lv_label_set_text(ui_loraMode, "Mode: Placeholder");
        lv_obj_set_style_text_color(ui_loraMode, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_loraStatus) {
        lv_label_set_text(ui_loraStatus, "NFC test UI ready, driver logic can be added next");
        lv_obj_set_style_text_color(ui_loraStatus, lv_color_hex(0xFFD24A), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_loraHint) lv_label_set_text(ui_loraHint, "ESC/BSP: Back to menu");
}

static void return_to_home_menu(void)
{
    render_home_menu();
}

static void enter_selected_app(void)
{
    switch (g_menu_index) {
    case 0:
        lora_render_page();
        break;
    case 1:
        render_cc1101_page();
        break;
    case 2:
    default:
        render_nfc_page();
        break;
    }
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
    if (line_fd == NULL) {
        return -1;
    }

    if (gpio_init_input(gpio) < 0) {
        return -1;
    }

    char edge_path[64];
    snprintf(edge_path, sizeof(edge_path), "/sys/class/gpio/gpio%d/edge", gpio);
    if (write_text_file(edge_path, "rising") < 0) {
        return -1;
    }

    int fd = gpio_open_value_fd(gpio);
    if (fd < 0) {
        return -1;
    }

    char dummy = 0;
    lseek(fd, 0, SEEK_SET);
    (void)read(fd, &dummy, 1);
    *line_fd = fd;
    return 0;
}

static int gpio_init_output(int gpio, int value)
{
    if (gpio_export_if_needed(gpio) < 0) {
        return -1;
    }

    if (value) {
        if (gpio_set_direction(gpio, "high") == 0) {
            return 0;
        }
    } else {
        if (gpio_set_direction(gpio, "low") == 0) {
            return 0;
        }
    }

    if (gpio_set_direction(gpio, "out") < 0) {
        return -1;
    }

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
    if (chip_path == NULL || line_fd == NULL) {
        return false;
    }

    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) {
        return false;
    }

    struct gpiohandle_request req;
    memset(&req, 0, sizeof(req));
    req.lines = 1;
    req.lineoffsets[0] = (uint32_t)offset;
    req.flags = GPIOHANDLE_REQUEST_INPUT;
    snprintf(req.consumer_label, sizeof(req.consumer_label), "userdemo-lora-in");

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
    if (line_fd < 0 || value == NULL) {
        return false;
    }

    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));
    if (ioctl(line_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
        return false;
    }

    *value = data.values[0] ? 1 : 0;
    return true;
}
#endif

static bool gpio_open_input_event_line(const char *chip_path, int offset, int *line_fd)
{
    if (chip_path == NULL || line_fd == NULL) {
        return false;
    }

    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) {
        return false;
    }

    struct gpioevent_request req;
    memset(&req, 0, sizeof(req));
    req.lineoffset = (uint32_t)offset;
    req.handleflags = GPIOHANDLE_REQUEST_INPUT;
    req.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
    snprintf(req.consumer_label, sizeof(req.consumer_label), "userdemo-lora-irq");

    if (ioctl(chip_fd, GPIO_GET_LINEEVENT_IOCTL, &req) < 0) {
        close(chip_fd);
        return false;
    }

    close(chip_fd);
    *line_fd = req.fd;
    (void)fcntl(*line_fd, F_SETFL, fcntl(*line_fd, F_GETFL, 0) | O_NONBLOCK);
    return true;
}

static int gpio_init_output_any(const char *chip_env_name, const char *offset_env_name, int gpio, int value, int *line_fd, const char *line_name)
{
    if (line_fd && *line_fd >= 0) {
        return 0;
    }

#if HAS_LINUX_GPIO_CDEV
    const char *chip_env = getenv(chip_env_name);
    const char *offset_env = getenv(offset_env_name);
    char chip_path[64] = "/dev/gpiochip0";
    int offset = gpio;

    if (chip_env && chip_env[0]) {
        snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    }
    if (offset_env && offset_env[0]) {
        offset = atoi(offset_env);
    }

    if (line_fd && gpio_open_output_line(chip_path, offset, value, line_fd)) {
        printf("LoRa GPIO %s via cdev: %s[%d]=%d\n", line_name ? line_name : "out", chip_path, offset, value);
        return 0;
    }
#endif

    if (gpio_init_output(gpio, value) == 0) {
        return 0;
    }

    printf("LoRa GPIO %s init failed: gpio=%d errno=%d\n", line_name ? line_name : "out", gpio, errno);
    return -1;
}

static int gpio_init_input_any(const char *chip_env_name, const char *offset_env_name, int gpio, int *line_fd, const char *line_name)
{
    if (line_fd && *line_fd >= 0) {
        return 0;
    }

#if HAS_LINUX_GPIO_CDEV
    const char *chip_env = getenv(chip_env_name);
    const char *offset_env = getenv(offset_env_name);
    char chip_path[64] = "/dev/gpiochip0";
    int offset = gpio;

    if (chip_env && chip_env[0]) {
        snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    }
    if (offset_env && offset_env[0]) {
        offset = atoi(offset_env);
    }

    if (line_fd && gpio_open_input_line(chip_path, offset, line_fd)) {
        printf("LoRa GPIO %s via cdev: %s[%d]\n", line_name ? line_name : "in", chip_path, offset);
        return 0;
    }
#endif

    if (gpio_init_input(gpio) == 0) {
        return 0;
    }

    printf("LoRa GPIO %s input init failed: gpio=%d errno=%d\n", line_name ? line_name : "in", gpio, errno);
    return -1;
}

static int gpio_init_input_irq_any(const char *chip_env_name, const char *offset_env_name, int gpio, int *line_fd, const char *line_name)
{
    if (line_fd && *line_fd >= 0) {
        return 0;
    }

#if HAS_LINUX_GPIO_CDEV
    const char *chip_env = getenv(chip_env_name);
    const char *offset_env = getenv(offset_env_name);
    char chip_path[64] = "/dev/gpiochip0";
    int offset = gpio;

    if (chip_env && chip_env[0]) {
        snprintf(chip_path, sizeof(chip_path), "%s", chip_env);
    }
    if (offset_env && offset_env[0]) {
        offset = atoi(offset_env);
    }

    if (line_fd && gpio_open_input_event_line(chip_path, offset, line_fd)) {
        printf("LoRa GPIO %s irq-event via cdev: %s[%d]\n", line_name ? line_name : "irq", chip_path, offset);
        return 0;
    }
#endif

    if (line_fd && gpio_init_input_irq_sysfs(gpio, line_fd) == 0) {
        printf("LoRa GPIO %s irq-event via sysfs: gpio%d rising\n", line_name ? line_name : "irq", gpio);
        return 0;
    }

    return -1;
}

static int gpio_get_value_any(int gpio, int line_fd)
{
#if HAS_LINUX_GPIO_CDEV
    int value = 0;
    if (line_fd >= 0 && gpio_get_input_line_value(line_fd, &value)) {
        return value;
    }
#endif
    return gpio_get_value(gpio);
}

static int gpio_set_value_any(int gpio, int line_fd, int value)
{
#if HAS_LINUX_GPIO_CDEV
    if (line_fd >= 0) {
        return gpio_set_output_line_value(line_fd, value) ? 0 : -1;
    }
#endif
    return gpio_set_value(gpio, value);
}

static size_t collect_spi_candidates(char out[][64], size_t max_count, const char *preferred)
{
    if (out == NULL || max_count == 0) {
        return 0;
    }

    size_t count = 0;
    auto append_candidate = [&](const char *path) {
        if (path == NULL || path[0] == '\0') {
            return;
        }
        for (size_t i = 0; i < count; ++i) {
            if (strcmp(out[i], path) == 0) {
                return;
            }
        }
        if (count < max_count) {
            snprintf(out[count], 64, "%s", path);
            ++count;
        }
    };

    append_candidate(preferred);

    append_candidate("/dev/spidev0.1");
    append_candidate("/dev/spidev0.0");

    DIR *dir = opendir("/dev");
    if (dir != NULL) {
        struct dirent *entry = NULL;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "spidev", 6) != 0) {
                continue;
            }

            char full_path[64];
            snprintf(full_path, sizeof(full_path), "/dev/%s", entry->d_name);
            append_candidate(full_path);
        }
        closedir(dir);
    }

    const char *fallbacks[] = {
        "/dev/spidev0.1", "/dev/spidev0.0",
        "/dev/spidev1.0", "/dev/spidev1.1",
        "/dev/spidev2.0", "/dev/spidev2.1",
        "/dev/spidev3.0", "/dev/spidev3.1",
        "/dev/spidev4.0", "/dev/spidev4.1",
    };

    for (size_t i = 0; i < sizeof(fallbacks) / sizeof(fallbacks[0]); ++i) {
        append_candidate(fallbacks[i]);
    }

    return count;
}

static void lora_update_power_debug(const char *stage, int sysfs_ret, int gpio_value, bool cdev_ok)
{
    char text[256];
    const char *chip_text = g_hat_5vout_chip[0] ? g_hat_5vout_chip : "sysfs";
    const char *value_text = gpio_value < 0 ? "read_fail" : (gpio_value ? "HIGH" : "LOW");

    snprintf(text, sizeof(text),
             "5VDBG %s cdev=%s chip=%s[%d] sysfs_ret=%d gpio5=%s",
             stage ? stage : "?",
             cdev_ok ? "ok" : "fail",
             chip_text,
             g_hat_5vout_offset,
             sysfs_ret,
             value_text);

    printf("%s\n", text);
}

#if HAS_LINUX_GPIO_CDEV
static bool gpio_line_name_matches(const char *name)
{
    static const char *candidates[] = {
        "G5_HAT_5VOUT_EN",
        "HAT_5VOUT_EN",
        "PG5",
        "G5",
    };

    if (name == NULL || name[0] == '\0') {
        return false;
    }

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (strcmp(name, candidates[i]) == 0) {
            return true;
        }
    }

    return false;
}

static bool gpio_find_named_line(char *chip_path, size_t chip_path_size, int *offset)
{
    if (chip_path == NULL || chip_path_size == 0 || offset == NULL) {
        return false;
    }

    for (int chip_index = 0; chip_index < 8; ++chip_index) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/gpiochip%d", chip_index);

        int chip_fd = open(path, O_RDONLY);
        if (chip_fd < 0) {
            continue;
        }

        struct gpiochip_info chip_info;
        memset(&chip_info, 0, sizeof(chip_info));
        if (ioctl(chip_fd, GPIO_GET_CHIPINFO_IOCTL, &chip_info) < 0) {
            close(chip_fd);
            continue;
        }

        for (int line = 0; line < (int)chip_info.lines; ++line) {
            struct gpioline_info line_info;
            memset(&line_info, 0, sizeof(line_info));
            line_info.line_offset = line;
            if (ioctl(chip_fd, GPIO_GET_LINEINFO_IOCTL, &line_info) < 0) {
                continue;
            }

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
    if (chip_path == NULL || line_fd == NULL) {
        return false;
    }

    int chip_fd = open(chip_path, O_RDONLY);
    if (chip_fd < 0) {
        return false;
    }

    struct gpiohandle_request req;
    memset(&req, 0, sizeof(req));
    req.lines = 1;
    req.lineoffsets[0] = (uint32_t)offset;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req.default_values[0] = (uint8_t)(value ? 1 : 0);
    snprintf(req.consumer_label, sizeof(req.consumer_label), "userdemo-lora-5v");

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
    if (line_fd < 0) {
        return false;
    }

    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));
    data.values[0] = (uint8_t)(value ? 1 : 0);
    return ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) == 0;
}
#endif

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

    if (g_hat_5vout_fd >= 0) {
        g_hat_5vout_last_cdev_ok = true;
        return true;
    }

    if (gpio_open_output_line(g_hat_5vout_chip, g_hat_5vout_offset, 1, &g_hat_5vout_fd)) {
        g_hat_5vout_last_cdev_ok = true;
        return true;
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
    if (g_spi_fd >= 0) {
        return true;
    }

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
        close(g_spi_fd);
        g_spi_fd = -1;
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

    if (g_hat_5vout_last_sysfs_ret == 0) {
        usleep(50000);
        return true;
    }

    lora_update_power_debug("enable_fail", g_hat_5vout_last_sysfs_ret, g_hat_5vout_last_value, cdev_ok);

    return false;
}

static bool pi4io_open_bus(int *fd)
{
#if !USERDEMO_HAS_LINUX_I2CDEV
    if (fd) *fd = -1;
    snprintf(g_pi4io_status, sizeof(g_pi4io_status),
             "I2C dev header missing, cannot access 0x%02X", g_pi4io_i2c_addr);
    return false;
#else
    if (fd == NULL) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C fd pointer invalid");
        return false;
    }

    char dev_path[64];
    snprintf(dev_path, sizeof(dev_path), "/dev/i2c-%d", g_pi4io_i2c_bus);
    *fd = open(dev_path, O_RDWR);
    if (*fd < 0) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status),
                 "open %s failed, SDA:%d SCL:%d errno=%d",
                 dev_path, g_pi4io_sda_gpio, g_pi4io_scl_gpio, errno);
        return false;
    }

    return true;
#endif
}

static bool pi4io_select_device(int fd)
{
    if (fd < 0) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status), "I2C fd invalid for 0x%02X", g_pi4io_i2c_addr);
        return false;
    }

    if (ioctl(fd, I2C_SLAVE, g_pi4io_i2c_addr) < 0) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status),
                 "select 0x%02X failed on /dev/i2c-%d errno=%d",
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
        snprintf(g_pi4io_status, sizeof(g_pi4io_status),
                 "I2C 0x%02X not found on /dev/i2c-%d (SDA:%d SCL:%d)",
                 g_pi4io_i2c_addr, g_pi4io_i2c_bus, g_pi4io_sda_gpio, g_pi4io_scl_gpio);
        return false;
    }

    snprintf(g_pi4io_status, sizeof(g_pi4io_status),
             "I2C 0x%02X found on /dev/i2c-%d (SDA:%d SCL:%d)",
             g_pi4io_i2c_addr, g_pi4io_i2c_bus, g_pi4io_sda_gpio, g_pi4io_scl_gpio);
    return true;
}

static bool pi4io_init_device(int fd)
{
    if (fd < 0) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status),
                 "I2C IO init invalid fd for 0x%02X", g_pi4io_i2c_addr);
        return false;
    }

    g_pi4io_polarity_cache = 0x00;
    g_pi4io_output_cache = 0x01;
    g_pi4io_config_cache = 0xFE;

    errno = 0;
    if (!pi4io_write_reg(fd, 0x02, g_pi4io_polarity_cache)) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status),
                 "I2C IO write POL failed at 0x%02X errno=%d",
                 g_pi4io_i2c_addr, errno);
        return false;
    }

    errno = 0;
    if (!pi4io_write_reg(fd, 0x01, g_pi4io_output_cache)) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status),
                 "I2C IO write OUT failed at 0x%02X errno=%d",
                 g_pi4io_i2c_addr, errno);
        return false;
    }

    errno = 0;
    if (!pi4io_write_reg(fd, 0x03, g_pi4io_config_cache)) {
        snprintf(g_pi4io_status, sizeof(g_pi4io_status),
                 "I2C IO write CFG failed at 0x%02X errno=%d",
                 g_pi4io_i2c_addr, errno);
        return false;
    }

    snprintf(g_pi4io_status, sizeof(g_pi4io_status),
             "I2C IO init ok OUT=0x%02X POL=0x%02X CFG=0x%02X P0=HIGH",
             g_pi4io_output_cache, g_pi4io_polarity_cache, g_pi4io_config_cache);
    return true;
}

static bool pi4io_scan_and_init_before_lora(void)
{
    int fd = -1;
    bool ok = false;

    g_pi4io_found = false;
    g_pi4io_initialized = false;

    if (!pi4io_open_bus(&fd)) {
        return false;
    }

    do {
        if (!pi4io_select_device(fd)) {
            break;
        }

        if (!pi4io_probe_device(fd)) {
            break;
        }

        g_pi4io_found = true;

        if (!pi4io_init_device(fd)) {
            break;
        }

        g_pi4io_initialized = true;
        ok = true;
    } while (0);

    if (fd >= 0) {
        close(fd);
    }

    return ok;
}

static bool probe_lora_spi_device(void)
{
    const char *spi_env = getenv("LORA_SPI_DEV");
    char candidates[16][64] = {{0}};
    const size_t candidate_count = collect_spi_candidates(candidates, 16, spi_env);
    char summary[256] = {0};

    if (access("/dev", F_OK) != 0) {
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
                 "Linux /dev not available; LoRa SPI HAL requires Raspberry Pi Linux runtime, not current host env");
        snprintf(g_lora_probe_summary, sizeof(g_lora_probe_summary),
                 "no /dev directory visible to process");
        snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "SPI: /dev unavailable");
        return false;
    }

    if (candidate_count == 0) {
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
                 "no /dev/spidev* found; enable SPI on Raspberry Pi OS and run binary on Pi");
        snprintf(g_lora_probe_summary, sizeof(g_lora_probe_summary),
                 "probe aborted: no spidev nodes");
        snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "SPI: no spidev found");
        return false;
    }

    printf("LoRa SPI probe policy: prefer SPI0 only, CE1(/dev/spidev0.1) then CE0(/dev/spidev0.0)\n");

    summary[0] = '\0';
    for (size_t i = 0; i < candidate_count; ++i) {
        const char *dev = candidates[i];
        if (spi_env && spi_env[0] && strcmp(spi_env, dev) == 0) {
            continue;
        }

        if (summary[0]) {
            strncat(summary, ", ", sizeof(summary) - strlen(summary) - 1);
        }
        strncat(summary, dev, sizeof(summary) - strlen(summary) - 1);
    }

    if (spi_env && spi_env[0]) {
        snprintf(g_lora_probe_summary, sizeof(g_lora_probe_summary),
                 "probe order: %s%s%s",
                 spi_env,
                 summary[0] ? ", " : "",
                 summary);
        snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "Try: %s -> 0.1 -> 0.0", spi_env);
    } else {
        snprintf(g_lora_probe_summary, sizeof(g_lora_probe_summary), "probe order: %s", summary);
        snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "Try: /dev/spidev0.1 -> /dev/spidev0.0");
    }

    auto try_probe = [](const char *dev) -> bool {
        if (dev == NULL || dev[0] == '\0' || access(dev, F_OK) != 0) {
            return false;
        }

        snprintf(g_spi_device, sizeof(g_spi_device), "%s", dev);
        g_lora_nss_manual = false;
        const char *cs_name = strstr(g_spi_device, "spidev0.1") ? "SPI0-CE1" :
                              (strstr(g_spi_device, "spidev0.0") ? "SPI0-CE0" : "non-SPI0");
        printf("LoRa probe: trying %s [%s] (cs=hw-auto)\n", g_spi_device, cs_name);

        g_lora_initialized = false;
        if (g_spi_fd >= 0) {
            close(g_spi_fd);
            g_spi_fd = -1;
        }

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
            close(g_spi_fd);
            g_spi_fd = -1;
            snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "SPI config failed on %s", g_spi_device);
            return false;
        }

        bool ok = sx1262_get_status_raw(&status);
        close(g_spi_fd);
        g_spi_fd = -1;
        if (!ok) {
            snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "status read failed on %s", g_spi_device);
            return false;
        }

        printf("LoRa probe: %s [%s] (cs=hw-auto) status=0x%02X\n", g_spi_device, cs_name, status);
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "probe ok on %s[%s] cs=hw-auto status=0x%02X",
                 g_spi_device, cs_name, status);
        snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "FOUND: %s (%s)", g_spi_device, cs_name);
        return true;
    };

    if (spi_env && spi_env[0] && try_probe(spi_env)) {
        return true;
    }

    for (size_t i = 0; i < candidate_count; ++i) {
        if (try_probe(candidates[i])) {
            return true;
        }
    }

    snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
             "all SPI buses probed, no SX1262 response (%s) - verify binary runs on Pi Linux, SPI enabled, CE1 wiring, and BUSY/RST lines",
             g_lora_probe_summary);
    snprintf(g_lora_probe_display, sizeof(g_lora_probe_display), "NOT FOUND: tried 0.1 and 0.0");

    return false;
}

static void lora_show_pin_info(void)
{
    if (ui_loraPins) {
        char text[192];
        snprintf(text, sizeof(text), "SPI0 SCK:%d MOSI:%d MISO:%d NSS:%d",
                 g_lora_sck_gpio, g_lora_mosi_gpio, g_lora_miso_gpio, g_lora_nss_gpio);
        lv_label_set_text(ui_loraPins, text);
    }

    if (ui_loraDevice) {
        char text[192];
        snprintf(text, sizeof(text),
                 "DEV:%s  CS:%s  RST:%d BUSY:%d IRQ:%d",
                 g_spi_device,
                 g_lora_nss_manual ? "GPIO" : "HW",
                 g_lora_rst_gpio, g_lora_busy_gpio, g_lora_irq_gpio);
        lv_label_set_text(ui_loraDevice, text);
    }
}

static void lora_show_radio_config(void)
{
    if (ui_loraDevice) {
        char text[192];
        snprintf(text, sizeof(text), "Freq:%s  BW:%s  SF:%s  CR:%s",
                 g_lora_cfg_freq, g_lora_cfg_bw, g_lora_cfg_sf, g_lora_cfg_cr);
        lv_label_set_text(ui_loraDevice, text);
    }

    if (ui_loraMode) {
        char text[192];
        snprintf(text, sizeof(text),
                 "Sync:%s  Preamble:%s  Power:%s  TCXO:%s",
                 g_lora_cfg_sync, g_lora_cfg_preamble, g_lora_cfg_power, g_lora_cfg_tcxo);
        lv_label_set_text(ui_loraMode, text);
        lv_obj_set_style_text_color(ui_loraMode, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void lora_style_text_obj(lv_obj_t *obj, uint32_t bg, uint32_t fg, lv_opa_t bg_opa, int radius)
{
    if (obj == NULL) {
        return;
    }

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, bg_opa, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, bg_opa == LV_OPA_TRANSP ? 0 : 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x303848), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(fg), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
}

static void lora_place_obj(lv_obj_t *obj, int x, int y, int w, int h)
{
    if (obj == NULL) {
        return;
    }

    lv_obj_set_x(obj, x);
    lv_obj_set_y(obj, y);
    lv_obj_set_width(obj, w);
    lv_obj_set_height(obj, h);
}

static void lora_set_topbar_bg(bool visible)
{
    if (ui_Screen1 == NULL) {
        return;
    }

    if (g_lora_topbar_bg == NULL) {
        g_lora_topbar_bg = lv_obj_create(ui_Screen1);
        lv_obj_remove_style_all(g_lora_topbar_bg);
        lv_obj_clear_flag(g_lora_topbar_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(g_lora_topbar_bg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(g_lora_topbar_bg, 320, 26);
        lv_obj_set_pos(g_lora_topbar_bg, 0, 0);
        lv_obj_set_style_bg_color(g_lora_topbar_bg, lv_color_hex(0x006B3A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(g_lora_topbar_bg, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (g_lora_topbar_zero == NULL) {
        g_lora_topbar_zero = lv_label_create(ui_Screen1);
        lv_label_set_text(g_lora_topbar_zero, "ZERO");
        lv_obj_set_pos(g_lora_topbar_zero, 5, 3);
        lv_obj_set_style_text_color(g_lora_topbar_zero, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(g_lora_topbar_zero, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(g_lora_topbar_zero, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(g_lora_topbar_zero, LV_OBJ_FLAG_HIDDEN);
    }

    if (g_lora_topbar_time == NULL) {
        g_lora_topbar_time = lv_label_create(ui_Screen1);
        lv_label_set_text(g_lora_topbar_time, "15:30");
        lv_obj_set_pos(g_lora_topbar_time, 238, 3);
        lv_obj_set_style_text_color(g_lora_topbar_time, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(g_lora_topbar_time, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(g_lora_topbar_time, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(g_lora_topbar_time, LV_OBJ_FLAG_HIDDEN);
    }

    if (g_lora_topbar_power == NULL) {
        g_lora_topbar_power = lv_label_create(ui_Screen1);
        lv_label_set_text(g_lora_topbar_power, "12%");
        lv_obj_set_pos(g_lora_topbar_power, 286, 3);
        lv_obj_set_style_text_color(g_lora_topbar_power, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(g_lora_topbar_power, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(g_lora_topbar_power, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(g_lora_topbar_power, LV_OBJ_FLAG_HIDDEN);
    }

    if (visible) {
        lv_obj_clear_flag(g_lora_topbar_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_background(g_lora_topbar_bg);
        if (ui_logo) lv_obj_add_flag(ui_logo, LV_OBJ_FLAG_HIDDEN);
        if (ui_time) lv_obj_add_flag(ui_time, LV_OBJ_FLAG_HIDDEN);
        if (ui_power) lv_obj_add_flag(ui_power, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_lora_topbar_zero, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_lora_topbar_time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(g_lora_topbar_power, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_lora_topbar_zero);
        lv_obj_move_foreground(g_lora_topbar_time);
        lv_obj_move_foreground(g_lora_topbar_power);
        if (ui_appname) {
            lv_obj_set_style_text_color(ui_appname, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(ui_appname, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (ui_TimeLabel) lv_obj_set_style_text_color(ui_TimeLabel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_Label4) lv_obj_set_style_text_color(ui_Label4, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_add_flag(g_lora_topbar_bg, LV_OBJ_FLAG_HIDDEN);
        if (ui_logo) lv_obj_clear_flag(ui_logo, LV_OBJ_FLAG_HIDDEN);
        if (ui_time) lv_obj_clear_flag(ui_time, LV_OBJ_FLAG_HIDDEN);
        if (ui_power) lv_obj_clear_flag(ui_power, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_lora_topbar_zero, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_lora_topbar_time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_lora_topbar_power, LV_OBJ_FLAG_HIDDEN);
        if (ui_appname) {
            lv_obj_set_style_text_color(ui_appname, lv_color_hex(0x8D44FF), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(ui_appname, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (ui_TimeLabel) lv_obj_set_style_text_color(ui_TimeLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_Label4) lv_obj_set_style_text_color(ui_Label4, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void lora_prepare_card_layout(void)
{
    lora_set_topbar_bg(true);

    if (ui_apppage) {
        lv_obj_set_width(ui_apppage, 320);
        lv_obj_set_height(ui_apppage, 144);
        lv_obj_set_x(ui_apppage, 0);
        lv_obj_set_y(ui_apppage, 26);
        lv_obj_set_style_bg_color(ui_apppage, lv_color_hex(0x05070D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_apppage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (ui_Label2) {
        lv_obj_clear_flag(ui_Label2, LV_OBJ_FLAG_HIDDEN);
        lora_place_obj(ui_Label2, 0, 0, 320, 24);
        lora_style_text_obj(ui_Label2, 0xB8FF9C, 0x1D4A18, LV_OPA_COVER, 0);
        lv_obj_set_style_text_font(ui_Label2, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_Label2, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_Label2, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_loraPins) lora_style_text_obj(ui_loraPins, 0x000000, 0xB8FF9C, LV_OPA_TRANSP, 0);
    if (ui_loraDevice) lora_style_text_obj(ui_loraDevice, 0x000000, 0xB8FF9C, LV_OPA_TRANSP, 0);
    if (ui_loraMode) lora_style_text_obj(ui_loraMode, 0x000000, 0xB8FF9C, LV_OPA_TRANSP, 0);
    if (ui_loraStatus) lora_style_text_obj(ui_loraStatus, 0x000000, 0xB8FF9C, LV_OPA_TRANSP, 0);
    if (ui_loraHint) lora_style_text_obj(ui_loraHint, 0x000000, 0xB8FF9C, LV_OPA_TRANSP, 0);
}

static void lora_render_messages_view(void)
{
    lora_prepare_card_layout();
    if (ui_appname) lv_label_set_text(ui_appname, "Messages");
    if (ui_Label2) {
        lv_obj_add_flag(ui_Label2, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui_loraPins) {
        lv_obj_add_flag(ui_loraPins, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui_loraDevice) {
        lora_place_obj(ui_loraDevice, 0, 34, 320, 96);
        char text[192];
        snprintf(text, sizeof(text), "%s",
                 g_lora_last_rx[0] ? g_lora_last_rx : "No messages");
        lv_label_set_text(ui_loraDevice, text);
        lv_obj_set_style_text_align(ui_loraDevice, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_loraDevice, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_loraMode) {
        lv_obj_add_flag(ui_loraMode, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui_loraStatus) {
        lv_obj_add_flag(ui_loraStatus, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui_loraHint) {
        lora_place_obj(ui_loraHint, 4, 140, 312, 14);
        lv_label_set_text(ui_loraHint, "Type to send     C/Right: Info     ESC: Menu");
    }
}

static void lora_render_info_view(void)
{
    lora_prepare_card_layout();
    if (ui_appname) lv_label_set_text(ui_appname, "LoRa");
    if (ui_Label2) lv_label_set_text(ui_Label2, "< Messages                         Info  ");
    if (ui_loraPins) {
        lora_place_obj(ui_loraPins, 12, 27, 145, 34);
        lv_label_set_text(ui_loraPins, "Role\nClient");
    }
    if (ui_loraDevice) {
        lora_place_obj(ui_loraDevice, 163, 27, 145, 34);
        char text[192];
        snprintf(text, sizeof(text), "Channel\n%s", g_lora_cfg_freq);
        lv_label_set_text(ui_loraDevice, text);
    }
    if (ui_loraMode) {
        lora_place_obj(ui_loraMode, 12, 67, 296, 38);
        char text[192];
        snprintf(text, sizeof(text), "LongFast   BW:%s  SF:%s  CR:%s\nSync:%s   Preamble:%s", g_lora_cfg_bw, g_lora_cfg_sf,
                 g_lora_cfg_cr, g_lora_cfg_sync, g_lora_cfg_preamble);
        lv_label_set_text(ui_loraMode, text);
    }
    if (ui_loraStatus) {
        lora_place_obj(ui_loraStatus, 12, 111, 296, 22);
        char text[192];
        snprintf(text, sizeof(text), "Power:%s   TCXO:%s   ChUtil: 4%%", g_lora_cfg_power, g_lora_cfg_tcxo);
        lv_label_set_text(ui_loraStatus, text);
    }
    if (ui_loraHint) {
        lora_place_obj(ui_loraHint, 10, 134, 300, 15);
        lv_label_set_text(ui_loraHint, "Z/Left: messages  |  type any key: send  |  ESC: menu");
    }
}

static void lora_render_send_view(void)
{
    lora_prepare_card_layout();
    if (ui_appname) lv_label_set_text(ui_appname, "Send");
    if (ui_Label2) {
        lora_place_obj(ui_Label2, 0, 0, 320, 24);
        lv_label_set_text(ui_Label2, "  To: Broadcast@LongFast");
        lv_obj_set_style_text_font(ui_Label2, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_loraPins) {
        lora_place_obj(ui_loraPins, 8, 28, 304, 16);
        char text[32];
        snprintf(text, sizeof(text), "%u left", (unsigned int)(sizeof(g_lora_tx_input) - 1 - strlen(g_lora_tx_input)));
        lv_label_set_text(ui_loraPins, text);
        lv_obj_set_style_text_align(ui_loraPins, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_loraPins, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_loraPins, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_loraPins, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_loraDevice) {
        lora_place_obj(ui_loraDevice, 8, 46, 304, 58);
        lv_label_set_text(ui_loraDevice, g_lora_tx_input[0] ? g_lora_tx_input : "_");
        lv_obj_set_style_text_align(ui_loraDevice, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_loraDevice, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_loraMode) {
        lv_obj_add_flag(ui_loraMode, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui_loraStatus) {
        lora_place_obj(ui_loraStatus, 4, 112, 312, 24);
        lv_label_set_text(ui_loraStatus, "OK Send    DEL Delete    ESC Cancel");
        lv_obj_set_style_text_align(ui_loraStatus, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_loraStatus, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_loraHint) {
        lv_obj_add_flag(ui_loraHint, LV_OBJ_FLAG_HIDDEN);
    }
}

static void lora_render_sent_popup(void)
{
    lora_prepare_card_layout();

    if (ui_appname) {
        lv_label_set_text(ui_appname, "Messages");
    }

    /*
     * 更靠上、更紧凑的弹窗
     * 屏幕约 320x170，顶部状态栏约 28px。
     */
    const lv_coord_t card_x = 52;
    const lv_coord_t card_y = 20;
    const lv_coord_t card_w = 216;
    const lv_coord_t card_h = 82;

    /*
     * 深绿色边框 + 黑色背景
     */
    if (ui_Label2) {
        lora_place_obj(ui_Label2, card_x, card_y, card_w, card_h);

        lv_obj_clear_flag(ui_Label2, LV_OBJ_FLAG_HIDDEN);

        lv_obj_set_style_bg_color(ui_Label2, lv_color_hex(0x020806), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_Label2, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_radius(ui_Label2, 12, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_border_width(ui_Label2, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_Label2, lv_color_hex(0x006B3A), LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_shadow_width(ui_Label2, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_opa(ui_Label2, LV_OPA_30, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(ui_Label2, lv_color_hex(0x00351D), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_ofs_y(ui_Label2, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_pad_all(ui_Label2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_Label2, "");
    }

    /*
     * 标题：深绿色字体
     */
    if (ui_loraPins) {
        lora_place_obj(ui_loraPins, card_x + 10, card_y + 7, card_w - 20, 20);

        lv_obj_clear_flag(ui_loraPins, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(ui_loraPins, "Received Message");

        lv_obj_set_style_bg_opa(ui_loraPins, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_loraPins, lv_color_hex(0x00A85A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_loraPins, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_loraPins, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_pad_all(ui_loraPins, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(ui_loraPins, LV_LABEL_LONG_CLIP);
    }

    /*
     * 消息正文：黑底上深绿色字
     */
    if (ui_loraDevice) {
        lora_place_obj(ui_loraDevice, card_x + 12, card_y + 30, card_w - 24, 31);

        lv_obj_clear_flag(ui_loraDevice, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(ui_loraDevice, g_lora_last_rx[0] ? g_lora_last_rx : "<empty>");

        lv_obj_set_style_bg_opa(ui_loraDevice, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_loraDevice, lv_color_hex(0x00994F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_loraDevice, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_loraDevice, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_pad_all(ui_loraDevice, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(ui_loraDevice, LV_LABEL_LONG_WRAP);
    }

    /*
     * 底部信号信息：短文本，避免裁剪
     */
    if (ui_loraMode) {
        lora_place_obj(ui_loraMode, card_x + 10, card_y + 64, card_w - 20, 16);

        lv_obj_clear_flag(ui_loraMode, LV_OBJ_FLAG_HIDDEN);

        char text[96];
        snprintf(text, sizeof(text), "SNR %.1f  RSSI %.0f",
                 g_lora_last_snr,
                 g_lora_last_rssi);

        lv_label_set_text(ui_loraMode, text);

        lv_obj_set_style_bg_opa(ui_loraMode, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_loraMode, lv_color_hex(0x008C49), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_loraMode, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_loraMode, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_pad_all(ui_loraMode, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(ui_loraMode, LV_LABEL_LONG_CLIP);
    }

    if (ui_loraStatus) {
        lv_obj_add_flag(ui_loraStatus, LV_OBJ_FLAG_HIDDEN);
    }

    if (ui_loraHint) {
        lv_obj_add_flag(ui_loraHint, LV_OBJ_FLAG_HIDDEN);
    }
}

static void lora_render_current_view(void)
{
    if (g_lora_sent_popup_until_ms != 0 && get_monotonic_ms() < g_lora_sent_popup_until_ms) {
        lora_render_sent_popup();
        return;
    }
    g_lora_sent_popup_until_ms = 0;
    if (g_lora_view != LORA_VIEW_INFO && g_lora_view != LORA_VIEW_SEND) {
        g_lora_view = LORA_VIEW_MESSAGES;
    }
    if (g_lora_view == LORA_VIEW_INFO) lora_render_info_view();
    else if (g_lora_view == LORA_VIEW_SEND) lora_render_send_view();
    else lora_render_messages_view();
}

static void lora_open_send_view(uint32_t first_key)
{
    g_lora_view = LORA_VIEW_SEND;
    g_lora_sent_popup_until_ms = 0;
    g_lora_tx_input[0] = '\0';
    char ch = lora_key_to_char(first_key);
    if (ch != '\0') {
        g_lora_tx_input[0] = ch;
        g_lora_tx_input[1] = '\0';
    }
    lora_render_send_view();
}

static void lora_render_page(void)
{
    g_current_page = APP_PAGE_LORA;
    set_home_widgets_hidden(true);
    set_detail_widgets_hidden(false);

    if (ui_appname) {
        lv_label_set_text(ui_appname, "LoRa-1262");
    }

    show_host_ip();
    lora_show_pin_info();

    if (ui_loraHint) {
        lv_label_set_text(ui_loraHint, "Z=RX  C=TX  OK=confirm  DEL/ESC=menu");
    }

    if (!g_lora_hw_ready) {
        if (ui_loraPins) {
            lv_label_set_text(ui_loraPins, "LoRa SPI detect result");
        }

        if (ui_loraDevice) {
            char text[128];
            snprintf(text, sizeof(text), "%s", g_lora_probe_display);
            lv_label_set_text(ui_loraDevice, text);
        }

        lora_set_statusf(lv_color_hex(0xFF4D4F), "begin rc=-2 (chip_not_found)");

        if (ui_loraMode) {
            lv_label_set_text(ui_loraMode, g_spi_device);
            lv_obj_set_style_text_color(ui_loraMode, lv_color_hex(0x8AA8FF),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        if (ui_loraHint) {
            lv_label_set_text(ui_loraHint, "SPI detect page only");
        }

        return;
    }

    g_lora_view = LORA_VIEW_MESSAGES;
    lora_render_current_view();

    /*
     * 关键修改：
     * 进入 LoRa 页面时，如果当前不是 TX 模式并且没有正在发送，
     * 直接启动 RX，而不是只刷新 SX1262 status。
     */
    if (!g_lora_tx_mode && !g_lora_tx_in_progress) {
        lora_start_receive_mode();
    } else {
        lora_render_current_view();
    }
}

static void update_lora_mode_ui(void)
{
    if (g_current_page == APP_PAGE_LORA && g_lora_hw_ready) {
        /*
         * LoRa 页面现在是 320x170 卡片布局，ui_loraMode 可能是 RSSI 卡片、
         * 输入框或配置卡片。旧的 "Mode: RX/TX" 文本会把布局覆盖掉，
         * 所以在新 LoRa 页面中不再直接写这个 label。
         */
        return;
    }

    if (ui_loraMode) {
        char text[64];
        snprintf(text, sizeof(text), "Mode: %s%s",
                 g_lora_selected_tx_mode ? "TX" : "RX",
                 (g_lora_selected_tx_mode != g_lora_tx_mode) ? " (OK confirm)" : "");
        lv_label_set_text(ui_loraMode, text);
        lv_obj_set_style_text_color(ui_loraMode,
                                    g_lora_selected_tx_mode ? lv_color_hex(0xFF6B6B) : lv_color_hex(0x00D26A),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static uint64_t get_monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void lora_refresh_status(const char *prefix)
{
    char text[160];

    uint8_t status = 0;
    if (g_lora_initialized && sx1262_get_status_raw(&status)) {
        snprintf(text, sizeof(text), "%s Radio ready, SX1262 status=0x%02X", prefix, status);
        lora_set_status(text, lv_color_hex(0x00D26A));
    } else {
        snprintf(text, sizeof(text), "%s Radio read failed, check SX1262 link", prefix);
        lora_set_status(text, lv_color_hex(0xFF4D4F));
    }
}

static void lora_update_message_view(const char *prefix, const char *payload)
{
    if (!ui_loraStatus) {
        return;
    }

    char text[256];
    snprintf(text, sizeof(text), "%s%s", prefix ? prefix : "", payload ? payload : "");
    lv_label_set_text(ui_loraStatus, text);
}

static bool lora_send_text_packet(const char *payload)
{
    if (!g_lora_initialized || g_lora_radio == NULL) {
        lora_set_status("Status: LoRa not initialized", lv_color_hex(0xFF4D4F));
        return false;
    }

    if (payload == NULL || payload[0] == '\0') {
        lora_set_status("Status: message is empty", lv_color_hex(0xFFD24A));
        return false;
    }

    if (g_lora_tx_in_progress) {
        lora_set_status("Status: TX already in progress", lv_color_hex(0xFFD24A));
        return false;
    }

    snprintf(g_lora_last_tx, sizeof(g_lora_last_tx), "%s", payload);
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
        lora_set_statusf(lv_color_hex(0xFF4D4F), "Status: send failed rc=%d(%s)",
                         (int)state, lora_radiolib_status_text(state));
        return false;
    }

    g_lora_tx_in_progress = true;
    g_lora_tx_start_ms = g_lora_last_auto_tx_ms = get_monotonic_ms();
    lora_set_statusf(lv_color_hex(0xFFB020), "Sending: %s", g_lora_last_tx);
    return true;
}

static const char *lora_radiolib_status_text(int16_t state)
{
    switch (state) {
    case RADIOLIB_ERR_NONE:
        return "ok";
    case RADIOLIB_ERR_CHIP_NOT_FOUND:
        return "chip_not_found";
    case RADIOLIB_ERR_TX_TIMEOUT:
        return "tx_timeout";
    case RADIOLIB_ERR_RX_TIMEOUT:
        return "rx_timeout";
    case RADIOLIB_ERR_CRC_MISMATCH:
        return "crc_mismatch";
    case RADIOLIB_ERR_SPI_WRITE_FAILED:
        return "spi_write_failed";
    case RADIOLIB_ERR_SPI_CMD_TIMEOUT:
        return "spi_cmd_timeout";
    case RADIOLIB_ERR_SPI_CMD_INVALID:
        return "spi_cmd_invalid";
    case RADIOLIB_ERR_SPI_CMD_FAILED:
        return "spi_cmd_failed";
    default:
        return "radiolib_err";
    }
}

static void lora_append_device_error_text(char *buf, size_t buf_size, uint16_t op_error)
{
    if (buf == NULL || buf_size == 0) {
        return;
    }

    if (op_error == 0) {
        snprintf(buf, buf_size, "none");
        return;
    }

    buf[0] = '\0';
    auto append_flag = [&](const char *name) {
        if (buf[0] != '\0') {
            strncat(buf, ",", buf_size - strlen(buf) - 1);
        }
        strncat(buf, name, buf_size - strlen(buf) - 1);
    };

    if (op_error & RADIOLIB_SX126X_PA_RAMP_ERR) append_flag("PA_RAMP");
    if (op_error & RADIOLIB_SX126X_PLL_LOCK_ERR) append_flag("PLL_LOCK");
    if (op_error & RADIOLIB_SX126X_XOSC_START_ERR) append_flag("XOSC_START");
    if (op_error & RADIOLIB_SX126X_IMG_CALIB_ERR) append_flag("IMG_CALIB");
    if (op_error & RADIOLIB_SX126X_ADC_CALIB_ERR) append_flag("ADC_CALIB");
    if (op_error & RADIOLIB_SX126X_PLL_CALIB_ERR) append_flag("PLL_CALIB");
    if (op_error & RADIOLIB_SX126X_RC13M_CALIB_ERR) append_flag("RC13M_CALIB");
    if (op_error & RADIOLIB_SX126X_RC64K_CALIB_ERR) append_flag("RC64K_CALIB");
}

static void lora_capture_device_errors(const char *stage, uint16_t irq_status)
{
    if (!g_lora_initialized || g_lora_radio == NULL) {
        return;
    }

    lora_set_statusf(lv_color_hex(0xFF4D4F),
                     "Status: %s irq=0x%04X",
                     stage ? stage : "radio_err", irq_status);
    snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
             "%s irq=0x%04X",
             stage ? stage : "radio_err", irq_status);
}

static void lora_show_boot_diag(const char *title, const char *detail)
{
    if (ui_appname) {
        lv_label_set_text(ui_appname, "LoRa-1262");
    }

    if (ui_loraPins) {
        lv_label_set_text(ui_loraPins, title ? title : "LoRa diag");
    }

    if (ui_loraDevice) {
        char text[192];
        snprintf(text, sizeof(text), "SPI:%s  RST:%d BUSY:%d IRQ:%d",
                 g_spi_device, g_lora_rst_gpio, g_lora_busy_gpio, g_lora_irq_gpio);
        lv_label_set_text(ui_loraDevice, text);
    }

    if (ui_loraMode) {
        char mode_text[320];
        snprintf(mode_text, sizeof(mode_text), "%s | %s",
                 g_pi4io_status,
                 g_lora_probe_summary);
        lv_label_set_text(ui_loraMode, mode_text);
        lv_obj_set_style_text_color(ui_loraMode, lv_color_hex(0x8AA8FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (ui_loraStatus) {
        lv_label_set_text(ui_loraStatus, detail ? detail : g_lora_last_diag);
        lv_obj_set_style_text_color(ui_loraStatus, lv_color_hex(0xFF4D4F), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (ui_loraHint) {
        lv_label_set_text(ui_loraHint, "Boot diag for CE0/CE1 check");
    }
}

static void lora_apply_mode(bool tx_mode)
{
    g_lora_selected_tx_mode = tx_mode;
    update_lora_mode_ui();

    if (!g_lora_initialized || g_lora_radio == NULL) {
        lora_set_statusf(lv_color_hex(0xFF4D4F),
                         "Status: LoRa init failed | %s",
                         g_lora_last_diag);
        return;
    }

    if (tx_mode) {
        g_lora_pending_rx_after_tx = false;
        g_lora_tx_mode = true;
        g_lora_last_auto_tx_ms = get_monotonic_ms();

        if (g_lora_tx_in_progress) {
            lora_update_message_view("TX Data: ", g_lora_last_tx);
            lora_set_status("Status: TX already in progress",
                            lv_color_hex(0xFFB020));
            return;
        }

        int16_t state = g_lora_radio->standby();

        if (state == RADIOLIB_ERR_NONE) {
            lora_update_message_view("TX Data: ", g_lora_last_tx);
            lora_set_status("Status: TX mode ready, auto send every 2s",
                            lv_color_hex(0xFFB020));
        } else {
            lora_set_statusf(lv_color_hex(0xFF4D4F),
                             "Status: set TX mode failed rc=%d(%s)",
                             (int)state,
                             lora_radiolib_status_text(state));
        }

    } else {
        if (g_lora_tx_in_progress) {
            g_lora_pending_rx_after_tx = true;
            lora_set_status("Status: TX in progress, will switch to RX after TX done",
                            lv_color_hex(0xFFD24A));
            return;
        }

        g_lora_pending_rx_after_tx = false;
        g_lora_tx_mode = false;
        g_lora_last_auto_tx_ms = get_monotonic_ms();

        char text[256];
        snprintf(text, sizeof(text),
                 "RX Data: %s | RSSI: %.1fdBm | SNR: %.1fdB",
                 g_lora_last_rx[0] ? g_lora_last_rx : "<waiting>",
                 g_lora_last_rssi,
                 g_lora_last_snr);

        lora_update_message_view("", text);
        lora_start_receive_mode();
    }
}

static void lora_start_receive_mode(void)
{
    if (!g_lora_initialized || g_lora_radio == NULL) {
        printf("LoRa RX: startReceive skipped, not initialized\n");
        return;
    }

    if (g_lora_tx_in_progress) {
        printf("LoRa RX: startReceive skipped, TX in progress\n");
        g_lora_pending_rx_after_tx = true;
        return;
    }

    g_lora_tx_mode = false;
    g_lora_selected_tx_mode = false;
    g_lora_pending_rx_after_tx = false;

    /*
     * 不要在这里清 g_lora_rx_done。
     * RX_DONE 处理中会先置 g_lora_rx_done=true，然后马上调用
     * lora_start_receive_mode() 重新进入连续接收；如果这里清零，
     * lora_poll_irq_and_update_ui() 后面的 UI 刷新分支就永远看不到
     * RX_DONE，表现为实际收到包但界面一直显示 <waiting>/未收到。
     */

    printf("LoRa RX: startReceive()\n");

    int16_t state = g_lora_radio->startReceive();

    printf("LoRa RX: startReceive rc=%d(%s)\n",
           (int)state,
           lora_radiolib_status_text(state));

    if (state == RADIOLIB_ERR_NONE) {
        /*
         * 如果这是 RX_DONE 之后为了继续接收而重启 RX，不要把刚刚显示的
         * "RX ok / RX Data" 覆盖成 "waiting packet"。
         */
        if (!g_lora_rx_done) {
            char text[256];
            snprintf(text, sizeof(text), "RX Data: %s | RSSI: %.1fdBm | SNR: %.1fdB",
                     g_lora_last_rx[0] ? g_lora_last_rx : "<waiting>",
                     g_lora_last_rssi,
                     g_lora_last_snr);

            lora_update_message_view("", text);
            lora_set_status("Status: RX mode running, waiting packet",
                            lv_color_hex(0x00D26A));
        }
    } else {
        lora_set_statusf(lv_color_hex(0xFF4D4F),
                         "Status: enter RX mode failed rc=%d(%s)",
                         (int)state,
                         lora_radiolib_status_text(state));

        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
                 "startReceive rc=%d(%s)",
                 (int)state,
                 lora_radiolib_status_text(state));
    }

    update_lora_mode_ui();
}

static void lora_send_demo_packet(void)
{
    if (!g_lora_initialized || g_lora_radio == NULL) {
        lora_set_status("Status: LoRa not initialized", lv_color_hex(0xFF4D4F));
        return;
    }

    if (!g_lora_tx_mode) {
        lora_set_status("Status: switch to TX mode first", lv_color_hex(0xFFD24A));
        return;
    }

    snprintf(g_lora_last_tx, sizeof(g_lora_last_tx), "Hello from M5 LoRa-1262 #%lu",
             (unsigned long)g_lora_tx_counter);

    g_lora_pending_rx_after_tx = false;
    g_lora_tx_done = false;
    g_lora_rx_done = false;
    int16_t state = g_lora_radio->startTransmit((uint8_t *)g_lora_last_tx, strlen(g_lora_last_tx));
    if (state != RADIOLIB_ERR_NONE) {
        g_lora_tx_in_progress = false;
        lora_set_statusf(lv_color_hex(0xFF4D4F), "Status: async TX start failed rc=%d(%s)",
                         (int)state, lora_radiolib_status_text(state));
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag), "startTransmit rc=%d(%s)",
                 (int)state, lora_radiolib_status_text(state));
        return;
    }

    g_lora_tx_in_progress = true;
    g_lora_tx_start_ms = g_lora_last_auto_tx_ms = get_monotonic_ms();
    lora_set_statusf(lv_color_hex(0xFFB020), "Status: sending -> %s", g_lora_last_tx);
    ++g_lora_tx_counter;
}

static void lora_check_tx_fallback(void)
{
    if (!g_lora_initialized || !g_lora_tx_in_progress || g_lora_radio == NULL) {
        return;
    }

    uint64_t now_ms = get_monotonic_ms();
    if (g_lora_tx_start_ms != 0 && now_ms - g_lora_tx_start_ms >= 4000ULL) {
        g_lora_tx_in_progress = false;
        g_lora_tx_start_ms = 0;
        g_lora_last_auto_tx_ms = now_ms;
        lora_capture_device_errors("TX timeout", 0);
        (void)g_lora_radio->standby();
        if (g_lora_pending_rx_after_tx || !g_lora_tx_mode) {
            lora_start_receive_mode();
        }
    }
}

static bool handle_app_key(uint32_t key)
{
    bool key_was_z = (key == 'z' || key == 'Z');
    bool key_was_c = (key == 'c' || key == 'C');

    if (key_was_z) {
        key = LV_KEY_LEFT;
    } else if (key_was_c) {
        key = LV_KEY_RIGHT;
    }

    if (g_current_page == APP_PAGE_HOME) {
        if (is_menu_prev_key(key)) {
            g_menu_index = (g_menu_index + 2) % 3;
            render_home_menu();
            return true;
        } else if (is_menu_next_key(key)) {
            g_menu_index = (g_menu_index + 1) % 3;
            render_home_menu();
            return true;
        } else if (key == LV_KEY_ENTER) {
            enter_selected_app();
            return true;
        }
        return false;
    }

    if (g_current_page == APP_PAGE_LORA) {
        if (g_lora_view == LORA_VIEW_SEND) {
            if (key == LV_KEY_ESC) {
                g_lora_view = LORA_VIEW_MESSAGES;
                g_lora_tx_input[0] = '\0';
                lora_render_current_view();
                return true;
            }
            if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
                size_t len = strlen(g_lora_tx_input);
                if (len > 0) g_lora_tx_input[len - 1] = '\0';
                lora_render_send_view();
                return true;
            }
            if (key == LV_KEY_ENTER) {
                if (lora_send_text_packet(g_lora_tx_input)) {
                    g_lora_view = LORA_VIEW_MESSAGES;
                    g_lora_sent_popup_until_ms = 0;
                    lora_render_current_view();
                    g_lora_tx_input[0] = '\0';
                }
                return true;
            }
            if (is_lora_text_key(key)) {
                size_t len = strlen(g_lora_tx_input);
                if (len + 1 < sizeof(g_lora_tx_input)) {
                    g_lora_tx_input[len] = lora_key_to_char(key);
                    g_lora_tx_input[len + 1] = '\0';
                }
                lora_render_send_view();
                return true;
            }
            return true;
        }

        if (key == LV_KEY_ESC || key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
            return_to_home_menu();
            return true;
        }

        if (is_menu_prev_key(key) || key == LV_KEY_UP) {
            g_lora_view = LORA_VIEW_MESSAGES;
            g_lora_sent_popup_until_ms = 0;
            lora_render_current_view();
            return true;
        } else if (is_menu_next_key(key) || key == LV_KEY_DOWN) {
            g_lora_view = LORA_VIEW_INFO;
            g_lora_sent_popup_until_ms = 0;
            lora_render_current_view();
            return true;
        } else if (key == LV_KEY_ENTER) {
            lora_render_current_view();
            return true;
        } else if (is_lora_text_key(key) && !key_was_z && !key_was_c) {
            lora_open_send_view(key);
            return true;
        }
    }

    if (key == LV_KEY_ESC || key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
        return_to_home_menu();
        return true;
    }

    return false;
}

static void lora_screen_key_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL) return;
    uint32_t key = lv_indev_get_key(indev);
    printf("LVGL key=0x%X page=%d\n", key, (int)g_current_page);
    (void)handle_app_key(key);
}

static void bind_keypad_group(void)
{
    lv_group_t *group = lv_group_create();
    lv_group_add_obj(group, ui_Screen1);
    lv_group_set_default(group);
    lv_group_focus_obj(ui_Screen1);

    lv_indev_t *indev = NULL;
    while ((indev = lv_indev_get_next(indev)) != NULL) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
            lv_indev_set_group(indev, group);
        }
    }

    lv_obj_add_event_cb(ui_Screen1, lora_screen_key_event_cb, LV_EVENT_KEY, NULL);
}

static void lora_init_hardware(void)
{
    delete g_lora_radio;
    g_lora_radio = NULL;

    delete g_lora_radio_module;
    g_lora_radio_module = NULL;

    lora_set_diag_step("i2c_scan", 0, "scan 0x43 before LoRa init");
    lora_show_boot_diag("I2C diag: scan 0x43", g_lora_last_diag);

    if (pi4io_scan_and_init_before_lora()) {
        lora_set_diag_step("i2c_scan", 0, g_pi4io_status);
    } else {
        lora_set_diag_step("i2c_scan", 1, g_pi4io_status);
    }
    lora_show_boot_diag("I2C diag: 0x43 result", g_pi4io_status);

    lora_set_diag_step("power_enable", 0, "start");
    lora_show_boot_diag("LoRa diag: power", g_lora_last_diag);

    if (!hat_5vout_enable()) {
        printf("Status: GPIO5 low set failed\n");
        lora_set_diag_step("power_enable", 1, "GPIO5 low set failed");
        lora_show_boot_diag("LoRa diag: power_enable", g_lora_last_diag);
    }

    usleep(100000);

    lora_set_diag_step("reset_gpio_init", 0, "prepare rst pin");
    lora_show_boot_diag("LoRa diag: reset_gpio_init", g_lora_last_diag);

    if (gpio_init_output_any("LORA_RST_CHIP", "LORA_RST_OFFSET",
                             g_lora_rst_gpio, 1, &g_lora_rst_fd, "RST") < 0) {
        g_lora_initialized = false;
        g_lora_hw_ready = false;
        lora_set_diag_step("reset_gpio_init", 1,
                           "rst gpio init failed, try LORA_RST_CHIP/OFFSET");
        lora_show_boot_diag("LoRa diag: reset_gpio_init", g_lora_last_diag);
        return;
    }

    if (gpio_init_input_any("LORA_BUSY_CHIP", "LORA_BUSY_OFFSET",
                            g_lora_busy_gpio, &g_lora_busy_fd, "BUSY") < 0) {
        g_lora_initialized = false;
        g_lora_hw_ready = false;
        lora_set_diag_step("busy_gpio_init", 1,
                           "busy gpio init failed, try LORA_BUSY_CHIP/OFFSET");
        lora_show_boot_diag("LoRa diag: busy_gpio_init", g_lora_last_diag);
        return;
    }

    lora_set_diag_step("hard_reset", 0, "toggle rst before probe");
    lora_show_boot_diag("LoRa diag: hard_reset", g_lora_last_diag);

    if (!sx1262_reset()) {
        g_lora_initialized = false;
        g_lora_hw_ready = false;
        lora_set_diag_step("hard_reset", 1, "rst/busy handshake failed");
        lora_show_boot_diag("LoRa diag: hard_reset", g_lora_last_diag);
        return;
    }

    lora_set_diag_step("resolve_spi", 0, "detect device");
    resolve_lora_spi_device();
    lora_show_boot_diag("LoRa diag: resolve_spi", g_lora_last_diag);

    if (!probe_lora_spi_device()) {
        g_lora_initialized = false;
        g_lora_hw_ready = false;
        lora_set_diag_step("probe_spi", 1, g_lora_last_diag);
        lora_show_boot_diag("LoRa diag: probe_spi", g_lora_last_diag);
        return;
    }

    lora_show_boot_diag("LoRa diag: probe_spi ok", g_lora_last_diag);

    lora_set_diag_step("pre_begin_prepare", 0,
                       "reset again before RadioLib begin");
    lora_show_boot_diag("LoRa diag: pre_begin_prepare", g_lora_last_diag);

    if (!sx1262_reset()) {
        g_lora_initialized = false;
        g_lora_hw_ready = false;
        lora_set_diag_step("pre_begin_prepare", 1,
                           "rst/busy handshake failed before RadioLib begin");
        lora_show_boot_diag("LoRa diag: pre_begin_prepare", g_lora_last_diag);
        return;
    }

    lora_set_diag_step("prepare_irq", 0, "init irq pin");
    lora_show_boot_diag("LoRa diag: prepare_irq", g_lora_last_diag);

    if (gpio_init_input_irq_any("LORA_IRQ_CHIP", "LORA_IRQ_OFFSET",
                                g_lora_irq_gpio, &g_lora_irq_fd, "IRQ") < 0) {
        g_lora_irq_poll_fallback = true;
        lora_set_diag_step("prepare_irq", 1,
                           "irq gpio init failed, fallback=poll");
        lora_show_boot_diag("LoRa diag: prepare_irq", g_lora_last_diag);
    } else {
        g_lora_irq_poll_fallback = false;
        lora_set_diag_step("prepare_irq", 0, "irq gpio ok");
        lora_show_boot_diag("LoRa diag: prepare_irq", g_lora_last_diag);
    }

    lora_set_diag_step("runtime_spi", 0, "open SPI for RadioLib runtime");
    lora_show_boot_diag("LoRa diag: runtime_spi", g_lora_last_diag);

    if (!lora_open_runtime_spi()) {
        g_lora_initialized = false;
        g_lora_hw_ready = false;
        lora_set_diag_step("runtime_spi", 1, g_lora_last_diag);
        lora_show_boot_diag("LoRa diag: runtime_spi", g_lora_last_diag);
        return;
    }

    lora_set_diag_step("radiolib_setup", 0, "create module");
    lora_show_boot_diag("LoRa diag: radiolib_setup", g_lora_last_diag);

    /*
     * 关键修改：
     * probe 阶段使用的是 spidev 硬件 CS，即 cs=hw-auto。
     * 所以这里 Module 的 NSS/CS 不要再传 GPIO7，改成 RADIOLIB_NC。
     */
    g_lora_nss_manual = false;

    g_lora_radio_module = new Module(&g_lora_radio_hal,
                                     RADIOLIB_NC,
                                     (uint32_t)g_lora_irq_gpio,
                                     (uint32_t)g_lora_rst_gpio,
                                     (uint32_t)g_lora_busy_gpio);

    g_lora_radio = new SX1262(g_lora_radio_module);

    if (g_lora_radio_module == NULL || g_lora_radio == NULL) {
        g_lora_initialized = false;
        g_lora_hw_ready = false;
        lora_set_diag_step("radiolib_setup", 1, "allocation failed");
        lora_show_boot_diag("LoRa diag: radiolib_setup", g_lora_last_diag);
        return;
    }

    lora_set_diag_step("radiolib_begin", 0,
                       "configure sx1262 via RadioLib");
    lora_show_boot_diag("LoRa diag: radiolib_begin", g_lora_last_diag);

    int16_t state = g_lora_radio->begin(
        868.0f,  // frequency MHz
        125.0f,       // bandwidth kHz
        12,            // spreading factor
        5,       // coding rate 4/5
        0x34,    // sync word
        22,      // output power dBm
        20,      // preamble length
        3.0f,    // TCXO voltage
        false
    );

    if (state != RADIOLIB_ERR_NONE) {
        g_lora_initialized = false;
        g_lora_hw_ready = false;

        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
                 "RadioLib begin rc=%d(%s)",
                 (int)state,
                 lora_radiolib_status_text(state));

        printf("LoRa init failed: rc=%d (%s)\n",
               (int)state,
               lora_radiolib_status_text(state));

        lora_set_diag_step("radiolib_begin", state, g_lora_last_diag);
        lora_show_boot_diag("LoRa diag: radiolib_begin", g_lora_last_diag);
        return;
    }

    /*
     * SX1262 电流限制和 DIO2 RF switch。
     */
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
    lora_show_boot_diag("LoRa diag: ready", g_lora_last_diag);

    /*
     * 关键修改：
     * 初始化成功后立刻进入 RX。
     */
    printf("LoRa: init done, auto enter RX\n");
    lora_start_receive_mode();
}

static void lora_service_irq_once(void)
{
    if (!g_lora_initialized || g_lora_radio == NULL) {
        return;
    }

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
            while (read(g_lora_irq_fd, &event_data, sizeof(event_data)) == (ssize_t)sizeof(event_data)) {
            }
#else
            char value_buf[8];
            lseek(g_lora_irq_fd, 0, SEEK_SET);
            while (read(g_lora_irq_fd, value_buf, sizeof(value_buf)) > 0) {
                lseek(g_lora_irq_fd, 0, SEEK_SET);
                break;
            }
#endif
        }
    }

    uint32_t irq_flags = g_lora_radio->getIrqFlags();

    if (irq_flags != RADIOLIB_SX126X_IRQ_NONE || irq_event) {
        printf("LoRa IRQ: event=%d flags=0x%08lX tx_in_progress=%d tx_mode=%d\n",
               irq_event ? 1 : 0,
               (unsigned long)irq_flags,
               g_lora_tx_in_progress ? 1 : 0,
               g_lora_tx_mode ? 1 : 0);
    }

    if (!irq_event && irq_flags == RADIOLIB_SX126X_IRQ_NONE) {
        return;
    }

    /*
     * TX 完成处理。
     */
    if (g_lora_tx_in_progress) {
        if (irq_flags & RADIOLIB_SX126X_IRQ_TX_DONE) {
            int16_t state = g_lora_radio->finishTransmit();

            if (state == RADIOLIB_ERR_NONE) {
                g_lora_tx_done = true;
            } else {
                g_lora_tx_in_progress = false;

                lora_set_statusf(lv_color_hex(0xFF4D4F),
                                 "Status: finish TX failed rc=%d(%s)",
                                 (int)state,
                                 lora_radiolib_status_text(state));

                snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
                         "finishTransmit rc=%d(%s)",
                         (int)state,
                         lora_radiolib_status_text(state));
            }
        } else if (irq_flags & RADIOLIB_SX126X_IRQ_TIMEOUT) {
            g_lora_tx_in_progress = false;
            g_lora_tx_start_ms = 0;

            lora_capture_device_errors("TX irq timeout", 0);

            if (g_lora_pending_rx_after_tx || !g_lora_tx_mode) {
                lora_start_receive_mode();
            }
        }

        return;
    }

    /*
     * RX 完成处理。
     */
    if (irq_flags & RADIOLIB_SX126X_IRQ_RX_DONE) {
        uint8_t rx_buf[sizeof(g_lora_last_rx)] = {0};

        int16_t state = g_lora_radio->readData(rx_buf,
                                               sizeof(g_lora_last_rx) - 1);

        printf("LoRa RX: readData rc=%d(%s)\n",
               (int)state,
               lora_radiolib_status_text(state));

        if (state == RADIOLIB_ERR_NONE) {
            memcpy(g_lora_last_rx, rx_buf, sizeof(g_lora_last_rx));
            g_lora_last_rx[sizeof(g_lora_last_rx) - 1] = '\0';

            g_lora_last_rssi = g_lora_radio->getRSSI();
            g_lora_last_snr = g_lora_radio->getSNR();
            g_lora_rx_done = true;

            printf("LoRa RX OK: '%s' RSSI=%.1f SNR=%.1f\n",
                   g_lora_last_rx,
                   g_lora_last_rssi,
                   g_lora_last_snr);
        } else if (state != RADIOLIB_ERR_CRC_MISMATCH) {
            snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
                     "readData rc=%d(%s)",
                     (int)state,
                     lora_radiolib_status_text(state));
        }

        if (!g_lora_tx_mode) {
            lora_start_receive_mode();
        }

    } else if (irq_flags & (RADIOLIB_SX126X_IRQ_CRC_ERR |
                            RADIOLIB_SX126X_IRQ_HEADER_ERR)) {
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
                 "RX crc/header error irq=0x%04lX",
                 (unsigned long)irq_flags);

        printf("LoRa RX error: %s\n", g_lora_last_diag);

        if (!g_lora_tx_mode) {
            lora_start_receive_mode();
        }

    } else if (irq_flags & RADIOLIB_SX126X_IRQ_TIMEOUT) {
        snprintf(g_lora_last_diag, sizeof(g_lora_last_diag),
                 "RX timeout irq=0x%04lX",
                 (unsigned long)irq_flags);

        printf("LoRa RX timeout: %s\n", g_lora_last_diag);

        /*
         * lora_service_irq_once() 在 readData() 后已经重新 startReceive()。
         * 这里如果再次调用，会把 UI 状态重新覆盖成 waiting packet。
         */
    }
}

static void lora_poll_irq_and_update_ui(void)
{
    if (!g_lora_initialized) {
        return;
    }

    /*
     * 关键修改：
     * 无论当前是否在 LoRa 页面，都要先处理 LoRa IRQ。
     * 否则停在 Home 页面时，LoRa 即使收到包也不会 readData。
     */
    lora_service_irq_once();
    lora_check_tx_fallback();

    /*
     * TX_DONE 后的状态处理。
     */
    if (g_lora_tx_done) {
        g_lora_tx_done = false;
        g_lora_tx_in_progress = false;
        g_lora_tx_start_ms = 0;

        if (g_lora_pending_rx_after_tx || !g_lora_tx_mode) {
            if (g_current_page == APP_PAGE_LORA) {
                char text[256];
                snprintf(text, sizeof(text),
                         "RX Data: %s | RSSI: %.1fdBm | SNR: %.1fdB",
                         g_lora_last_rx[0] ? g_lora_last_rx : "<waiting>",
                         g_lora_last_rssi,
                         g_lora_last_snr);

                lora_update_message_view("", text);
                lora_set_status("Status: TX done, switching to RX",
                                lv_color_hex(0x00D26A));
            }

            lora_start_receive_mode();

        } else {
            if (g_current_page == APP_PAGE_LORA) {
                lora_update_message_view("TX Data: ", g_lora_last_tx);
                lora_set_status("Status: TX done, auto send every 2s",
                                lv_color_hex(0xFFB020));
            }
        }
    }

    /*
     * RX_DONE 后的 UI 更新。
     * 注意：接收本身已经在后台处理，这里只是显示。
     */
    if (g_lora_rx_done) {
        g_lora_rx_done = false;

        if (g_current_page == APP_PAGE_LORA) {
            g_lora_view = LORA_VIEW_MESSAGES;
            g_lora_sent_popup_until_ms = get_monotonic_ms() + 2000ULL;
        }

        if (g_current_page == APP_PAGE_LORA) {
            lora_render_current_view();
        }

        /*
         * 不要在这里再次 startReceive()。
         * RX IRQ 分支读完数据后已经调用 lora_start_receive_mode() 继续接收。
         * 如果这里再调用一次，此时 g_lora_rx_done 已经被清成 false，
         * lora_start_receive_mode() 会把屏幕重新覆盖成：
         * "RX mode running, waiting packet"，导致用户看不到刚收到的数据。
         */
    }

    if (g_current_page == APP_PAGE_LORA &&
        g_lora_sent_popup_until_ms != 0 &&
        get_monotonic_ms() >= g_lora_sent_popup_until_ms) {
        g_lora_sent_popup_until_ms = 0;
        g_lora_view = LORA_VIEW_MESSAGES;
        lora_render_current_view();
    }

    /*
     * TX 自动发送逻辑。
     */
    if (g_lora_initialized && g_lora_tx_mode && !g_lora_tx_in_progress) {
        uint64_t now_ms = get_monotonic_ms();

        if (now_ms - g_lora_last_auto_tx_ms >= 2000ULL) {
            lora_send_demo_packet();
        }
    }
}

static void clear_screen_on_boot(void)
{
    lv_obj_t *boot_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(boot_scr);
    lv_obj_set_style_bg_color(boot_scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(boot_scr, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_scr_load(boot_scr);
    lv_timer_handler();
    usleep(30000);
}

static const char *getenv_default(const char *name, const char *dflt)
{
    const char *value = getenv(name);
    return value ? value : dflt;
}

static void resolve_lora_spi_device(void)
{
    const char *spi_env = getenv("LORA_SPI_DEV");
    char candidates[16][64] = {{0}};
    const size_t candidate_count = collect_spi_candidates(candidates, 16, spi_env);

    if (spi_env != NULL && spi_env[0] != '\0' && access(spi_env, F_OK) == 0) {
        snprintf(g_spi_device, sizeof(g_spi_device), "%s", spi_env);
        return;
    }

    for (size_t i = 0; i < candidate_count; ++i) {
        if (access(candidates[i], F_OK) == 0) {
            snprintf(g_spi_device, sizeof(g_spi_device), "%s", candidates[i]);
            return;
        }
    }

    snprintf(g_spi_device, sizeof(g_spi_device), "%s", spi_env && spi_env[0] ? spi_env : "/dev/spidev0.1");
}

int get_st7789v_fbdev(char *dev_path, size_t buf_size)
{
    if (dev_path == NULL || buf_size == 0)
    {
        return -1;
    }

    FILE *fp = fopen("/proc/fb", "r");
    if (fp == NULL)
    {
        perror("Failed to open /proc/fb");
        return -1;
    }

    char line[256];
    int fb_num = -1;

    /* 逐行读取，查找包含 fb_st7789v 的行，格式如：0 fb_st7789v */
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (strstr(line, "fb_st7789v") != NULL)
        {
            if (sscanf(line, "%d", &fb_num) == 1)
            {
                break;
            }
        }
    }

    fclose(fp);

    if (fb_num < 0)
    {
        fprintf(stderr, "fb_st7789v not found in /proc/fb\n");
        return -1;
    }

    snprintf(dev_path, buf_size, "/dev/fb%d", fb_num);
    return 0;
}


#if LV_USE_EVDEV

static void lv_linux_indev_init(void)
{
    const char *mouse_device = getenv_default("LV_LINUX_MOUSE_DEVICE", NULL);
    const char *keyboard_device = getenv_default("LV_LINUX_KEYBOARD_DEVICE", "/dev/input/by-path/platform-fe204000.i2c-event");
    const char *keyboard_map = getenv_default("LV_LINUX_KEYBOARD_MAP", "/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map");
    // /home/nihao/w2T/github/m5stack-linux-dtoverlays/modules/tca8418-1.0/tca8418_keypad_m5stack_keymap.map


    lv_indev_t *touch = NULL;
    if (mouse_device)
        touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, mouse_device);

    lv_indev_t *keyboard = NULL;
    (void)keyboard_map;
    if (keyboard_device)
        keyboard = lv_evdev_create(LV_INDEV_TYPE_KEYPAD, keyboard_device);
    (void)keyboard;
    raw_keyboard_init();
}
#endif

#if LV_USE_LINUX_FBDEV
static void lv_linux_disp_init(void)
{
    // export LV_LINUX_FBDEV_DEVICE="/dev/fb$(grep 'fb_st7789v' /proc/fb | awk '{print $1}')"
    const char *device = NULL;
    char fbdev[64] = {0};
    device = getenv_default("LV_LINUX_FBDEV_DEVICE", NULL);
    if ((device == NULL) && (get_st7789v_fbdev(fbdev, sizeof(fbdev)) == 0))
    {
        device = fbdev;
    }
    printf("Using framebuffer device: %s\n", device);
    lv_display_t *disp = lv_linux_fbdev_create();
    if (disp == NULL)
    {
        printf("Failed to create fbdev display!\n");
        return;
    }

    lv_linux_fbdev_set_file(disp, device);

    // 打印获取到的分辨率
    lv_coord_t w = lv_display_get_horizontal_resolution(disp);
    lv_coord_t h = lv_display_get_vertical_resolution(disp);
    printf("Framebuffer resolution: %dx%d\n", w, h);
}
#if !LV_USE_EVDEV && !LV_USE_LIBINPUT
static void lv_linux_indev_init(void)
{
}
#endif

#elif LV_USE_LINUX_DRM
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_DRM_CARD", "/dev/dri/card0");
    lv_display_t *disp = lv_linux_drm_create();

    lv_linux_drm_set_file(disp, device, -1);
}
#elif LV_USE_SDL
static void lv_linux_disp_init(void)
{
    const char *width_env = getenv("LV_SDL_VIDEO_WIDTH");
    const char *height_env = getenv("LV_SDL_VIDEO_HEIGHT");
    const int width = atoi(width_env ? width_env : "320");
    const int height = atoi(height_env ? height_env : "170");

    lv_sdl_window_create(width, height);
}

static void lv_linux_indev_init(void)
{
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();
}

#else
static void lv_linux_disp_init(void)
{
    printf("No supported LVGL display backend enabled.\n");
}

static void lv_linux_indev_init(void)
{
}
#endif

int main(void)
{

    lv_init();

    /*Linux display device init*/
    lv_linux_disp_init();

    lv_linux_indev_init();
    clear_screen_on_boot();
    /*Create a Demo*/
    // lv_demo_widgets();
    // lv_demo_widgets_start_slideshow();
    // lv_demo_music();

    ui_init();
    bind_keypad_group();
    lora_init_hardware();
    if (g_lora_hw_ready) {
        render_home_menu();
    } else {
        lora_render_page();
    }
    // lv_demo_widgets(); // 用LVGL自带demo测试
    /*Handle LVGL tasks*/
    printf("Entering main loop...\n");
    while (1)
    {
        raw_keyboard_poll();
        lora_poll_irq_and_update_ui();
        lv_timer_handler();
        usleep(1000);
    }

    return 0;
}
