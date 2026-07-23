#pragma once

#include <string>

namespace zclaw::paths {

std::string home_dir();
std::string zeroclaw_dir();
std::string providers_config();
std::string ui_config();
std::string zeroclaw_config();
std::string zeroclaw_binary();
std::string zeroclaw_archive();

}  // namespace zclaw::paths
