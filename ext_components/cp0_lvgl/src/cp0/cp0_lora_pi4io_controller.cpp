#include "cp0_lora_pi4io_controller.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#if __has_include(<linux/i2c-dev.h>)
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#define CP0_PI4IO_HAS_LINUX_I2CDEV 1
#else
#define CP0_PI4IO_HAS_LINUX_I2CDEV 0
#endif

namespace cp0_lora_pi4io_controller {
namespace {

constexpr int I2C_BUS = 1;
constexpr int SDA_GPIO = 2;
constexpr int SCL_GPIO = 3;
constexpr uint8_t I2C_ADDRESS = 0x43;

char status_text[160] = "I2C 0x43 not checked";
uint8_t output_cache = 0x00;
uint8_t config_cache = 0xFF;
uint8_t polarity_cache = 0x00;

bool open_bus(int *fd)
{
#if !CP0_PI4IO_HAS_LINUX_I2CDEV
    if (fd) *fd = -1;
    snprintf(status_text, sizeof(status_text),
             "I2C dev header missing, cannot access 0x%02X", I2C_ADDRESS);
    return false;
#else
    if (fd == nullptr) {
        snprintf(status_text, sizeof(status_text), "I2C fd pointer invalid");
        return false;
    }
    char path[64];
    snprintf(path, sizeof(path), "/dev/i2c-%d", I2C_BUS);
    *fd = open(path, O_RDWR);
    if (*fd < 0) {
        snprintf(status_text, sizeof(status_text),
                 "open %s failed, SDA:%d SCL:%d errno=%d",
                 path, SDA_GPIO, SCL_GPIO, errno);
        return false;
    }
    return true;
#endif
}

bool select_device(int fd)
{
    if (fd < 0) {
        snprintf(status_text, sizeof(status_text),
                 "I2C fd invalid for 0x%02X", I2C_ADDRESS);
        return false;
    }
#if CP0_PI4IO_HAS_LINUX_I2CDEV
    if (ioctl(fd, I2C_SLAVE, I2C_ADDRESS) < 0) {
        snprintf(status_text, sizeof(status_text),
                 "select 0x%02X failed on /dev/i2c-%d errno=%d",
                 I2C_ADDRESS, I2C_BUS, errno);
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool write_register(int fd, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return write(fd, data, sizeof(data)) == (ssize_t)sizeof(data);
}

bool probe(int fd)
{
    uint8_t reg = 0x00;
    if (write(fd, &reg, 1) != 1) {
        snprintf(status_text, sizeof(status_text),
                 "I2C 0x%02X not found on /dev/i2c-%d (SDA:%d SCL:%d)",
                 I2C_ADDRESS, I2C_BUS, SDA_GPIO, SCL_GPIO);
        return false;
    }
    snprintf(status_text, sizeof(status_text),
             "I2C 0x%02X found on /dev/i2c-%d (SDA:%d SCL:%d)",
             I2C_ADDRESS, I2C_BUS, SDA_GPIO, SCL_GPIO);
    return true;
}

bool initialize(int fd)
{
    if (fd < 0) {
        snprintf(status_text, sizeof(status_text),
                 "I2C IO init invalid fd for 0x%02X", I2C_ADDRESS);
        return false;
    }

    polarity_cache = 0x00;
    output_cache = 0x01;
    config_cache = 0xFE;
    struct RegisterWrite {
        uint8_t reg;
        uint8_t value;
        const char *name;
    };
    const RegisterWrite writes[] = {
        {0x02, polarity_cache, "POL"},
        {0x01, output_cache, "OUT"},
        {0x03, config_cache, "CFG"},
    };
    for (const auto &item : writes) {
        errno = 0;
        if (!write_register(fd, item.reg, item.value)) {
            snprintf(status_text, sizeof(status_text),
                     "I2C IO write %s failed at 0x%02X errno=%d",
                     item.name, I2C_ADDRESS, errno);
            return false;
        }
    }

    snprintf(status_text, sizeof(status_text),
             "I2C IO init ok OUT=0x%02X POL=0x%02X CFG=0x%02X P0=HIGH",
             output_cache, polarity_cache, config_cache);
    return true;
}

} // namespace

bool scan_and_initialize()
{
    int fd = -1;
    if (!open_bus(&fd)) return false;
    const bool ok = select_device(fd) && probe(fd) && initialize(fd);
    close(fd);
    return ok;
}

const char *status()
{
    return status_text;
}

} // namespace cp0_lora_pi4io_controller
