#include "hal_lvgl_bsp.h"

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <regex>
#include <algorithm>
#include <cctype>
#include <limits.h>
#include <unistd.h>

static std::string get_app_root_path()
{
    char exe_path[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) {
        return "APPLaunch/";
    }

    exe_path[len] = '\0';
    std::string path(exe_path);
    size_t slash = path.find_last_of('/');
    std::string exe_dir = (slash == std::string::npos) ? "." : path.substr(0, slash);

    return exe_dir + "/APPLaunch/";
}

std::string cp0_file_path(std::string file)
{
    std::regex pattern(R"(\.(png|wav|ttf)$)", std::regex::icase);
    std::smatch m;

    std::string root_path = get_app_root_path();
    bool matched = std::regex_search(file, m, pattern);

    if (matched) {
        std::string ext = m[1].str();

        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) {
                           return std::tolower(c);
                       });

        if (ext == "png") {
            return "share/images/" + file;
        } else if (ext == "wav") {
            return root_path + "share/audio/" + file;
        } else if (ext == "ttf") {
            return root_path + "share/font/" + file;
        }
    }

    return ""; // 或者 return ""; 或者 throw，根据你的需求
}
