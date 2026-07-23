#pragma once

#include "zclaw_types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace zclaw {

std::size_t provider_preset_count();
ProviderConfig provider_preset(std::size_t index);
std::size_t provider_preset_index(const std::string &family);
const char *provider_display_name(const std::string &family);
std::vector<ProviderConfig> default_provider_configs();

}  // namespace zclaw
