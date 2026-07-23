#pragma once

#include "zclaw_config_store_status.h"
#include "zclaw_types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace zclaw {

class ProviderManager {
public:
    explicit ProviderManager(std::string storage_path);

    ConfigStoreLoadStatus load(std::string *error = nullptr);
    const std::vector<ProviderConfig> &providers() const;
    const ProviderConfig &setup_provider() const;

    bool update_setup_provider(const ProviderConfig &provider, std::string *error = nullptr);
    bool activate(const ProviderConfig &provider, std::string *error = nullptr);
    bool add(const ProviderConfig &provider, std::size_t *index = nullptr,
             std::string *error = nullptr);
    bool erase(std::size_t index, const ProviderConfig &empty_default,
               std::string *error = nullptr);
    bool replace(std::size_t index, const ProviderConfig &provider,
                 std::string *error = nullptr);

private:
    bool persist(const std::vector<ProviderConfig> &providers, std::string *error) const;

    std::string storage_path_;
    std::vector<ProviderConfig> providers_;
    ProviderConfig setup_provider_;
};

}  // namespace zclaw
