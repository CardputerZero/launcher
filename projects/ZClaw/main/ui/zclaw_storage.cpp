#include "zclaw_storage.h"

#include <filesystem>
#include <system_error>
#include <vector>

namespace zclaw {
namespace {

namespace fs = std::filesystem;

bool set_owner_only_permissions(const fs::path &directory, std::string *error)
{
    std::error_code ec;
    fs::permissions(directory, fs::perms::owner_all,
                    fs::perm_options::replace, ec);
    if (!ec)
        return true;
    if (error)
        *error = "Could not secure settings directory: " + ec.message();
    return false;
}

}  // namespace

bool ensure_private_parent_directory(const std::string &path, std::string *error)
{
    const fs::path parent = fs::path(path).parent_path();
    if (parent.empty() || parent == parent.root_path())
        return true;

    std::vector<fs::path> missing;
    for (fs::path current = parent; !current.empty();) {
        std::error_code status_error;
        const fs::file_status status = fs::symlink_status(current, status_error);
        if (!status_error && fs::is_symlink(status)) {
            if (error)
                *error = "Could not secure settings directory: Symbolic link in path";
            return false;
        }
        if (!status_error && fs::exists(status))
            break;
        if (status_error && status_error != std::errc::no_such_file_or_directory) {
            if (error)
                *error = "Could not inspect settings directory: " +
                         status_error.message();
            return false;
        }
        missing.push_back(current);
        const fs::path next = current.parent_path();
        if (next == current)
            break;
        current = next;
    }

    std::error_code create_error;
    fs::create_directories(parent, create_error);
    if (create_error) {
        if (error)
            *error = "Could not create settings directory: " +
                     create_error.message();
        return false;
    }

    std::error_code status_error;
    const fs::file_status parent_status = fs::symlink_status(parent, status_error);
    if (status_error || !fs::is_directory(parent_status)) {
        if (error)
            *error = "Could not create settings directory: " +
                     (status_error ? status_error.message() : "Not a directory");
        return false;
    }

    for (auto directory = missing.rbegin(); directory != missing.rend(); ++directory) {
        if (!set_owner_only_permissions(*directory, error))
            return false;
    }
    if (missing.empty() && !set_owner_only_permissions(parent, error))
        return false;

    return true;
}

}  // namespace zclaw
