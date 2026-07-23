#include "cp0_lvgl_app.h"
#include "hal/hal_paths.h"
#include "hal_lvgl_bsp.h"

#include "cp0_config_json.h"
#include "../cp0_config_service.hpp"
#include "../cp0_signal_registration.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

std::string join_path(const std::string &base, const char *leaf)
{
    std::string path = base.empty() ? "." : base;
    if (path.back() != '/')
        path.push_back('/');
    return path + leaf;
}

std::string config_dir()
{
    const char *configured = std::getenv("APPLAUNCH_SDL_CONFIG_DIR");
    if (configured && configured[0])
        return configured;
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0])
        return join_path(xdg, "applaunch-sdl");
    const char *home = std::getenv("HOME");
    if (home && home[0])
        return join_path(join_path(home, ".config"), "applaunch-sdl");
    return join_path(hal_path_data_dir() ? hal_path_data_dir() : ".", "sdl_config");
}

bool make_directories(const std::string &path)
{
    if (path.empty())
        return false;
    std::string partial;
    partial.reserve(path.size());
    for (char character : path) {
        partial.push_back(character);
        if (character == '/' && partial.size() > 1 &&
            mkdir(partial.c_str(), 0755) != 0 && errno != EEXIST)
            return false;
    }
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

void fsync_dir(const std::string &path)
{
#ifdef O_DIRECTORY
    const int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
#else
    const int fd = open(path.c_str(), O_RDONLY);
#endif
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
}

bool load_config(cp0cfg::Entries &entries)
{
    FILE *fp = std::fopen(join_path(config_dir(), "config.json").c_str(), "r");
    if (!fp)
        return false;
    std::string text;
    char buffer[512];
    size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), fp)) > 0)
        text.append(buffer, count);
    const bool read_ok = std::ferror(fp) == 0;
    std::fclose(fp);
    return read_ok && cp0cfg::from_json(text, entries);
}

int save_config(const cp0cfg::Entries &entries)
{
    const std::string dir = config_dir();
    if (!make_directories(dir))
        return -1;
    const std::string path = join_path(dir, "config.json");
    const std::string temporary = path + ".tmp";
    const std::string json = cp0cfg::to_json(entries);
    FILE *fp = std::fopen(temporary.c_str(), "w");
    if (!fp)
        return -1;
    const bool written = std::fwrite(json.data(), 1, json.size(), fp) == json.size();
    const bool flushed = written && std::fflush(fp) == 0 && fsync(fileno(fp)) == 0;
    const bool closed = std::fclose(fp) == 0;
    if (!flushed || !closed || std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        return -1;
    }
    fsync_dir(dir);
    return 0;
}

} // namespace

extern "C" void init_config(void)
{
    static cp0::SignalRegistration<decltype(cp0_signal_config_api)> registration;
    auto config = std::make_shared<cp0cfg::ConfigService>(load_config, save_config);
    registration.replace(cp0_signal_config_api, [config](std::list<std::string> arguments,
                                                         std::function<void(int, std::string)> callback) {
        config->api_call(std::move(arguments), std::move(callback));
    });
}
