#include "zclaw_request_id.h"

#include <chrono>
#include <utility>

namespace zclaw {
namespace {

std::uint64_t unix_time_seconds()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

}  // namespace

RequestIdGenerator::RequestIdGenerator(Clock clock)
    : clock_(clock ? std::move(clock) : Clock(unix_time_seconds))
{
}

std::string RequestIdGenerator::next()
{
    const std::uint64_t sequence =
        sequence_.fetch_add(1, std::memory_order_relaxed);
    return "zclaw-ui-" + std::to_string(clock_()) + "-" +
           std::to_string(sequence);
}

}  // namespace zclaw
