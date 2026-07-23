#include "zclaw_provider_collection_model.h"

#include <algorithm>

namespace zclaw {

void activate_provider_config(std::vector<ProviderConfig> *providers,
                              ProviderConfig *active,
                              const ProviderConfig &selected_default)
{
    if (!providers || !active)
        return;

    if (providers->empty())
        providers->push_back(*active);
    else
        (*providers)[0] = *active;

    if (active->family == selected_default.family)
        return;

    const auto existing = std::find_if(
        providers->begin() + 1, providers->end(),
        [&](const ProviderConfig &provider) {
            return provider.family == selected_default.family;
        });
    ProviderConfig selected = selected_default;
    if (existing != providers->end()) {
        selected = *existing;
        providers->erase(existing);
    }
    providers->insert(providers->begin(), selected);
    *active = selected;
}

bool replace_provider_config(std::vector<ProviderConfig> *providers,
                             ProviderConfig *active, std::size_t index,
                             const ProviderConfig &replacement)
{
    if (!providers || !active || index >= providers->size())
        return false;
    (*providers)[index] = replacement;
    if (index == 0)
        *active = replacement;
    return true;
}

bool erase_provider_config(std::vector<ProviderConfig> *providers,
                           ProviderConfig *active, std::size_t index,
                           const ProviderConfig &empty_default)
{
    if (!providers || !active || index >= providers->size())
        return false;
    providers->erase(providers->begin() + static_cast<std::ptrdiff_t>(index));
    if (index == 0)
        *active = providers->empty() ? empty_default : (*providers)[0];
    return true;
}

}  // namespace zclaw
