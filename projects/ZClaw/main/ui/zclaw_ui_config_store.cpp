#include "zclaw_ui_config_store.h"

#include "zclaw_atomic_file.h"
#include "zclaw_config_document_model.h"
#include "zclaw_text_file.h"

namespace zclaw {

bool load_ui_config(const std::string &path, UiConfig *config)
{
    return load_ui_config_with_status(path, config) ==
           ConfigStoreLoadStatus::Loaded;
}

ConfigStoreLoadStatus load_ui_config_with_status(
    const std::string &path, UiConfig *config, std::string *error)
{
    if (error)
        error->clear();
    if (!config) {
        if (error)
            *error = "Missing UI settings output.";
        return ConfigStoreLoadStatus::Error;
    }

    const TextFileReadResult document = read_text_file(path);
    if (document.status == TextFileReadStatus::NotFound)
        return ConfigStoreLoadStatus::NotFound;
    if (document.status == TextFileReadStatus::Error) {
        if (error)
            *error = document.error;
        return ConfigStoreLoadStatus::Error;
    }
    UiConfig parsed;
    if (!parse_ui_config_document_checked(document.contents, &parsed)) {
        if (error)
            *error = "UI settings file is invalid.";
        return ConfigStoreLoadStatus::Invalid;
    }
    *config = std::move(parsed);
    return ConfigStoreLoadStatus::Loaded;
}

bool save_ui_config(const std::string &path, const UiConfig &config, std::string *error)
{
    return atomic_write_file(path, make_ui_config_document(config), {}, error);
}

}  // namespace zclaw
