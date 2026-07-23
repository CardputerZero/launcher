#include "zclaw_ui_config_manager.h"

#include "zclaw_storage.h"
#include "zclaw_ui_config_store.h"

#include <utility>

namespace zclaw {

UiConfigManager::UiConfigManager(std::string storage_path)
    : storage_path_(std::move(storage_path))
{
}

ConfigStoreLoadStatus UiConfigManager::load(std::string *error)
{
    UiConfig loaded;
    const ConfigStoreLoadStatus status =
        load_ui_config_with_status(storage_path_, &loaded, error);
    if (status == ConfigStoreLoadStatus::Loaded)
        config_ = std::move(loaded);
    return status;
}

const UiConfig &UiConfigManager::config() const
{
    return config_;
}

bool UiConfigManager::update(const UiConfig &config, std::string *error)
{
    if (!ensure_private_parent_directory(storage_path_, error) ||
        !save_ui_config(storage_path_, config, error))
        return false;
    config_ = config;
    return true;
}

bool UiConfigManager::clear_token(std::string *error)
{
    UiConfig candidate = config_;
    candidate.bearer_token.clear();
    return update(candidate, error);
}

}  // namespace zclaw
