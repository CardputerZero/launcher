#include "zclaw_provider_collection_model.h"

#include <cassert>
#include <vector>

namespace {

bool same_provider(const zclaw::ProviderConfig &left,
                   const zclaw::ProviderConfig &right)
{
    return left.alias == right.alias && left.family == right.family &&
           left.model == right.model && left.uri == right.uri &&
           left.api_key == right.api_key;
}

}  // namespace

int main()
{
    using zclaw::ProviderConfig;

    const ProviderConfig custom = {
        "zclaw", "custom", "custom-model", "https://custom.example/v1", "custom-key"
    };
    const ProviderConfig openai = {
        "zclaw", "openai", "openai-model", "https://api.openai.com/v1", "openai-key"
    };
    const ProviderConfig ollama = {
        "local", "ollama", "llama", "http://127.0.0.1:11434", ""
    };
    std::vector<ProviderConfig> providers = {custom, ollama, openai};
    ProviderConfig active = custom;

    zclaw::activate_provider_config(&providers, &active, custom);
    assert(same_provider(active, custom));
    zclaw::activate_provider_config(&providers, &active, openai);
    assert(same_provider(active, openai));
    assert(providers.size() == 3);
    assert(same_provider(providers[0], openai));

    active.model = "edited-openai";
    zclaw::activate_provider_config(&providers, &active, custom);
    zclaw::activate_provider_config(&providers, &active, openai);
    assert(active.model == "edited-openai");

    ProviderConfig replacement = ollama;
    replacement.model = "new-local-model";
    assert(zclaw::replace_provider_config(&providers, &active, 1, replacement));
    assert(active.family == "openai");
    assert(providers[1].model == "new-local-model");
    assert(!zclaw::replace_provider_config(&providers, &active,
                                           providers.size(), replacement));
    assert(!zclaw::replace_provider_config(nullptr, &active, 0, replacement));

    const ProviderConfig fallback = {
        "zclaw", "custom", "fallback", "https://fallback.example/v1", ""
    };
    assert(zclaw::erase_provider_config(&providers, &active, 1, fallback));
    assert(active.family == "openai");
    assert(zclaw::erase_provider_config(&providers, &active, 0, fallback));
    assert(same_provider(active, providers[0]));

    std::vector<ProviderConfig> only_active = {active};
    assert(zclaw::erase_provider_config(&only_active, &active, 0, fallback));
    assert(only_active.empty());
    assert(same_provider(active, fallback));
    assert(!zclaw::erase_provider_config(&only_active, &active, 0, fallback));

    zclaw::activate_provider_config(nullptr, &active, custom);
    zclaw::activate_provider_config(&providers, nullptr, custom);
    return 0;
}
