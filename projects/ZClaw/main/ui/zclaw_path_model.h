#pragma once

#include <string>

namespace zclaw {

struct ZClawPaths {
    std::string home;
    std::string zeroclaw_directory;
    std::string providers_config;
    std::string ui_config;
    std::string zeroclaw_config;
    std::string zeroclaw_binary;
};

std::string select_home_directory(const std::string &environment_home,
                                  const std::string &account_home);
ZClawPaths make_zclaw_paths(const std::string &home_directory);

}  // namespace zclaw
