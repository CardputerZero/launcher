#include "hal_lvgl_bsp.h"
#include "cp0_lvgl_app.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

class SoundcardSystem
{
public:
    using callback_t = std::function<void(int, std::string)>;
    using arg_t = std::list<std::string>;

    void api_call(arg_t arg, callback_t callback)
    {
        const std::string cmd = arg.empty() ? "" : arg.front();
        if (cmd == "ListCards") {
            report(callback, 0, list_cards());
        } else if (cmd == "ListControls") {
            int card_idx = std::atoi(nth_arg(arg, 1).c_str());
            report(callback, 0, list_controls(card_idx));
        } else if (cmd == "GetControlDetail") {
            int card_idx = std::atoi(nth_arg(arg, 1).c_str());
            const std::string ctrl_name = nth_arg(arg, 2);
            report(callback, 0, get_control_detail(card_idx, ctrl_name));
        } else if (cmd == "SetControl") {
            int card_idx = std::atoi(nth_arg(arg, 1).c_str());
            const std::string ctrl_name = nth_arg(arg, 2);
            const std::string val_str = nth_arg(arg, 3);
            int ret = set_control(card_idx, ctrl_name, val_str);
            report(callback, ret, std::to_string(ret));
        } else {
            report(callback, -1, "unknown soundcard api command");
        }
    }

private:
    void report(callback_t callback, int code, const std::string &data)
    {
        if (callback)
            callback(code, data);
    }

    static std::string nth_arg(const arg_t &arg, size_t index)
    {
        auto it = arg.begin();
        std::advance(it, std::min(index, arg.size()));
        return it == arg.end() ? std::string() : *it;
    }

    static std::string capture_argv(std::initializer_list<const char *> args)
    {
        std::list<std::string> req;
        req.push_back("CaptureArgv");
        for (const char *a : args)
            if (a) req.push_back(a);

        std::string out;
        cp0_signal_process_api(req, [&](int, std::string data) {
            out = std::move(data);
        });
        return out;
    }

    static int run_argv(std::initializer_list<const char *> args)
    {
        std::list<std::string> req;
        req.push_back("RunArgv");
        req.push_back("0");
        for (const char *a : args)
            if (a) req.push_back(a);

        int rc = -1;
        cp0_signal_process_api(req, [&](int code, std::string) {
            rc = code;
        });
        return rc;
    }

    static std::string trim(const std::string &s)
    {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return {};
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    static bool parse_limits(const std::string &line, int &mn, int &mx)
    {
        size_t p = line.find("Limits:");
        if (p == std::string::npos) return false;
        std::string rest = line.substr(p + 7);
        for (const char *pfx : {"Playback ", "Capture "}) {
            if (rest.find(pfx) == 0) { rest = rest.substr(std::strlen(pfx)); break; }
        }
        int a = 0, b = 0;
        if (std::sscanf(rest.c_str(), " %d - %d", &a, &b) == 2) {
            mn = a; mx = b; return true;
        }
        return false;
    }

    static int parse_current_val(const std::string &line)
    {
        size_t p = line.find(": ");
        if (p == std::string::npos) return -1;
        int v = 0;
        if (std::sscanf(line.c_str() + p + 2, " %d", &v) == 1) return v;
        return -1;
    }

    static std::string extract_value_str(const std::string &line)
    {
        static const char *prefixes[] = {
            "Mono:", "Front Left:", "Front Right:", "Rear Left:", "Rear Right:",
            "Center:", "LFE:", "Side Left:", "Side Right:", "Capture:", "Playback:",
            "Item0:", nullptr
        };
        for (int i = 0; prefixes[i]; ++i) {
            size_t p = line.find(prefixes[i]);
            if (p != std::string::npos) return trim(line.substr(p));
        }
        return trim(line);
    }

    static bool is_value_line(const std::string &tl)
    {
        static const char *prefixes[] = {
            "Mono:", "Front Left:", "Front Right:", "Rear Left:", "Rear Right:",
            "Center:", "LFE:", "Side Left:", "Side Right:", "Capture:", "Playback:",
            "Item0:", nullptr
        };
        for (int i = 0; prefixes[i]; ++i)
            if (tl.rfind(prefixes[i], 0) == 0) return true;
        return false;
    }

    static std::string list_cards()
    {
        std::string raw = capture_argv({"cat", "/proc/asound/cards"});
        std::ostringstream oss;
        std::istringstream ss(raw);
        std::string line;
        while (std::getline(ss, line)) {
            int idx = -1;
            if (std::sscanf(line.c_str(), " %d [", &idx) != 1 || idx < 0) continue;

            const char *rb = std::strchr(line.c_str(), ']');
            std::string display_name;
            if (rb) {
                const char *colon = std::strchr(rb, ':');
                if (colon) {
                    const char *dash = std::strchr(colon, '-');
                    if (dash) display_name = trim(std::string(dash + 1));
                }
            }
            if (display_name.empty()) {
                const char *lb = std::strchr(line.c_str(), '[');
                if (lb && rb && rb > lb + 1)
                    display_name = trim(std::string(lb + 1, rb));
            }

            char buf[16];
            std::snprintf(buf, sizeof(buf), "Card %d", idx);
            std::string card_name = std::string(buf) + (display_name.empty() ? "" : ": " + display_name);
            oss << idx << '\t' << card_name << '\n';
        }
        return oss.str();
    }

    static std::string list_controls(int card_index)
    {
        char card_str[16];
        std::snprintf(card_str, sizeof(card_str), "%d", card_index);
        std::string raw = capture_argv({"amixer", "-c", card_str});

        std::ostringstream oss;
        if (raw.empty()) return oss.str();

        std::istringstream ss(raw);
        std::string line;

        struct CtrlAccum {
            std::string name;
            std::string type;
            int min_val = 0;
            int max_val = 0;
            int step = 1;
            std::string current_str;
            int current_val = 0;
            bool active = false;
        } cur;

        auto flush = [&]() {
            if (cur.active && !cur.name.empty()) {
                oss << cur.name << '\t' << cur.type << '\t'
                    << cur.min_val << '\t' << cur.max_val << '\t'
                    << cur.step << '\t' << cur.current_str << '\t'
                    << cur.current_val << '\n';
            }
            cur = CtrlAccum{};
        };

        while (std::getline(ss, line)) {
            std::string tl = trim(line);
            if (tl.rfind("Simple mixer control", 0) == 0) {
                flush();
                size_t q1 = tl.find('\'');
                size_t q2 = (q1 != std::string::npos) ? tl.find('\'', q1 + 1) : std::string::npos;
                if (q1 != std::string::npos && q2 != std::string::npos)
                    cur.name = tl.substr(q1 + 1, q2 - q1 - 1);
                cur.active = true;
                continue;
            }
            if (!cur.active) continue;

            if (tl.rfind("Capabilities:", 0) == 0) {
                cur.type = (tl.find("enum") != std::string::npos) ? "ENUMERATED" : "INTEGER";
            } else if (tl.rfind("Limits:", 0) == 0) {
                parse_limits(tl, cur.min_val, cur.max_val);
            } else if (cur.current_str.empty() && is_value_line(tl)) {
                cur.current_str = extract_value_str(tl);
                int v = parse_current_val(tl);
                if (v >= 0) cur.current_val = v;
            }
        }
        flush();
        return oss.str();
    }

    static std::string get_control_detail(int card_index, const std::string &ctrl_name)
    {
        char card_str[16];
        std::snprintf(card_str, sizeof(card_str), "%d", card_index);
        return capture_argv({"amixer", "-c", card_str, "sget", ctrl_name.c_str()});
    }

    static int set_control(int card_index, const std::string &ctrl_name, const std::string &val_str)
    {
        char card_str[16];
        std::snprintf(card_str, sizeof(card_str), "%d", card_index);
        return run_argv({"amixer", "-c", card_str, "sset", ctrl_name.c_str(), val_str.c_str()});
    }
};

} // namespace

extern "C" void init_soundcard(void)
{
    auto soundcard = std::make_shared<SoundcardSystem>();
    cp0_signal_soundcard_api.append([soundcard](std::list<std::string> arg, std::function<void(int, std::string)> callback) {
        soundcard->api_call(std::move(arg), std::move(callback));
    });
}
