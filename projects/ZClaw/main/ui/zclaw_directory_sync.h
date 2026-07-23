#pragma once

#include <string>

namespace zclaw {

bool sync_parent_directory(const std::string &path,
                           std::string *error = nullptr);

}  // namespace zclaw
