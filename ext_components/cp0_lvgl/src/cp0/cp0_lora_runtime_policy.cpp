#include "cp0_lora_runtime_policy.hpp"

namespace cp0_lora_runtime_policy {
namespace {

bool interval_elapsed(uint64_t start_ms, uint64_t now_ms, uint64_t interval_ms)
{
    return now_ms >= start_ms && now_ms - start_ms >= interval_ms;
}

} // namespace

bool should_timeout_transmit(bool initialized, bool tx_in_progress,
                             bool radio_available, uint64_t tx_start_ms,
                             uint64_t now_ms)
{
    return initialized && tx_in_progress && radio_available && tx_start_ms != 0 &&
           interval_elapsed(tx_start_ms, now_ms, TX_TIMEOUT_MS);
}

bool should_send_demo(bool initialized, bool tx_mode, bool tx_in_progress,
                      uint64_t last_auto_tx_ms, uint64_t now_ms)
{
    return initialized && tx_mode && !tx_in_progress &&
           interval_elapsed(last_auto_tx_ms, now_ms, AUTO_TX_INTERVAL_MS);
}

} // namespace cp0_lora_runtime_policy
