#pragma once

#include <string>

namespace zclaw {

bool sha256_file_matches(const std::string &path,
                         const std::string &expected_hex);
bool cleanup_binary_install_artifacts(const std::string &directory,
                                      std::string *error = nullptr);

}  // namespace zclaw
