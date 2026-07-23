#pragma once

#include <cstddef>

namespace cp0_lora_device_discovery {

size_t collect_spi_candidates(char out[][64], size_t max_count, const char *preferred);

} // namespace cp0_lora_device_discovery
