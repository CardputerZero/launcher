#include "zclaw_provider_manager.h"
#include "zclaw_provider_store.h"
#include "zclaw_ui_config_manager.h"
#include "zclaw_ui_config_store.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

int main()
{
    char directory[] = "/tmp/zclaw-manager-load-test-XXXXXX";
    const char *root = ::mkdtemp(directory);
    assert(root);
    const std::filesystem::path base(root);
    const std::string provider_path = (base / "providers.tsv").string();
    const std::string ui_path = (base / "ui.tsv").string();
    std::string error;

    zclaw::ProviderManager providers(provider_path);
    assert(providers.load(&error) == zclaw::ConfigStoreLoadStatus::NotFound);
    assert(!providers.providers().empty());
    const std::string default_family = providers.setup_provider().family;

    std::ofstream(provider_path) << "damaged\n";
    assert(providers.load(&error) == zclaw::ConfigStoreLoadStatus::Invalid);
    assert(error == "Provider settings file is invalid.");
    assert(providers.setup_provider().family == default_family);

    const std::vector<zclaw::ProviderConfig> saved = {
        {"active", "custom", "model", "https://example.test", "key"},
    };
    assert(zclaw::save_provider_configs(provider_path, saved, &error));
    assert(providers.load(&error) == zclaw::ConfigStoreLoadStatus::Loaded);
    assert(providers.setup_provider().alias == "active");

    zclaw::UiConfigManager ui(ui_path);
    assert(ui.load(&error) == zclaw::ConfigStoreLoadStatus::NotFound);
    assert(ui.config().agent_alias == "zclaw");
    std::ofstream(ui_path) << "unknown\tvalue\nmalformed\n";
    assert(ui.load(&error) == zclaw::ConfigStoreLoadStatus::Invalid);
    assert(error == "UI settings file is invalid.");
    assert(ui.config().agent_alias == "zclaw");

    zclaw::UiConfig saved_ui;
    saved_ui.agent_alias = "loaded-agent";
    assert(zclaw::save_ui_config(ui_path, saved_ui, &error));
    assert(ui.load(&error) == zclaw::ConfigStoreLoadStatus::Loaded);
    assert(ui.config().agent_alias == "loaded-agent");

    error.clear();
    zclaw::UiConfigManager directory_as_file(base.string());
    assert(directory_as_file.load(&error) == zclaw::ConfigStoreLoadStatus::Error);
    assert(!error.empty());

    std::error_code ignored;
    std::filesystem::remove_all(base, ignored);
    return 0;
}
