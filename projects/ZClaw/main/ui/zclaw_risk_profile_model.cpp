#include "zclaw_risk_profile_model.h"

#include "zclaw_text.h"

#include <sstream>
#include <utility>

namespace zclaw {
RiskProfileUpdate add_default_risk_profile(std::string config)
{
    std::istringstream lines(config);
    std::string line;
    while (std::getline(lines, line)) {
        if (trim_ascii_whitespace(line) == "[risk_profiles.default]")
            return {std::move(config), false};
    }

    if (!config.empty() && config.back() != '\n')
        config += '\n';
    config += "\n[risk_profiles.default]\n";
    return {std::move(config), true};
}

}  // namespace zclaw
