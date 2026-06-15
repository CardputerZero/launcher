#include "cp0_lvgl_app.h"
#include "hal_lvgl_bsp.h"
#include "../cp0_app_internal_utils.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <ifaddrs.h>
#include <iterator>
#include <list>
#include <memory>
#include <net/if.h>
#include <pwd.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>

namespace {

static time_t g_sdl_time_offset = 0;

static time_t parse_timestamp(const char *timestamp, bool *ok)
{
    if (ok)
        *ok = false;
    if (!timestamp || !timestamp[0])
        return 0;

    std::tm tm{};
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(timestamp, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
        return 0;

    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = -1;

    time_t parsed = std::mktime(&tm);
    if (parsed == static_cast<time_t>(-1))
        return 0;
    if (ok)
        *ok = true;
    return parsed;
}

class SdlOsInfoSystem {
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    void api_call(arg_t arg, callback_t callback)
    {
        const std::string cmd = nth_arg(arg, 0);
        if (cmd == "NetworkDefaultInfoRead" || cmd == "EthInfoRead") {
            cp0_eth_info_t info{};
            int ret = network_default_info_read(&info);
            report(callback, ret, encode_eth_info(info));
        } else if (cmd == "AccountInfoRead") {
            cp0_account_info_t info{};
            int ret = account_info_read(&info);
            report(callback, ret, encode_account_info(info));
        } else if (cmd == "TimeSet") {
            report(callback, time_set(nth_arg(arg, 1).c_str()), "");
        } else if (cmd == "AptUpdateBackground") {
            report(callback, apt_update_background(), "");
        } else if (cmd == "UpdateLauncherBackground") {
            report(callback, update_launcher_background(), "");
        } else {
            report(callback, -1, "unknown osinfo api command");
        }
    }

    static int api_simple(const arg_t &arg, std::string *out = nullptr)
    {
        int result = -1;
        cp0_signal_osinfo_api(arg, [&](int code, std::string data) {
            result = code;
            if (out)
                *out = std::move(data);
        });
        return result;
    }

private:
    static void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    static std::string nth_arg(const arg_t &arg, size_t index)
    {
        auto it = arg.begin();
        for (size_t i = 0; i < index && it != arg.end(); ++i)
            ++it;
        return it == arg.end() ? std::string() : *it;
    }

    static void clear_net_info(cp0_eth_info_t *info)
    {
        if (!info)
            return;
        std::memset(info, 0, sizeof(*info));
        cp0_copy_cstr(info->ipv4, sizeof(info->ipv4), "N/A");
        cp0_copy_cstr(info->gateway, sizeof(info->gateway), "N/A");
        cp0_copy_cstr(info->mac, sizeof(info->mac), "N/A");
    }

    static std::string default_iface_from_route()
    {
        std::ifstream route("/proc/net/route");
        std::string line;
        std::getline(route, line);
        while (std::getline(route, line)) {
            std::istringstream iss(line);
            std::string iface;
            unsigned long dest = 0;
            if (iss >> iface >> std::hex >> dest && dest == 0)
                return iface;
        }
        return "";
    }

    static std::string default_gateway_from_route(const std::string &iface)
    {
        std::ifstream route("/proc/net/route");
        std::string line;
        std::getline(route, line);
        while (std::getline(route, line)) {
            std::istringstream iss(line);
            std::string row_iface;
            unsigned long dest = 0;
            unsigned long gateway = 0;
            if (!(iss >> row_iface >> std::hex >> dest >> gateway))
                continue;
            if (dest != 0 || (!iface.empty() && row_iface != iface))
                continue;
            struct in_addr addr;
            addr.s_addr = static_cast<in_addr_t>(gateway);
            char buf[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)))
                return buf;
        }
        return "N/A";
    }

    static std::string mac_for_iface(const std::string &iface)
    {
        if (iface.empty())
            return "N/A";
        std::ifstream file("/sys/class/net/" + iface + "/address");
        std::string mac;
        if (std::getline(file, mac) && !mac.empty())
            return mac;
        return "N/A";
    }

    static int network_default_info_read(cp0_eth_info_t *info)
    {
        clear_net_info(info);
        if (!info)
            return -1;

        std::string iface = default_iface_from_route();
        struct ifaddrs *ifap = nullptr;
        if (getifaddrs(&ifap) == 0) {
            for (struct ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                    continue;
                if (std::strcmp(ifa->ifa_name, "lo") == 0 || std::strcmp(ifa->ifa_name, "lo0") == 0)
                    continue;
                if (!iface.empty() && iface != ifa->ifa_name)
                    continue;
                iface = ifa->ifa_name;
                struct sockaddr_in *sa = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
                char ip[INET_ADDRSTRLEN] = {};
                if (inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip)))
                    cp0_copy_cstr(info->ipv4, sizeof(info->ipv4), ip);
                break;
            }
            freeifaddrs(ifap);
        }

        cp0_copy_string(info->gateway, sizeof(info->gateway), default_gateway_from_route(iface));
        cp0_copy_string(info->mac, sizeof(info->mac), mac_for_iface(iface));
        return 0;
    }

    static int account_info_read(cp0_account_info_t *info)
    {
        if (!info)
            return -1;
        std::memset(info, 0, sizeof(*info));
        const char *user = getlogin();
        if (!user || !user[0]) {
            struct passwd *pw = getpwuid(getuid());
            user = pw ? pw->pw_name : nullptr;
        }
        cp0_copy_cstr(info->user, sizeof(info->user), user && user[0] ? user : "N/A");
        char host[sizeof(info->hostname)] = {};
        if (gethostname(host, sizeof(host) - 1) == 0 && host[0])
            cp0_copy_cstr(info->hostname, sizeof(info->hostname), host);
        else
            cp0_copy_cstr(info->hostname, sizeof(info->hostname), "SDL");
        return 0;
    }

    static int time_set(const char *timestamp)
    {
        bool ok = false;
        const time_t target = parse_timestamp(timestamp, &ok);
        if (!ok)
            return -1;
        g_sdl_time_offset = target - std::time(nullptr);
        return 0;
    }

    static int apt_update_background()
    {
        return 0;
    }

    static int update_launcher_background()
    {
        return 0;
    }

    static std::string encode_eth_info(const cp0_eth_info_t &info)
    {
        return std::string(info.ipv4) + "\n" + info.gateway + "\n" + info.mac;
    }

    static void decode_eth_info(const std::string &data, cp0_eth_info_t *info)
    {
        clear_net_info(info);
        if (!info)
            return;
        std::istringstream lines(data);
        std::string line;
        if (std::getline(lines, line)) cp0_copy_string(info->ipv4, sizeof(info->ipv4), line);
        if (std::getline(lines, line)) cp0_copy_string(info->gateway, sizeof(info->gateway), line);
        if (std::getline(lines, line)) cp0_copy_string(info->mac, sizeof(info->mac), line);
    }

    static std::string encode_account_info(const cp0_account_info_t &info)
    {
        return std::string(info.user) + "\n" + info.hostname;
    }

    static void decode_account_info(const std::string &data, cp0_account_info_t *info)
    {
        if (!info)
            return;
        std::memset(info, 0, sizeof(*info));
        std::istringstream lines(data);
        std::string line;
        if (std::getline(lines, line)) cp0_copy_string(info->user, sizeof(info->user), line);
        if (std::getline(lines, line)) cp0_copy_string(info->hostname, sizeof(info->hostname), line);
    }

public:
    static int api_eth_info(const char *command, cp0_eth_info_t *info)
    {
        if (!info)
            return -1;
        std::string data;
        int ret = api_simple({command}, &data);
        if (ret == 0)
            decode_eth_info(data, info);
        return ret;
    }

    static int api_account_info(cp0_account_info_t *info)
    {
        if (!info)
            return -1;
        std::string data;
        int ret = api_simple({"AccountInfoRead"}, &data);
        if (ret == 0)
            decode_account_info(data, info);
        return ret;
    }
};

} // namespace

extern "C" time_t cp0_sdl_time_now(void)
{
    return std::time(nullptr) + g_sdl_time_offset;
}

extern "C" void init_osinfo(void)
{
    auto osinfo = std::make_shared<SdlOsInfoSystem>();
    cp0_signal_osinfo_api.append([osinfo](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        osinfo->api_call(std::move(arg), std::move(callback));
    });
}

extern "C" int cp0_network_default_info_read(cp0_eth_info_t *info)
{
    return SdlOsInfoSystem::api_eth_info("NetworkDefaultInfoRead", info);
}

extern "C" int cp0_eth_info_read(cp0_eth_info_t *info)
{
    return SdlOsInfoSystem::api_eth_info("EthInfoRead", info);
}

extern "C" int cp0_account_info_read(cp0_account_info_t *info)
{
    return SdlOsInfoSystem::api_account_info(info);
}

extern "C" int cp0_time_set(const char *timestamp)
{
    return SdlOsInfoSystem::api_simple({"TimeSet", timestamp ? timestamp : ""});
}

extern "C" int cp0_system_apt_update_background(void)
{
    return SdlOsInfoSystem::api_simple({"AptUpdateBackground"});
}

extern "C" int cp0_system_update_launcher_background(void)
{
    return SdlOsInfoSystem::api_simple({"UpdateLauncherBackground"});
}
