#pragma once

namespace zclaw {

struct RuntimeReadiness {
    bool zeroclaw_directory_exists = false;
    bool config_file_exists = false;
    bool setup_complete = false;
    bool bearer_token_present = false;
};

bool first_run_needed(const RuntimeReadiness &readiness);

}  // namespace zclaw
