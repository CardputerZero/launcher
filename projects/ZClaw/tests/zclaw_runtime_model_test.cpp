#include "zclaw_runtime_model.h"

#include <cassert>

int main()
{
    zclaw::RuntimeReadiness readiness;
    assert(zclaw::first_run_needed(readiness));

    readiness.zeroclaw_directory_exists = true;
    assert(zclaw::first_run_needed(readiness));
    readiness.config_file_exists = true;
    assert(zclaw::first_run_needed(readiness));
    readiness.setup_complete = true;
    assert(zclaw::first_run_needed(readiness));
    readiness.bearer_token_present = true;
    assert(!zclaw::first_run_needed(readiness));

    const zclaw::RuntimeReadiness missing_directory = {
        false, true, true, true
    };
    const zclaw::RuntimeReadiness missing_config = {
        true, false, true, true
    };
    const zclaw::RuntimeReadiness incomplete_setup = {
        true, true, false, true
    };
    const zclaw::RuntimeReadiness missing_token = {
        true, true, true, false
    };
    assert(zclaw::first_run_needed(missing_directory));
    assert(zclaw::first_run_needed(missing_config));
    assert(zclaw::first_run_needed(incomplete_setup));
    assert(zclaw::first_run_needed(missing_token));
    return 0;
}
