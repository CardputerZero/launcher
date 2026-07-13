#pragma once

#include "hal_lvgl_bsp.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace launcher_platform {

inline bool decode_field(const std::string &encoded, std::string &decoded)
{
    auto hex_value = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    decoded.clear();
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] != '%') { decoded.push_back(encoded[i]); continue; }
        if (i + 2 >= encoded.size()) return false;
        const int high = hex_value(encoded[i + 1]);
        const int low = hex_value(encoded[i + 2]);
        if (high < 0 || low < 0) return false;
        decoded.push_back(static_cast<char>((high << 4) | low));
        i += 2;
    }
    return true;
}

inline std::string path(std::string name)
{
    std::string resolved;
    cp0_signal_filesystem_api({"Path", std::move(name)}, [&](int code, std::string data) {
        if (code == 0) resolved = std::move(data);
    });
    return resolved;
}

inline const char *path_c(const std::string &name)
{
    static thread_local std::unordered_map<std::string, std::string> cache;
    auto it = cache.find(name);
    if (it == cache.end()) it = cache.emplace(name, path(name)).first;
    return it->second.c_str();
}

} // namespace launcher_platform
