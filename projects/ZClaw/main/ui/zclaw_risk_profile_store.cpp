#include "zclaw_risk_profile_store.h"

#include "zclaw_atomic_file.h"
#include "zclaw_risk_profile_model.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace zclaw {
namespace {

void set_error(std::string *error, const char *message)
{
    if (error)
        *error = message;
}

}  // namespace

bool RiskProfileStore::ensure_default(const std::string &path,
                                      std::string *error) const
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        set_error(error, "Cannot read ZeroClaw config.");
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        set_error(error, "Cannot read ZeroClaw config.");
        return false;
    }
    RiskProfileUpdate update = add_default_risk_profile(buffer.str());
    if (!update.changed)
        return true;

    struct stat state {};
    const std::uint32_t permissions =
        ::stat(path.c_str(), &state) == 0
            ? static_cast<std::uint32_t>(state.st_mode & 0777)
            : 0600;
    return atomic_write_file(path, update.contents, {permissions}, error);
}

}  // namespace zclaw
