#pragma once

#include "zclaw_types.h"

#include <string>
#include <vector>

namespace zclaw {

struct CliConfigEntry {
    std::string path;
    std::string value;
};

struct CliConfigPlan {
    std::string agent_alias;
    std::string webhook_url;
    std::vector<CliConfigEntry> initial_settings;
    std::vector<CliConfigEntry> agent_settings;
};

CliConfigPlan make_cli_config_plan(const UiConfig &config,
                                   const ProviderConfig &provider);

}  // namespace zclaw
