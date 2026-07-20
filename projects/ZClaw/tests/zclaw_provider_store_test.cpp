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
    assert(::rmdir(dir) == 0);
    return 0;
}
