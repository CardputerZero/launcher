#include "hal_lvgl_bsp.h"

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <regex>

std::string cp0_file_path(std::string file)
{
    std::regex pattern(R"(\.(png|wav|ttf)$)", std::regex::icase);

    std::string root_path;
    std::smatch m;

    // std::string root_path = "/usr/share/APPLaunch/";
    bool matched = std::regex_search(file, m, pattern);

    if (matched) {
        std::string ext = m[1].str();

        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) {
                           return std::tolower(c);
                       });

        if (ext == "png") {
            return root_path + "share/images/" + file;
        } else if (ext == "wav") {
            return root_path + "share/audio/" + file;
        } else if (ext == "ttf") {
            return root_path + "share/font/" + file;
        }
    }

    return ""; // 或者 return ""; 或者 throw，根据你的需求
}