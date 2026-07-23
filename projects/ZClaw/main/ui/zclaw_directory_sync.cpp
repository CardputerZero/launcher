#include "zclaw_directory_sync.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace zclaw {

bool sync_parent_directory(const std::string &path, std::string *error)
{
    if (error)
        error->clear();
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (parent.empty())
        parent = ".";

    int flags = O_RDONLY;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif
    const int directory = ::open(parent.c_str(), flags);
    if (directory < 0) {
        if (error)
            *error = "Could not sync parent directory: " +
                     std::string(std::strerror(errno));
        return false;
    }

    struct stat status {};
    const int stat_result = ::fstat(directory, &status);
    bool ok = stat_result == 0 && S_ISDIR(status.st_mode);
    int saved_error = ok ? 0 : (stat_result != 0 ? errno : ENOTDIR);
    while (ok && ::fsync(directory) != 0) {
        if (errno == EINTR)
            continue;
#if defined(__APPLE__)
        if (errno == EINVAL)  // Darwin does not support fsync on directories.
            break;
#endif
#if defined(ENOTSUP)
        if (errno == ENOTSUP)
            break;
#endif
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP
        if (errno == EOPNOTSUPP)
            break;
#endif
        ok = false;
        saved_error = errno;
    }
    if (::close(directory) != 0 && ok) {
        ok = false;
        saved_error = errno;
    }
    if (!ok && error)
        *error = "Could not sync parent directory: " +
                 std::string(std::strerror(saved_error));
    return ok;
}

}  // namespace zclaw
