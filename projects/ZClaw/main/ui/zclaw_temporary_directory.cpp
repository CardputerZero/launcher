#include "zclaw_temporary_directory.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <utility>
#include <vector>

#include <stdlib.h>

namespace zclaw {

TemporaryDirectory::TemporaryDirectory(std::string path)
    : path_(std::move(path))
{
}

TemporaryDirectory::~TemporaryDirectory()
{
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
}

std::unique_ptr<TemporaryDirectory> TemporaryDirectory::create(
    const std::string &parent, const std::string &prefix, std::string *error)
{
    if (error)
        error->clear();
    if (prefix.empty() || prefix == "." || prefix == ".." ||
        prefix.find('/') != std::string::npos) {
        if (error)
            *error = "Cannot create temporary directory: Invalid prefix.";
        return nullptr;
    }
    std::string path_template = parent + "/" + prefix + ".XXXXXX";
    std::vector<char> writable_path(path_template.begin(), path_template.end());
    writable_path.push_back('\0');
    char *created = ::mkdtemp(writable_path.data());
    if (!created) {
        if (error)
            *error = "Cannot create temporary directory: " +
                     std::string(std::strerror(errno));
        return nullptr;
    }
    return std::unique_ptr<TemporaryDirectory>(new TemporaryDirectory(created));
}

const std::string &TemporaryDirectory::path() const
{
    return path_;
}

}  // namespace zclaw
