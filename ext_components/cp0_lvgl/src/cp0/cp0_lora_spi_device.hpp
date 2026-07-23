#pragma once

#include <cstddef>
#include <cstdint>

namespace cp0_lora {

class SpiDevice
{
public:
    explicit SpiDevice(const char *path = "/dev/spidev0.1", uint32_t speed_hz = 1000000);
    ~SpiDevice();

    SpiDevice(const SpiDevice &) = delete;
    SpiDevice &operator=(const SpiDevice &) = delete;

    const char *path() const;
    void set_path(const char *path);
    bool is_open() const;
    bool open();
    void close();
    bool transfer(const uint8_t *tx, uint8_t *rx, size_t length);

private:
    int fd_ = -1;
    char path_[64] = {};
    uint32_t speed_hz_;
};

} // namespace cp0_lora
