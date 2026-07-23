#pragma once

#include "zclaw_types.h"

#include <cstddef>
#include <vector>

namespace zclaw {

void activate_provider_config(std::vector<ProviderConfig> *providers,
                              ProviderConfig *active,
                              const ProviderConfig &selected_default);
bool replace_provider_config(std::vector<ProviderConfig> *providers,
                             ProviderConfig *active, std::size_t index,
                             const ProviderConfig &replacement);
bool erase_provider_config(std::vector<ProviderConfig> *providers,
                           ProviderConfig *active, std::size_t index,
                           const ProviderConfig &empty_default);

}  // namespace zclaw
