#pragma once

#include "zclaw_client.h"

#include <string>
#include <vector>

namespace zclaw {

std::string encode_config_field(const std::string &value);
std::string decode_config_field(const std::string &value);
std::vector<std::string> split_config_line(const std::string &line);

bool load_provider_configs(const std::string &path, std::vector<ProviderConfig> *providers);
bool save_provider_configs(const std::string &path, const std::vector<ProviderConfig> &providers,
                           std::string *error = nullptr);

}  // namespace zclaw
