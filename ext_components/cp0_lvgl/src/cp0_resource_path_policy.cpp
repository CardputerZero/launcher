#include "cp0_resource_path_policy.hpp"

#include <algorithm>
#include <cctype>

namespace cp0::filesystem {
namespace {

bool starts_with(const std::string &value, const std::string &prefix)
{
    return value.compare(0, prefix.size(), prefix) == 0;
}

std::string lower_extension(const std::string &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash) ||
        dot + 1 == path.size())
        return {};
    std::string extension = path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension;
}

} // namespace

ResourceKind classify_resource(const std::string &path)
{
    const std::string extension = lower_extension(path);
    if (extension == "png" || extension == "gif" || extension == "jpg" ||
        extension == "jpeg" || extension == "svg")
        return ResourceKind::image;
    if (extension == "wav" || extension == "mp3" || extension == "ogg")
        return ResourceKind::audio;
    if (extension == "ttf" || extension == "otf") return ResourceKind::font;
    return ResourceKind::none;
}

bool has_lvgl_drive(const std::string &path)
{
    return path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) &&
           path[1] == ':';
}

bool has_parent_component(const std::string &path)
{
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = path.find_first_of("/\\", start);
        const std::size_t length = (end == std::string::npos ? path.size() : end) - start;
        if (length == 2 && path.compare(start, length, "..") == 0) return true;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

std::string strip_app_root_prefix(const std::string &path)
{
    static const std::string system_prefix = "/usr/share/APPLaunch/";
    static const std::string package_prefix = "APPLaunch/";
    if (starts_with(path, system_prefix)) return path.substr(system_prefix.size());
    if (starts_with(path, package_prefix)) return path.substr(package_prefix.size());
    return path;
}

} // namespace cp0::filesystem
