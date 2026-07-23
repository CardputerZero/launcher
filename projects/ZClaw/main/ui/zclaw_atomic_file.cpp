#include "zclaw_atomic_file.h"

#include "zclaw_directory_sync.h"

#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace zclaw {
namespace {

bool write_all(int fd, const std::string &data)
{
    std::size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t written =
            ::write(fd, data.data() + offset, data.size() - offset);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (written == 0) {
            errno = EIO;
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

void set_error(std::string *error, const char *operation)
{
    if (error)
        *error = std::string(operation) + ": " + std::strerror(errno);
}

}  // namespace

bool atomic_write_file(const std::string &path, const std::string &contents,
                       const AtomicFileOptions &options, std::string *error)
{
    if (error)
        error->clear();

    std::string temporary_path = path + ".tmp.XXXXXX";
    std::vector<char> temporary_name(temporary_path.begin(), temporary_path.end());
    temporary_name.push_back('\0');
    const int fd = ::mkstemp(temporary_name.data());
    if (fd < 0) {
        set_error(error, "Could not create temporary file");
        return false;
    }

    bool ok = ::fchmod(fd, static_cast<mode_t>(options.permissions & 0777)) == 0;
    int saved_errno = ok ? 0 : errno;
    if (ok && !write_all(fd, contents)) {
        ok = false;
        saved_errno = errno;
    }
    if (ok && ::fsync(fd) != 0) {
        ok = false;
        saved_errno = errno;
    }
    if (::close(fd) != 0 && ok) {
        ok = false;
        saved_errno = errno;
    }
    if (!ok) {
        errno = saved_errno;
        set_error(error, "Could not write file");
        ::unlink(temporary_name.data());
        return false;
    }

    if (::rename(temporary_name.data(), path.c_str()) != 0) {
        set_error(error, "Could not replace file");
        ::unlink(temporary_name.data());
        return false;
    }
    return sync_parent_directory(path, error);
}

}  // namespace zclaw
