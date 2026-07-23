#include "cp0_desktop_exec_policy.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace cp0_desktop_exec_policy {
namespace {

bool contains_shell_meta(const char *text)
{
    static const char *kShellMetacharacters = "|&;<>`$\\\n\r";
    return !text || std::strpbrk(text, kShellMetacharacters) != nullptr;
}

std::string first_token(const char *exec)
{
    std::istringstream stream(exec ? exec : "");
    std::string token;
    stream >> token;
    return token;
}

bool file_executable(const std::string &path)
{
#if defined(_WIN32)
    (void)path;
    return false;
#else
    struct stat status;
    return stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
           access(path.c_str(), X_OK) == 0;
#endif
}

Result reject(const char *reason)
{
    return {false, reason};
}

} // namespace

Result evaluate(const char *exec)
{
    if (!exec || !exec[0]) return reject("empty Exec");
    if (std::strlen(exec) > 512) return reject("Exec too long");
    if (contains_shell_meta(exec)) return reject("Exec contains shell metacharacters");

    const std::string token = first_token(exec);
    if (token.empty()) return reject("missing executable");

    if (token.find('/') != std::string::npos) {
        return file_executable(token) ? Result{true, {}}
                                      : reject("executable path is not executable");
    }

    static const char *kAllowedNames[] = {"bash", "python3", "vim", "vi", "nano", "sh"};
    const bool allowed =
        std::find(std::begin(kAllowedNames), std::end(kAllowedNames), token) !=
        std::end(kAllowedNames);
    return allowed ? Result{true, {}} : reject("executable name is not allowlisted");
}

} // namespace cp0_desktop_exec_policy
