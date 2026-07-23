#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cp0_runner {
struct Result { int exit_code = -1; std::string output; };
using Output = std::function<void(const char *, std::size_t)>;
using InputConsumed = std::function<void()>;
Result run(std::vector<std::string> argv, const std::string *input, Output output,
           const std::atomic<bool> *cancel, int timeout_ms,
           std::size_t tail_capacity = 64 * 1024,
           std::uint32_t uid = UINT32_MAX, std::uint32_t gid = UINT32_MAX,
           const std::string &user_name = {}, const std::string &home = {},
           const std::string &shell = {}, InputConsumed input_consumed = {});
}
