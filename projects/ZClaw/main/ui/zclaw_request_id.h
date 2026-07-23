#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace zclaw {

class RequestIdGenerator {
public:
    using Clock = std::function<std::uint64_t()>;

    explicit RequestIdGenerator(Clock clock = {});

    std::string next();

private:
    Clock clock_;
    std::atomic<std::uint64_t> sequence_{0};
};

}  // namespace zclaw
