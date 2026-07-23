#pragma once

#include "zclaw_selection_model.h"
#include "zclaw_types.h"

#include <string>
#include <vector>

namespace zclaw {

struct SettingsRowPresentation {
    std::string title;
    std::string value;
};

struct SettingsPresentation {
    std::string title;
    std::string hint;
    std::vector<SettingsRowPresentation> rows;
};

SettingsPresentation present_settings_main(const UiConfig &config);
SettingsPresentation present_authorization(const UiConfig &config);
SettingsPresentation present_setup(const ProviderConfig &provider);
SettingsPresentation present_setup_providers(const PagedSelection &selection,
                                             int maximum_rows);
SettingsPresentation present_providers(const std::vector<ProviderConfig> &providers,
                                       const PagedSelection &selection,
                                       int maximum_rows);
SettingsPresentation present_provider_detail(const ProviderConfig &provider);
SettingsPresentation present_setup_progress();

}  // namespace zclaw
