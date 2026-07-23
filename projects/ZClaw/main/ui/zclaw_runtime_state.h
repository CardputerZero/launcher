#pragma once

#include "zclaw_types.h"

namespace zclaw {

class RuntimeState {
public:
    static bool first_run_needed(const UiConfig &config);
};

}  // namespace zclaw
