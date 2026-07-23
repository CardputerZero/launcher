#pragma once

#include <string>

namespace zclaw {

class RiskProfileStore {
public:
    bool ensure_default(const std::string &path, std::string *error) const;
};

}  // namespace zclaw
