#pragma once

#include "zclaw_client.h"

#include <cstddef>
#include <string>
#include <vector>

namespace zclaw {

std::string encode_config_field(const std::string &value);
std::string decode_config_field(const std::string &value);
std::vector<std::string> split_config_line(const std::string &line);

void activate_provider_config(std::vector<ProviderConfig> *providers, ProviderConfig *active,
                              const ProviderConfig &selected_default);
bool replace_provider_config(std::vector<ProviderConfig> *providers, ProviderConfig *active,
                             std::size_t index, const ProviderConfig &replacement);
bool erase_provider_config(std::vector<ProviderConfig> *providers, ProviderConfig *active,
                           std::size_t index, const ProviderConfig &empty_default);

bool atomic_write_config(const std::string &path, const std::string &contents,
                         std::string *error = nullptr);
bool load_provider_configs(const std::string &path, std::vector<ProviderConfig> *providers);
bool save_provider_configs(const std::string &path, const std::vector<ProviderConfig> &providers,
                           std::string *error = nullptr);

}  // namespace zclaw
