#include "zclaw_runtime_state.h"

#include "zclaw_paths.h"
#include "zclaw_runtime_model.h"

#include <string>
#include <sys/stat.h>

namespace zclaw {
namespace {

bool is_regular_file(const std::string &path)
{
    struct stat state {};
    return ::stat(path.c_str(), &state) == 0 && S_ISREG(state.st_mode);
}

bool is_directory(const std::string &path)
{
    struct stat state {};
    return ::stat(path.c_str(), &state) == 0 && S_ISDIR(state.st_mode);
}

}  // namespace

bool RuntimeState::first_run_needed(const UiConfig &config)
{
    const RuntimeReadiness readiness = {
        is_directory(paths::zeroclaw_dir()),
        is_regular_file(paths::zeroclaw_config()),
        config.setup_complete,
        !config.bearer_token.empty(),
    };
    return zclaw::first_run_needed(readiness);
}

}  // namespace zclaw
