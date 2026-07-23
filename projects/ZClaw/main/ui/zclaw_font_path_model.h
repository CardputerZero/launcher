#pragma once

#include <functional>
#include <string>
#include <vector>

namespace zclaw {

using FontPathExists = std::function<bool(const std::string &)>;

std::string select_font_path(const std::string &override_path,
                             const std::vector<std::string> &candidates,
                             const FontPathExists &path_exists);

}  // namespace zclaw
