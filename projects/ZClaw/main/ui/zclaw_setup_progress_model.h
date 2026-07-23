#pragma once

#include "zclaw_types.h"

#include <string>

namespace zclaw {

struct SetupProgressPresentation {
    std::string status;
    std::string detail;
    std::string percent_text;
    std::string speed;
    std::string source_url;
    std::string destination_path;
    int percent = 0;
};

SetupProgressPresentation present_setup_progress(const SetupProgress &progress);

}  // namespace zclaw
