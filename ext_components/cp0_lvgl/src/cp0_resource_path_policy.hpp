#pragma once

#include <string>

namespace cp0::filesystem {

enum class ResourceKind {
    none,
    image,
    audio,
    font,
};

ResourceKind classify_resource(const std::string &path);
bool has_lvgl_drive(const std::string &path);
bool has_parent_component(const std::string &path);
std::string strip_app_root_prefix(const std::string &path);

} // namespace cp0::filesystem
