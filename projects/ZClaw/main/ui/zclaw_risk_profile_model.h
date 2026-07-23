#pragma once

#include <string>

namespace zclaw {

struct RiskProfileUpdate {
    std::string contents;
    bool changed = false;
};

RiskProfileUpdate add_default_risk_profile(std::string config);

}  // namespace zclaw
