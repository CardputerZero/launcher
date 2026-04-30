#pragma once
#include "ui_app_page.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#endif
#include "hal/hal_settings.h"
#include "hal/hal_network.h"
#include "compat/input_keys.h"

// ============================================================
//  HACK Tools  UIHackPage
//  Screen: 320 x 170  (top bar 20px, ui_APP_Container 320x150)
//
//  View states:
//    VIEW_MAIN       - Main menu with tool selection list
//    VIEW_SUB        - Sub-page for each tool
//    VIEW_INPUT      - Text input mode (Port Scanner IP / Ping host)
// ============================================================

class UIHackPage : public app_base
{
    enum class ViewState { MAIN, SUB, INPUT };

    struct MenuItem
    {
        const char *icon;
        const char *label;
        const char *sub_title;
        std::function<void(lv_obj_t *container)> build_sub;
        std::function<void(uint32_t key)>        on_sub_key;
    };

public:
    UIHackPage() : app_base()
    {
        menu_init();
        creat_UI();
        event_handler_init();
    }
    ~UIHackPage() {}

private:
    std::unordered_map<std::string, lv_obj_t *> ui_obj_;
    std::vector<MenuItem> menu_items_;
    int   selected_idx_  = 0;
    ViewState view_state_ = ViewState::MAIN;

    static constexpr int ITEM_H       = 28;
    static constexpr int VISIBLE_ROWS = 4;
    static constexpr int LIST_Y       = 22;
    static constexpr int LIST_H       = 128;

    // ---- WiFi Scanner state ----
    hal_wifi_ap_t wifi_aps_[WIFI_AP_MAX];
    int wifi_ap_count_ = 0;
    int wifi_sel_      = 0;
    lv_obj_t *wifi_list_cont_ = nullptr;

    // ---- Port Scanner state ----
    std::string port_ip_buf_;
    lv_obj_t *port_input_lbl_ = nullptr;
    lv_obj_t *port_result_cont_ = nullptr;
    lv_obj_t *port_hint_lbl_ = nullptr;
    bool port_scanning_ = false;

    // ---- Network Info state ----
    lv_obj_t *netinfo_cont_ = nullptr;

    // ---- Ping state ----
    std::string ping_host_buf_;
    lv_obj_t *ping_input_lbl_ = nullptr;
    lv_obj_t *ping_result_cont_ = nullptr;
    lv_obj_t *ping_hint_lbl_ = nullptr;
    bool ping_running_ = false;

    // ==================== helper: styled label ====================
    static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                                int x, int y, uint32_t color = 0xE6EDF3,
                                const lv_font_t *font = &lv_font_montserrat_12)
    {
        lv_obj_t *lbl = lv_label_create(parent);
        lv_label_set_text(lbl, text);
        lv_obj_set_pos(lbl, x, y);
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN | LV_STATE_DEFAULT);
        return lbl;
    }

    // ==================== keycode to char ====================
    static char keycode_to_char(uint32_t key)
    {
        if (key >= KEY_1 && key <= KEY_9) return '1' + (key - KEY_1);
        if (key == KEY_0) return '0';
        static const char qwerty[] = "qwertyuiop";
        if (key >= KEY_Q && key <= KEY_P) return qwerty[key - KEY_Q];
        static const char asdf[] = "asdfghjkl";
        if (key >= KEY_A && key <= KEY_L) return asdf[key - KEY_A];
        static const char zxcv[] = "zxcvbnm";
        if (key >= KEY_Z && key <= KEY_M) return zxcv[key - KEY_Z];
        if (key == KEY_SPACE) return ' ';
        if (key == 52) return '.'; // KEY_DOT
        return 0;
    }

    // ==================== menu data init ====================
    void menu_init()
    {
        menu_items_.push_back({
            LV_SYMBOL_WIFI,
            "WiFi Scanner",
            "WiFi Scanner",
            [this](lv_obj_t *c) { build_wifi_scan_page(c); },
            [this](uint32_t key) { handle_wifi_scan_key(key); }
        });

        menu_items_.push_back({
            LV_SYMBOL_CHARGE,
            "Port Scanner",
            "Port Scanner",
            [this](lv_obj_t *c) { build_port_scan_page(c); },
            [this](uint32_t key) { handle_port_scan_key(key); }
        });

        menu_items_.push_back({
            LV_SYMBOL_LIST,
            "Network Info",
            "Network Info",
            [this](lv_obj_t *c) { build_netinfo_page(c); },
            [this](uint32_t key) { handle_netinfo_key(key); }
        });

        menu_items_.push_back({
            LV_SYMBOL_LOOP,
            "Ping",
            "Ping Host",
            [this](lv_obj_t *c) { build_ping_page(c); },
            [this](uint32_t key) { handle_ping_key(key); }
        });
    }

    // ==================== WiFi Scanner sub-page ====================
    void build_wifi_scan_page(lv_obj_t *c)
    {
        make_label(c, "Scanning...", 0, 2, 0x888888);

        wifi_list_cont_ = lv_obj_create(c);
        lv_obj_set_size(wifi_list_cont_, 296, 80);
        lv_obj_set_pos(wifi_list_cont_, 0, 2);
        lv_obj_set_style_bg_opa(wifi_list_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(wifi_list_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(wifi_list_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(wifi_list_cont_, LV_OBJ_FLAG_SCROLLABLE);

        make_label(c, "UP/DN:select  R:refresh  ESC:back", 0, 102, 0x555555, &lv_font_montserrat_10);

        wifi_do_scan();
    }

    void wifi_do_scan()
    {
        wifi_ap_count_ = hal_wifi_scan(wifi_aps_, WIFI_AP_MAX);
        wifi_sel_ = 0;
        wifi_build_ap_rows();
    }

    void wifi_build_ap_rows()
    {
        if (!wifi_list_cont_) return;
        lv_obj_clean(wifi_list_cont_);

        if (wifi_ap_count_ == 0) {
            make_label(wifi_list_cont_, "No networks found", 4, 8, 0x888888);
            return;
        }

        int visible = 4;
        int offset = wifi_sel_ - visible / 2;
        if (offset < 0) offset = 0;
        if (offset > wifi_ap_count_ - visible) offset = wifi_ap_count_ - visible;
        if (offset < 0) offset = 0;

        for (int vi = 0; vi < visible && (vi + offset) < wifi_ap_count_; ++vi) {
            int ai = vi + offset;
            bool sel = (ai == wifi_sel_);
            hal_wifi_ap_t *ap = &wifi_aps_[ai];

            lv_obj_t *row = lv_obj_create(wifi_list_cont_);
            lv_obj_set_size(row, 294, 18);
            lv_obj_set_pos(row, 0, vi * 20);
            lv_obj_set_style_radius(row, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            uint32_t bg = sel ? 0x1F3A5F : 0x161B22;
            lv_obj_set_style_bg_color(row, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

            char txt[128];
            const char *lock = (strcmp(ap->security, "Open") == 0 || ap->security[0] == 0)
                                ? "OPEN" : ap->security;
            snprintf(txt, sizeof(txt), "%s  %d%%  %s",
                     ap->ssid, ap->signal, lock);

            uint32_t tc = sel ? 0xFFFFFF : 0xCCCCCC;
            if (ap->in_use) tc = 0x58A6FF;
            make_label(row, txt, 4, 1, tc, &lv_font_montserrat_10);
        }
    }

    void handle_wifi_scan_key(uint32_t key)
    {
        switch (key) {
        case KEY_UP:
            if (wifi_sel_ > 0) { --wifi_sel_; wifi_build_ap_rows(); }
            break;
        case KEY_DOWN:
            if (wifi_sel_ < wifi_ap_count_ - 1) { ++wifi_sel_; wifi_build_ap_rows(); }
            break;
        case KEY_R:
        case KEY_F5:
            wifi_do_scan();
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    // ==================== Port Scanner sub-page ====================
    void build_port_scan_page(lv_obj_t *c)
    {
        port_ip_buf_ = "127.0.0.1";
        port_scanning_ = false;

        make_label(c, "Target:", 0, 4, 0xE6EDF3);

        std::string display = port_ip_buf_ + "_";
        port_input_lbl_ = make_label(c, display.c_str(), 60, 4, 0xFFFFFF, &lv_font_montserrat_12);
        lv_obj_set_width(port_input_lbl_, 220);
        lv_label_set_long_mode(port_input_lbl_, LV_LABEL_LONG_CLIP);

        port_result_cont_ = lv_obj_create(c);
        lv_obj_set_size(port_result_cont_, 296, 72);
        lv_obj_set_pos(port_result_cont_, 0, 22);
        lv_obj_set_style_bg_opa(port_result_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(port_result_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(port_result_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_scroll_dir(port_result_cont_, LV_DIR_VER);
        lv_obj_add_flag(port_result_cont_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(port_result_cont_, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_width(port_result_cont_, 3, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(port_result_cont_, lv_color_hex(0x1F6FEB), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(port_result_cont_, 200, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

        make_label(port_result_cont_, "Press ENTER to scan", 4, 4, 0x555555);

        port_hint_lbl_ = make_label(c, "Type IP, ENTER:scan  ESC:back", 0, 98, 0x555555, &lv_font_montserrat_10);

        // Enable text input mode
        view_state_ = ViewState::INPUT;
    }

    void port_update_input()
    {
        if (!port_input_lbl_) return;
        std::string display = port_ip_buf_ + "_";
        lv_label_set_text(port_input_lbl_, display.c_str());
    }

    static const char *port_service_name(int port)
    {
        switch (port) {
        case 21:   return "FTP";
        case 22:   return "SSH";
        case 23:   return "Telnet";
        case 25:   return "SMTP";
        case 53:   return "DNS";
        case 80:   return "HTTP";
        case 443:  return "HTTPS";
        case 3306:  return "MySQL";
        case 5432:  return "PostgreSQL";
        case 8080:  return "HTTP-Alt";
        default:   return "Unknown";
        }
    }

    static bool try_connect_port(const char *ip, int port, int timeout_ms)
    {
#ifdef _WIN32
        (void)ip; (void)port; (void)timeout_ms;
        return false;
#else
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return false;

        // Set non-blocking
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &addr.sin_addr);

        int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
        if (ret == 0) {
            close(sock);
            return true;
        }

        if (errno != EINPROGRESS) {
            close(sock);
            return false;
        }

        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(sock, &wset);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        ret = select(sock + 1, NULL, &wset, NULL, &tv);
        if (ret > 0) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
            close(sock);
            return (err == 0);
        }

        close(sock);
        return false;
#endif
    }

    void do_port_scan()
    {
        if (!port_result_cont_) return;
        lv_obj_clean(port_result_cont_);

        if (port_ip_buf_.empty()) {
            make_label(port_result_cont_, "No target IP specified", 4, 4, 0xE74C3C);
            return;
        }

        make_label(port_result_cont_, "Scanning...", 4, 4, 0x7EA8D8);
        // Force LVGL to refresh so user sees "Scanning..."
        lv_refr_now(NULL);

        static const int ports[] = {22, 80, 443, 8080, 3306, 5432, 21, 23, 25, 53};
        static const int num_ports = sizeof(ports) / sizeof(ports[0]);

        lv_obj_clean(port_result_cont_);

        char header[64];
        snprintf(header, sizeof(header), "Results for %s:", port_ip_buf_.c_str());
        make_label(port_result_cont_, header, 4, 0, 0x58A6FF, &lv_font_montserrat_10);

        int y = 14;
        int open_count = 0;
        for (int i = 0; i < num_ports; ++i) {
            bool open = try_connect_port(port_ip_buf_.c_str(), ports[i], 200);
            char line[96];
            snprintf(line, sizeof(line), "%-5d  %-12s  %s",
                     ports[i], port_service_name(ports[i]),
                     open ? "OPEN" : "CLOSED");

            uint32_t color = open ? 0x2ECC71 : 0x555555;
            make_label(port_result_cont_, line, 4, y, color, &lv_font_montserrat_10);
            y += 13;
            if (open) open_count++;
        }

        char summary[64];
        snprintf(summary, sizeof(summary), "%d open / %d scanned", open_count, num_ports);
        make_label(port_result_cont_, summary, 4, y + 4, 0x7EA8D8, &lv_font_montserrat_10);
    }

    void handle_port_scan_key(uint32_t key)
    {
        if (view_state_ == ViewState::INPUT) {
            if (key == KEY_ESC) {
                view_state_ = ViewState::SUB;
                close_sub_page();
                return;
            }
            if (key == KEY_ENTER) {
                do_port_scan();
                return;
            }
            if (key == KEY_BACKSPACE) {
                if (!port_ip_buf_.empty())
                    port_ip_buf_.pop_back();
                port_update_input();
                return;
            }
            char ch = keycode_to_char(key);
            if (ch) {
                port_ip_buf_ += ch;
                port_update_input();
            }
            return;
        }

        // SUB state fallback (after scan completes, still in INPUT)
        switch (key) {
        case KEY_UP:
            if (port_result_cont_)
                lv_obj_scroll_by(port_result_cont_, 0, 20, LV_ANIM_ON);
            break;
        case KEY_DOWN:
            if (port_result_cont_)
                lv_obj_scroll_by(port_result_cont_, 0, -20, LV_ANIM_ON);
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    // ==================== Network Info sub-page ====================
    void build_netinfo_page(lv_obj_t *c)
    {
        // Hostname
        char hostname[128] = "unknown";
        gethostname(hostname, sizeof(hostname));
        char host_buf[160];
        snprintf(host_buf, sizeof(host_buf), "Hostname: %s", hostname);
        make_label(c, host_buf, 0, 2, 0x58A6FF);

        // Network interfaces
        hal_netif_info_t entries[16];
        int count = 0;
        hal_network_list(entries, 16, &count);

        if (count == 0) {
            make_label(c, "No network interfaces found", 0, 22, 0x555555);
        } else {
            int y = 20;
            for (int i = 0; i < count && y < 100; ++i) {
                char line[128];
                snprintf(line, sizeof(line), "%-8s  %s/%s  %s",
                         entries[i].iface,
                         entries[i].ipv4,
                         entries[i].netmask,
                         entries[i].is_up ? "UP" : "DOWN");

                uint32_t color = entries[i].is_up ? 0xE6EDF3 : 0x555555;
                make_label(c, line, 0, y, color, &lv_font_montserrat_10);
                y += 14;
            }
        }

        // WiFi status
        hal_wifi_status_t ws = hal_wifi_get_status();
        char wifi_buf[128];
        if (ws.connected)
            snprintf(wifi_buf, sizeof(wifi_buf), "WiFi: %s  IP:%s  %d%%",
                     ws.ssid, ws.ip, ws.signal);
        else
            snprintf(wifi_buf, sizeof(wifi_buf), "WiFi: Disconnected");
        make_label(c, wifi_buf, 0, 102, ws.connected ? 0x2ECC71 : 0x888888, &lv_font_montserrat_10);
    }

    void handle_netinfo_key(uint32_t key)
    {
        lv_obj_t *content = ui_obj_.count("sub_content") ? ui_obj_["sub_content"] : nullptr;
        switch (key) {
        case KEY_ENTER:
        case KEY_R:
            // Refresh
            if (content) {
                lv_obj_clean(content);
                build_netinfo_page(content);
            }
            break;
        case KEY_UP:
            if (content)
                lv_obj_scroll_by(content, 0, 20, LV_ANIM_ON);
            break;
        case KEY_DOWN:
            if (content)
                lv_obj_scroll_by(content, 0, -20, LV_ANIM_ON);
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    // ==================== Ping sub-page ====================
    void build_ping_page(lv_obj_t *c)
    {
        ping_host_buf_ = "127.0.0.1";
        ping_running_ = false;

        make_label(c, "Host:", 0, 4, 0xE6EDF3);

        std::string display = ping_host_buf_ + "_";
        ping_input_lbl_ = make_label(c, display.c_str(), 46, 4, 0xFFFFFF, &lv_font_montserrat_12);
        lv_obj_set_width(ping_input_lbl_, 240);
        lv_label_set_long_mode(ping_input_lbl_, LV_LABEL_LONG_CLIP);

        ping_result_cont_ = lv_obj_create(c);
        lv_obj_set_size(ping_result_cont_, 296, 72);
        lv_obj_set_pos(ping_result_cont_, 0, 22);
        lv_obj_set_style_bg_opa(ping_result_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ping_result_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(ping_result_cont_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_scroll_dir(ping_result_cont_, LV_DIR_VER);
        lv_obj_add_flag(ping_result_cont_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(ping_result_cont_, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_width(ping_result_cont_, 3, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ping_result_cont_, lv_color_hex(0x1F6FEB), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ping_result_cont_, 200, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

        make_label(ping_result_cont_, "Press ENTER to ping", 4, 4, 0x555555);

        ping_hint_lbl_ = make_label(c, "Type host, ENTER:ping  ESC:back", 0, 98, 0x555555, &lv_font_montserrat_10);

        view_state_ = ViewState::INPUT;
    }

    void ping_update_input()
    {
        if (!ping_input_lbl_) return;
        std::string display = ping_host_buf_ + "_";
        lv_label_set_text(ping_input_lbl_, display.c_str());
    }

    void do_ping()
    {
        if (!ping_result_cont_) return;
        lv_obj_clean(ping_result_cont_);

        if (ping_host_buf_.empty()) {
            make_label(ping_result_cont_, "No host specified", 4, 4, 0xE74C3C);
            return;
        }

        make_label(ping_result_cont_, "Pinging...", 4, 4, 0x7EA8D8);
        lv_refr_now(NULL);

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "ping -c 4 -W 2 %s 2>&1", ping_host_buf_.c_str());

        FILE *fp = popen(cmd, "r");
        if (!fp) {
            lv_obj_clean(ping_result_cont_);
            make_label(ping_result_cont_, "Failed to run ping", 4, 4, 0xE74C3C);
            return;
        }

        std::vector<std::string> lines;
        char line_buf[256];
        while (fgets(line_buf, sizeof(line_buf), fp)) {
            // Strip trailing newline
            size_t len = strlen(line_buf);
            while (len > 0 && (line_buf[len-1] == '\n' || line_buf[len-1] == '\r'))
                line_buf[--len] = '\0';
            if (len > 0)
                lines.push_back(line_buf);
        }
        pclose(fp);

        lv_obj_clean(ping_result_cont_);

        if (lines.empty()) {
            make_label(ping_result_cont_, "No response", 4, 4, 0xE74C3C);
            return;
        }

        int y = 0;
        for (size_t i = 0; i < lines.size() && i < 12; ++i) {
            // Truncate long lines for display
            std::string display_line = lines[i];
            if (display_line.size() > 48)
                display_line = display_line.substr(0, 48) + "...";

            uint32_t color = 0xE6EDF3;
            if (display_line.find("time=") != std::string::npos)
                color = 0x2ECC71;
            else if (display_line.find("Unreachable") != std::string::npos ||
                     display_line.find("timeout") != std::string::npos ||
                     display_line.find("100%") != std::string::npos)
                color = 0xE74C3C;
            else if (display_line.find("statistics") != std::string::npos ||
                     display_line.find("rtt") != std::string::npos)
                color = 0x58A6FF;

            make_label(ping_result_cont_, display_line.c_str(), 4, y, color, &lv_font_montserrat_10);
            y += 13;
        }
    }

    void handle_ping_key(uint32_t key)
    {
        if (view_state_ == ViewState::INPUT) {
            if (key == KEY_ESC) {
                view_state_ = ViewState::SUB;
                close_sub_page();
                return;
            }
            if (key == KEY_ENTER) {
                do_ping();
                return;
            }
            if (key == KEY_BACKSPACE) {
                if (!ping_host_buf_.empty())
                    ping_host_buf_.pop_back();
                ping_update_input();
                return;
            }
            char ch = keycode_to_char(key);
            if (ch) {
                ping_host_buf_ += ch;
                ping_update_input();
            }
            return;
        }

        switch (key) {
        case KEY_UP:
            if (ping_result_cont_)
                lv_obj_scroll_by(ping_result_cont_, 0, 20, LV_ANIM_ON);
            break;
        case KEY_DOWN:
            if (ping_result_cont_)
                lv_obj_scroll_by(ping_result_cont_, 0, -20, LV_ANIM_ON);
            break;
        case KEY_ESC:
            close_sub_page();
            break;
        default:
            break;
        }
    }

    // ==================== UI build (main menu) ====================
    void creat_UI()
    {
        lv_obj_t *bg = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(bg, 320, 150);
        lv_obj_set_pos(bg, 0, 0);
        lv_obj_set_style_radius(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(bg, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(bg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["bg"] = bg;

        lv_obj_t *title_bar = lv_obj_create(bg);
        lv_obj_set_size(title_bar, 320, 22);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title_bar, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl_title = lv_label_create(title_bar);
        lv_label_set_text(lbl_title, LV_SYMBOL_WARNING "  HACK Tools");
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, "UP/DN:select  ENTER:open  ESC:back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -4);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x7EA8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *list_cont = lv_obj_create(bg);
        lv_obj_set_size(list_cont, 320, LIST_H);
        lv_obj_set_pos(list_cont, 0, LIST_Y);
        lv_obj_set_style_radius(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(list_cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(list_cont, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["list_cont"] = list_cont;

        build_menu_rows();
    }

    // ==================== build menu rows ====================
    void build_menu_rows()
    {
        lv_obj_t *list_cont = ui_obj_["list_cont"];
        lv_obj_clean(list_cont);

        int item_count = (int)menu_items_.size();
        int visible = LIST_H / ITEM_H;
        int offset_idx = selected_idx_ - visible / 2;
        if (offset_idx < 0) offset_idx = 0;
        if (offset_idx > item_count - visible) offset_idx = item_count - visible;
        if (offset_idx < 0) offset_idx = 0;

        for (int vi = 0; vi < visible && (vi + offset_idx) < item_count; ++vi) {
            int mi = vi + offset_idx;
            bool is_sel = (mi == selected_idx_);
            create_menu_row(list_cont, vi, mi, is_sel);
        }
    }

    void create_menu_row(lv_obj_t *parent, int visual_row, int menu_idx, bool selected)
    {
        const MenuItem &item = menu_items_[menu_idx];

        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_size(row, 318, ITEM_H - 2);
        lv_obj_set_pos(row, 1, visual_row * ITEM_H + 1);
        lv_obj_set_style_radius(row, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        if (selected) {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x1F3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_t *sel_bar = lv_obj_create(row);
            lv_obj_set_size(sel_bar, 3, ITEM_H - 6);
            lv_obj_set_pos(sel_bar, 2, 2);
            lv_obj_set_style_radius(sel_bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(sel_bar, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(sel_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(sel_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(sel_bar, LV_OBJ_FLAG_SCROLLABLE);
        } else {
            lv_obj_set_style_bg_color(row, lv_color_hex(0x161B22), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        if (menu_idx < (int)menu_items_.size() - 1) {
            lv_obj_t *div = lv_obj_create(parent);
            lv_obj_set_size(div, 310, 1);
            lv_obj_set_pos(div, 5, (visual_row + 1) * ITEM_H);
            lv_obj_set_style_radius(div, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(div, lv_color_hex(0x21262D), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(div, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(div, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
        }

        lv_obj_t *lbl_icon = lv_label_create(row);
        lv_label_set_text(lbl_icon, item.icon);
        lv_obj_set_pos(lbl_icon, 8, (ITEM_H - 16) / 2 - 1);
        lv_obj_set_style_text_color(lbl_icon,
            selected ? lv_color_hex(0x58A6FF) : lv_color_hex(0x4A7ABF),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_name = lv_label_create(row);
        lv_label_set_text(lbl_name, item.label);
        lv_obj_set_pos(lbl_name, 30, (ITEM_H - 16) / 2 - 1);
        lv_obj_set_width(lbl_name, 240);
        lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl_name,
            selected ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xCCCCCC),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_arrow = lv_label_create(row);
        lv_label_set_text(lbl_arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_pos(lbl_arrow, 298, (ITEM_H - 14) / 2 - 1);
        lv_obj_set_style_text_color(lbl_arrow,
            selected ? lv_color_hex(0x58A6FF) : lv_color_hex(0x3A4A5A),
            LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_arrow, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // ==================== open sub-page ====================
    void open_sub_page(int idx)
    {
        if (idx < 0 || idx >= (int)menu_items_.size()) return;
        view_state_ = ViewState::SUB;

        const MenuItem &item = menu_items_[idx];

        lv_obj_t *panel = lv_obj_create(ui_APP_Container);
        lv_obj_set_size(panel, 320, 150);
        lv_obj_set_pos(panel, 0, 0);
        lv_obj_set_style_radius(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x0D1117), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        ui_obj_["sub_panel"] = panel;

        lv_obj_t *title_bar = lv_obj_create(panel);
        lv_obj_set_size(title_bar, 320, 22);
        lv_obj_set_pos(title_bar, 0, 0);
        lv_obj_set_style_radius(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1F6FEB), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(title_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(title_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(title_bar, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

        char title_buf[64];
        snprintf(title_buf, sizeof(title_buf), "%s  %s", item.icon, item.sub_title);
        lv_obj_t *lbl_title = lv_label_create(title_bar);
        lv_label_set_text(lbl_title, title_buf);
        lv_obj_set_align(lbl_title, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *lbl_hint = lv_label_create(title_bar);
        lv_label_set_text(lbl_hint, LV_SYMBOL_LEFT "  ESC: Back");
        lv_obj_set_align(lbl_hint, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lbl_hint, -6);
        lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0xAECBFA), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *content = lv_obj_create(panel);
        lv_obj_set_size(content, 316, 124);
        lv_obj_set_pos(content, 2, 24);
        lv_obj_set_style_radius(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(content, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_hor(content, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_ver(content, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_scroll_dir(content, LV_DIR_VER);
        lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_width(content, 3, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(content, lv_color_hex(0x1F6FEB), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(content, 200, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(content, 2, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        ui_obj_["sub_content"] = content;

        if (item.build_sub)
            item.build_sub(content);
    }

    // ==================== close sub-page ====================
    void close_sub_page()
    {
        wifi_list_cont_     = nullptr;
        port_input_lbl_     = nullptr;
        port_result_cont_   = nullptr;
        port_hint_lbl_      = nullptr;
        netinfo_cont_       = nullptr;
        ping_input_lbl_     = nullptr;
        ping_result_cont_   = nullptr;
        ping_hint_lbl_      = nullptr;

        if (ui_obj_.count("sub_panel") && ui_obj_["sub_panel"]) {
            lv_obj_del(ui_obj_["sub_panel"]);
            ui_obj_["sub_panel"]   = nullptr;
            ui_obj_["sub_content"] = nullptr;
        }
        view_state_ = ViewState::MAIN;
    }

    // ==================== event binding ====================
    void event_handler_init()
    {
        lv_obj_add_event_cb(ui_root, UIHackPage::static_lvgl_handler, LV_EVENT_ALL, this);
    }
    static void static_lvgl_handler(lv_event_t *e)
    {
        UIHackPage *self = static_cast<UIHackPage *>(lv_event_get_user_data(e));
        if (self) self->event_handler(e);
    }
    void event_handler(lv_event_t *e)
    {
        if (IS_KEY_RELEASED(e))
        {
            uint32_t key = LV_EVENT_KEYBOARD_GET_KEY(e);
            switch (view_state_)
            {
            case ViewState::MAIN:  handle_main_key(key); break;
            case ViewState::SUB:   handle_sub_key(key);  break;
            case ViewState::INPUT: handle_sub_key(key);  break;
            }
        }
    }

    // ==================== main menu keys ====================
    void handle_main_key(uint32_t key)
    {
        int count = (int)menu_items_.size();
        switch (key)
        {
        case KEY_UP:
            if (selected_idx_ > 0) {
                --selected_idx_;
                build_menu_rows();
            }
            break;

        case KEY_DOWN:
            if (selected_idx_ < count - 1) {
                ++selected_idx_;
                build_menu_rows();
            }
            break;

        case KEY_ENTER:
        case KEY_RIGHT:
            open_sub_page(selected_idx_);
            break;

        case KEY_ESC:
            if (go_back_home) go_back_home();
            break;

        default:
            break;
        }
    }

    // ==================== sub-page keys ====================
    void handle_sub_key(uint32_t key)
    {
        const MenuItem &item = menu_items_[selected_idx_];
        if (item.on_sub_key) {
            item.on_sub_key(key);
            return;
        }

        lv_obj_t *content = ui_obj_.count("sub_content") ? ui_obj_["sub_content"] : nullptr;
        switch (key)
        {
        case KEY_UP:
            if (content)
                lv_obj_scroll_by(content, 0, 20, LV_ANIM_ON);
            break;

        case KEY_DOWN:
            if (content)
                lv_obj_scroll_by(content, 0, -20, LV_ANIM_ON);
            break;

        case KEY_ESC:
            close_sub_page();
            break;

        default:
            break;
        }
    }
};
