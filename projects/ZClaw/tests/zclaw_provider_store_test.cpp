#include "zclaw_provider_store.h"

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

bool same_provider(const ProviderConfig &left, const ProviderConfig &right)
{
    return left.alias == right.alias && left.family == right.family &&
           left.model == right.model && left.uri == right.uri &&
           left.api_key == right.api_key;
}

}  // namespace

int main()
{
    char dir_template[] = "/tmp/zclaw-provider-store-XXXXXX";
    const char *dir = ::mkdtemp(dir_template);
    assert(dir);
    const std::string path = std::string(dir) + "/providers.tsv";

    const std::vector<ProviderConfig> original = {
        {"zclaw", "custom", "model\\name\nnext", "https://example.com/a\tb", "key\\value\nline"},
        {"second", "ollama", "llama3.1", "http://127.0.0.1:11434", ""},
        {"zclaw", "openai", "gpt-4.1-mini", "https://api.openai.com/v1", "openai-key"},
    };
    std::string error;
    assert(zclaw::save_provider_configs(path, original, &error));
    assert(error.empty());

    struct stat st {};
    assert(::stat(path.c_str(), &st) == 0);
    assert((st.st_mode & 0777) == 0600);

    std::vector<ProviderConfig> loaded;
    assert(zclaw::load_provider_configs(path, &loaded));
    assert(loaded.size() == original.size());
    for (size_t i = 0; i < original.size(); ++i)
        assert(same_provider(loaded[i], original[i]));

    ProviderConfig active = loaded[0];
    const ProviderConfig custom_default = {
        "zclaw", "custom", "", "https://api.example.com/v1", ""
    };
    zclaw::activate_provider_config(&loaded, &active, custom_default);
    assert(active.uri == original[0].uri);
    assert(loaded[0].uri == original[0].uri);

    const ProviderConfig openai_default = {
        "zclaw", "openai", "default-model", "https://default.invalid/v1", ""
    };
    zclaw::activate_provider_config(&loaded, &active, openai_default);
    assert(active.family == "openai");
    assert(active.api_key == "openai-key");
    active.model = "edited-openai-model";
    zclaw::activate_provider_config(&loaded, &active, custom_default);
    assert(active.family == "custom");
    assert(active.uri == original[0].uri);
    zclaw::activate_provider_config(&loaded, &active, openai_default);
    assert(active.model == "edited-openai-model");

    ProviderConfig edited_active = active;
    edited_active.api_key = "updated-key";
    assert(zclaw::replace_provider_config(&loaded, &active, 0, edited_active));
    assert(active.api_key == "updated-key");
    assert(loaded[0].api_key == "updated-key");
    const ProviderConfig fallback = {
        "zclaw", "openai", "fallback", "https://api.openai.com/v1", ""
    };
    assert(zclaw::erase_provider_config(&loaded, &active, 0, fallback));
    assert(!loaded.empty());
    assert(same_provider(active, loaded[0]));

    std::vector<ProviderConfig> only_active = {active};
    assert(zclaw::erase_provider_config(&only_active, &active, 0, fallback));
    assert(only_active.empty());
    assert(same_provider(active, fallback));

    const std::string ui_path = std::string(dir) + "/ui.tsv";
    assert(zclaw::atomic_write_config(ui_path, "token\tsecret\n", &error));
    struct stat ui_st {};
    assert(::stat(ui_path.c_str(), &ui_st) == 0);
    assert((ui_st.st_mode & 0777) == 0600);

    const std::vector<ProviderConfig> replacement = {
        {"zclaw", "openai", "gpt-4.1-mini", "https://api.openai.com/v1", "new-key"},
    };
    assert(zclaw::save_provider_configs(path, replacement, &error));
    assert(zclaw::load_provider_configs(path, &loaded));
    assert(loaded.size() == 1 && same_provider(loaded[0], replacement[0]));

    {
        std::ofstream legacy(path, std::ios::trunc);
        assert(legacy);
        legacy << "legacy\tcustom\tmodel\\tname\thttps://example.com/v1\tkey\\\\value\n";
    }
    assert(zclaw::load_provider_configs(path, &loaded));
    assert(loaded.size() == 1);
    assert(loaded[0].alias == "legacy");
    assert(loaded[0].model == "model\tname");
    assert(loaded[0].api_key == "key\\value");

    assert(!zclaw::save_provider_configs(std::string(dir) + "/missing/providers.tsv",
                                         original, &error));
    assert(!error.empty());
    assert(zclaw::load_provider_configs(path, &loaded));
    assert(loaded.size() == 1 && loaded[0].alias == "legacy");

    assert(::unlink(path.c_str()) == 0);
    assert(::unlink(ui_path.c_str()) == 0);
    assert(::rmdir(dir) == 0);
    return 0;
}
