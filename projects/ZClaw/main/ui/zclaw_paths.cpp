#include "zclaw_paths.h"

#include "zclaw_path_model.h"

#include <cerrno>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#include <vector>

namespace zclaw::paths {
namespace {

std::string account_home_directory()
{
    long buffer_size = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    if (buffer_size < 0)
        buffer_size = 16384;
    std::vector<char> buffer(static_cast<std::size_t>(buffer_size));
    while (buffer.size() <= 1024U * 1024U) {
        struct passwd account {};
        struct passwd *result = nullptr;
        const int status = ::getpwuid_r(::geteuid(), &account, buffer.data(),
                                       buffer.size(), &result);
        if (status == 0)
            return result && account.pw_dir ? account.pw_dir : "";
        if (status != ERANGE)
            return "";
        buffer.resize(buffer.size() * 2U);
    }
    return "";
}

ZClawPaths current_paths()
{
    const char *environment_home = std::getenv("HOME");
    return make_zclaw_paths(select_home_directory(
        environment_home ? environment_home : "", account_home_directory()));
}

}  // namespace

std::string home_dir()
{
    return current_paths().home;
}

std::string zeroclaw_dir()
{
    return current_paths().zeroclaw_directory;
}

std::string providers_config()
{
    return current_paths().providers_config;
}

std::string ui_config()
{
    return current_paths().ui_config;
}

std::string zeroclaw_config()
{
    return current_paths().zeroclaw_config;
}

std::string zeroclaw_binary()
{
    return current_paths().zeroclaw_binary;
}

std::string zeroclaw_archive()
{
    return zeroclaw_dir() + "/zeroclaw.tar.gz";
}

}  // namespace zclaw::paths
