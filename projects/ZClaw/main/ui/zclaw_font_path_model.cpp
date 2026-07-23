#include "zclaw_font_path_model.h"

namespace zclaw {

std::string select_font_path(const std::string &override_path,
                             const std::vector<std::string> &candidates,
                             const FontPathExists &path_exists)
{
    if (!path_exists)
        return "";
    if (!override_path.empty() && path_exists(override_path))
        return override_path;
    for (const std::string &candidate : candidates) {
        if (!candidate.empty() && path_exists(candidate))
            return candidate;
    }
    return "";
}

}  // namespace zclaw
