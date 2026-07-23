#include "zclaw_provider_manager.h"

#include "zclaw_provider_catalog.h"
#include "zclaw_provider_collection_model.h"
#include "zclaw_provider_store.h"
#include "zclaw_storage.h"

#include <utility>

namespace zclaw {

ProviderManager::ProviderManager(std::string storage_path)
    : storage_path_(std::move(storage_path))
{
}

ConfigStoreLoadStatus ProviderManager::load(std::string *error)
{
    std::vector<ProviderConfig> loaded;
    const ConfigStoreLoadStatus status =
        load_provider_configs_with_status(storage_path_, &loaded, error);
    if (status == ConfigStoreLoadStatus::Loaded)
        providers_ = std::move(loaded);
    else if (status == ConfigStoreLoadStatus::NotFound || providers_.empty())
        providers_ = default_provider_configs();
    setup_provider_ = providers_.empty() ? provider_preset(0) : providers_.front();
    return status;
}

const std::vector<ProviderConfig> &ProviderManager::providers() const
{
    return providers_;
}

const ProviderConfig &ProviderManager::setup_provider() const
{
    return setup_provider_;
}

bool ProviderManager::update_setup_provider(const ProviderConfig &provider, std::string *error)
{
    std::vector<ProviderConfig> candidate = providers_;
    if (candidate.empty())
        candidate.push_back(provider);
    else
        candidate.front() = provider;
    if (!persist(candidate, error))
        return false;
    providers_ = std::move(candidate);
    setup_provider_ = provider;
    return true;
}

bool ProviderManager::activate(const ProviderConfig &provider, std::string *error)
{
    std::vector<ProviderConfig> candidate = providers_;
    ProviderConfig active = setup_provider_;
    activate_provider_config(&candidate, &active, provider);
    if (!persist(candidate, error))
        return false;
    providers_ = std::move(candidate);
    setup_provider_ = std::move(active);
    return true;
}

bool ProviderManager::add(const ProviderConfig &provider, std::size_t *index,
                          std::string *error)
{
    std::vector<ProviderConfig> candidate = providers_;
    candidate.push_back(provider);
    if (!persist(candidate, error))
        return false;
    providers_ = std::move(candidate);
    if (index)
        *index = providers_.size() - 1;
    return true;
}

bool ProviderManager::erase(std::size_t index, const ProviderConfig &empty_default,
                            std::string *error)
{
    std::vector<ProviderConfig> candidate = providers_;
    ProviderConfig active = setup_provider_;
    if (!erase_provider_config(&candidate, &active, index, empty_default))
        return false;
    if (!persist(candidate, error))
        return false;
    providers_ = std::move(candidate);
    setup_provider_ = std::move(active);
    return true;
}

bool ProviderManager::replace(std::size_t index, const ProviderConfig &provider,
                              std::string *error)
{
    std::vector<ProviderConfig> candidate = providers_;
    ProviderConfig active = setup_provider_;
    if (!replace_provider_config(&candidate, &active, index, provider))
        return false;
    if (!persist(candidate, error))
        return false;
    providers_ = std::move(candidate);
    setup_provider_ = std::move(active);
    return true;
}

bool ProviderManager::persist(const std::vector<ProviderConfig> &providers,
                              std::string *error) const
{
    if (!ensure_private_parent_directory(storage_path_, error))
        return false;
    return save_provider_configs(storage_path_, providers, error);
}

}  // namespace zclaw
