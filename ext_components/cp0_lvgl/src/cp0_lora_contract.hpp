#pragma once

#include "cp0_lvgl_app.h"

#include <functional>
#include <list>
#include <mutex>
#include <string>

namespace cp0::lora {

constexpr size_t kMaxTextPayloadBytes = 127;

struct Result {
    int code = -1;
    std::string payload;
};

struct Operations {
    std::function<bool()> initialize;
    std::function<void()> poll;
    std::function<void(cp0_lora_info_t *, bool)> read_info;
    std::function<bool(const std::string &)> send_text;
    std::function<void()> start_receive;
    std::function<void(bool)> set_tx_mode;
    std::function<void()> shutdown;
};

std::string encode_info(const cp0_lora_info_t &info);
bool decode_info(const std::string &payload, cp0_lora_info_t *info);
Result dispatch(const std::list<std::string> &arguments, const Operations &operations);

class Service {
public:
    explicit Service(Operations operations);
    Result call(const std::list<std::string> &arguments);

private:
    Operations operations_;
    std::mutex mutex_;
};

} // namespace cp0::lora
