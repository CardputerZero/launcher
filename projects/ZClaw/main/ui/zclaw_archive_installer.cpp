#include "zclaw_archive_installer.h"

#include "zclaw_directory_sync.h"
#include "zclaw_process_runner.h"
#include "zclaw_process_executor.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace zclaw {
namespace {

std::filesystem::path find_file(const std::string &directory,
                                const std::string &filename)
{
    std::error_code filesystem_error;
    const std::filesystem::recursive_directory_iterator end;
    for (std::filesystem::recursive_directory_iterator item(
             directory, std::filesystem::directory_options::skip_permission_denied,
             filesystem_error);
         !filesystem_error && item != end; item.increment(filesystem_error)) {
        const std::filesystem::file_status status =
            item->symlink_status(filesystem_error);
        if (!filesystem_error && std::filesystem::is_regular_file(status) &&
            item->path().filename() == filename)
            return item->path();
    }
    return {};
}

bool copy_all(int source, int destination)
{
    char buffer[16384];
    while (true) {
        ssize_t count = ::read(source, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return count == 0;
        ssize_t offset = 0;
        while (offset < count) {
            const ssize_t written = ::write(
                destination, buffer + offset,
                static_cast<std::size_t>(count - offset));
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0) {
                if (written == 0)
                    errno = EIO;
                return false;
            }
            offset += written;
        }
    }
}

bool copy_executable(const std::filesystem::path &source,
                     const std::string &destination, std::string *error)
{
    const int source_fd = ::open(source.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (source_fd < 0) {
        if (error)
            *error = "Cannot open executable: " + std::string(std::strerror(errno));
        return false;
    }

    struct stat source_status {};
    const int stat_result = ::fstat(source_fd, &source_status);
    if (stat_result != 0 || !S_ISREG(source_status.st_mode)) {
        const int saved_error = stat_result != 0 ? errno : EINVAL;
        ::close(source_fd);
        if (error)
            *error = "Cannot open executable: " +
                     std::string(std::strerror(saved_error));
        return false;
    }

    std::string temporary_template = destination + ".tmp.XXXXXX";
    std::vector<char> temporary(temporary_template.begin(),
                                temporary_template.end());
    temporary.push_back('\0');
    const int destination_fd = ::mkstemp(temporary.data());
    if (destination_fd < 0) {
        const int saved_error = errno;
        ::close(source_fd);
        if (error)
            *error = "Cannot install executable: " +
                     std::string(std::strerror(saved_error));
        return false;
    }

    const int descriptor_flags = ::fcntl(destination_fd, F_GETFD);
    if (descriptor_flags < 0 ||
        ::fcntl(destination_fd, F_SETFD, descriptor_flags | FD_CLOEXEC) != 0) {
        const int saved_error = errno;
        ::close(source_fd);
        ::close(destination_fd);
        ::unlink(temporary.data());
        if (error)
            *error = "Cannot install executable: " +
                     std::string(std::strerror(saved_error));
        return false;
    }

    bool ok = ::fchmod(destination_fd, 0700) == 0 &&
              copy_all(source_fd, destination_fd) &&
              ::fsync(destination_fd) == 0;
    int saved_error = ok ? 0 : errno;
    if (::close(source_fd) != 0 && ok) {
        ok = false;
        saved_error = errno;
    }
    if (::close(destination_fd) != 0 && ok) {
        ok = false;
        saved_error = errno;
    }
    if (ok && ::rename(temporary.data(), destination.c_str()) != 0) {
        ok = false;
        saved_error = errno;
    }
    if (!ok) {
        ::unlink(temporary.data());
        if (error)
            *error = "Cannot install executable: " +
                     std::string(std::strerror(saved_error));
        return false;
    }
    return sync_parent_directory(destination, error);
}

}  // namespace

ArchiveInstaller::ArchiveInstaller()
    : ArchiveInstaller([](const Command &command) {
          ProcessExecutor executor;
          return executor.run(command);
      })
{
}

ArchiveInstaller::ArchiveInstaller(CommandRunner command_runner)
    : command_runner_(std::move(command_runner))
{
}

bool ArchiveInstaller::install_executable(
    const std::string &archive_path, const std::string &work_directory,
    const std::string &executable_name, const std::string &destination,
    std::string *error) const
{
    if (error)
        error->clear();
    const CommandResult extracted = command_runner_(
        {"tar", "-xzf", archive_path, "-C", work_directory});
    if (!extracted.ok()) {
        if (error)
            *error = "Cannot extract archive.\n" + extracted.output;
        return false;
    }

    const std::filesystem::path source = find_file(work_directory, executable_name);
    if (source.empty()) {
        if (error)
            *error = "Archive does not contain " + executable_name + ".";
        return false;
    }
    return copy_executable(source, destination, error);
}

}  // namespace zclaw
