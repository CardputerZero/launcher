#include "zclaw_path_model.h"

#include <filesystem>

namespace zclaw {

std::string select_home_directory(const std::string &environment_home,
                                  const std::string &account_home)
{
    if (!environment_home.empty())
        return environment_home;
    if (!account_home.empty())
        return account_home;
    return ".";
}

ZClawPaths make_zclaw_paths(const std::string &home_directory)
{
    namespace fs = std::filesystem;
    ZClawPaths paths;
    const fs::path home = home_directory.empty() ? fs::path(".")
                                                  : fs::path(home_directory);
    const fs::path zeroclaw = home / ".zeroclaw";
    paths.home = home.string();
    paths.zeroclaw_directory = zeroclaw.string();
    paths.providers_config = (zeroclaw / "zclaw_providers.tsv").string();
    paths.ui_config = (zeroclaw / "zclaw_ui.tsv").string();
    paths.zeroclaw_config = (zeroclaw / "config.toml").string();
    paths.zeroclaw_binary = (zeroclaw / "bin" / "zeroclaw").string();
    return paths;
}

}  // namespace zclaw
