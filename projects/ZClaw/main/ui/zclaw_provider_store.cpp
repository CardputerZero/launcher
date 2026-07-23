#include "zclaw_provider_store.h"

#include "zclaw_atomic_file.h"
#include "zclaw_config_document_model.h"
#include "zclaw_text_file.h"

namespace zclaw {

bool load_provider_configs(const std::string &path, std::vector<ProviderConfig> *providers)
{
    return load_provider_configs_with_status(path, providers) ==
           ConfigStoreLoadStatus::Loaded;
}

ConfigStoreLoadStatus load_provider_configs_with_status(
    const std::string &path, std::vector<ProviderConfig> *providers,
    std::string *error)
{
    if (error)
        error->clear();
    if (!providers) {
        if (error)
            *error = "Missing provider settings output.";
        return ConfigStoreLoadStatus::Error;
    }
    providers->clear();
    const TextFileReadResult document = read_text_file(path);
    if (document.status == TextFileReadStatus::NotFound)
        return ConfigStoreLoadStatus::NotFound;
    if (document.status == TextFileReadStatus::Error) {
        if (error)
            *error = document.error;
        return ConfigStoreLoadStatus::Error;
    }
    if (!parse_provider_config_document_checked(document.contents, providers)) {
        if (error)
            *error = "Provider settings file is invalid.";
        return ConfigStoreLoadStatus::Invalid;
    }
    return ConfigStoreLoadStatus::Loaded;
}

bool save_provider_configs(const std::string &path, const std::vector<ProviderConfig> &providers,
                           std::string *error)
{
    return atomic_write_file(path, make_provider_config_document(providers), {}, error);
}

}  // namespace zclaw
