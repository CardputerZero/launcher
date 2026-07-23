#pragma once

#include "zclaw_config_store_status.h"
#include "zclaw_types.h"

#include <string>
#include <vector>

namespace zclaw {

bool load_provider_configs(const std::string &path, std::vector<ProviderConfig> *providers);
ConfigStoreLoadStatus load_provider_configs_with_status(
    const std::string &path, std::vector<ProviderConfig> *providers,
    std::string *error = nullptr);
bool save_provider_configs(const std::string &path, const std::vector<ProviderConfig> &providers,
                           std::string *error = nullptr);

}  // namespace zclaw
