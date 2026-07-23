#include "zclaw_runtime_model.h"

namespace zclaw {

bool first_run_needed(const RuntimeReadiness &readiness)
{
    return !readiness.zeroclaw_directory_exists ||
           !readiness.config_file_exists || !readiness.setup_complete ||
           !readiness.bearer_token_present;
}

}  // namespace zclaw
