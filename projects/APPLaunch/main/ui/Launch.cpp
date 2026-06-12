/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "Launch.h"

#include "ui.h"
#include "UILaunchPage.h"
#include "ui_loading.h"
#include "page_app.h"
#include "cp0_lvgl_app.h"
#include "cp0_lvgl_file.hpp"
#include "sample_log.h"

#include <dirent.h>
#include <fstream>
#include <functional>
#include <memory>
#include <stdio.h>
#include <string>
#include <string.h>
#include <utility>

namespace {
constexpr size_t kHomeCarouselSlotCount = 5;
constexpr int kHomeCarouselCenterSlot = 2;
}

// ============================================================
// Launch shortcut examples
// ============================================================
/*
root@pi:/home/pi# cat /usr/share/APPLaunch/applications/vim.desktop
[Desktop Entry]
Name=Vim
TryExec=vim
Exec=vim
Terminal=true
Icon=share/images/e-Mail_80.png
*/

// ============================================================
// Launch
// ============================================================
void Launch::bind_ui()
{
        if (bound_) {
            refresh_home_carousel();
            return;
        }
        bound_ = true;

        // Fixed icon; users cannot modify it
        app_list.emplace_back("Python",
                              cp0_file_path("python_100.png"), "python3", true, false);
        app_list.emplace_back("STORE",
                              cp0_file_path("store_100.png"),
                              "/usr/share/APPLaunch/bin/M5CardputerZero-AppStore", false, true, true);
        app_list.emplace_back("CLI",
                              cp0_file_path("cli_100.png"), "bash", true, false);
        app_list.emplace_back("GAME",
                              cp0_file_path("game_100.png"), page_v<UIGamePage>);

        app_list.emplace_back("SETTING",
                              cp0_file_path("setting_100.png"), page_v<UISetupPage>);


        // Dynamic icons filtered by Settings configuration
        #define APP_ENABLED(key) (cp0_config_get_int("app_" key, 1) != 0)

        if (APP_ENABLED("Music"))
        app_list.emplace_back("MUSIC",
                              cp0_file_path("music_100.png"), page_v<UIMusicPage>);

        if (APP_ENABLED("Math"))
        app_list.emplace_back("MATH",
                              cp0_file_path("math_100.png"),
                              "/usr/share/APPLaunch/bin/M5CardputerZero-Calculator", false);

        app_list.emplace_back("Compass",
                              cp0_file_path("compass_needle_80.png"), page_v<UICompassPage>);


#if defined(__linux__) && !defined(HAL_PLATFORM_SDL)
        if (APP_ENABLED("IP_Panel"))
        app_list.emplace_back("IP_PANEL",
                              cp0_file_path("ip_panel_100.png"), page_v<UIIpPanelPage>);
        if (APP_ENABLED("File"))
        app_list.emplace_back("FILE",
                              cp0_file_path("file_100.png"), page_v<UIFilePage>);
        if (APP_ENABLED("SSH"))
        app_list.emplace_back("SSH",
                              cp0_file_path("ssh_100.png"), page_v<UISSHPage>);
        if (APP_ENABLED("Mesh"))
        app_list.emplace_back("MESH",
                              cp0_file_path("mesh_100.png"), page_v<UIMeshPage>);
        if (APP_ENABLED("Rec"))
        app_list.emplace_back("REC",
                              cp0_file_path("rec_100.png"), page_v<UIRecPage>);
        if (APP_ENABLED("Camera"))
        app_list.emplace_back("CAMERA",
                              cp0_file_path("camera_100.png"), page_v<UICameraPage>);
        if (APP_ENABLED("LoRa"))
        app_list.emplace_back("LORA", cp0_file_path("lora_100.png"), page_v<UILoraPage>);
        if (APP_ENABLED("Tank"))
        app_list.emplace_back("TANK", cp0_file_path("tank_100.png"), page_v<UITankBattlePage>);
#endif
        #undef APP_ENABLED

        fixed_count = app_list.size();

        applications_load();
        refresh_home_carousel();

        // Initialize inotify and watch the applications directory
        inotify_init_watch();

        // Create a 3s LVGL timer to periodically check directory changes
        watch_timer = lv_timer_create(app_dir_watch_cb, 3000, this);

    }

void Launch::launch_app()
    {
        const app *selected = app_at_index(current_app);
        if (selected)
            selected->launch(this);
    }

void Launch::lv_go_back_home(void *arg)
    {
        auto self = (Launch *)arg;
        SLOGI("[HOME] lv_go_back_home executing (page=%p)", self->app_Page.get());
        lv_timer_enable(true);
        if (self->launch_page_)
            self->launch_page_->show_home_screen();
        lv_refr_now(NULL);
        if (self->app_Page)
            self->app_Page.reset();
        SLOGI("[HOME] lv_go_back_home done, on launcher home");
    }

void Launch::go_back_home()
    {
        SLOGI("[HOME] go_back_home() requested, scheduling async call (page=%p)", app_Page.get());
        lv_async_call(lv_go_back_home, this);
    }

    // Changed to accept std::string and no longer depend on app::Exec
void Launch::launch_Exec_in_terminal(const std::string &exec, bool sysplause)
    {
        SLOGI("Launching terminal app: %s", exec.c_str());
        /* Instant visual feedback; paint before the (potentially slow)
         * Console page construction so the user sees it right away. */
        ui_loading::show("Loading...");
        lv_refr_now(NULL);
        auto p = std::make_shared<UIConsolePage>();
        app_Page = p;
        lv_disp_load_scr(p->screen());
        lv_indev_set_group(lv_indev_get_next(NULL), p->input_group());
        p->navigate_home = std::bind(&Launch::go_back_home, this);
        p->terminal_sysplause = sysplause;
        /* Console page fully covers APP_Container; safe to hide now.
         * The heavy exec() call below will still run while the terminal
         * page is on-screen — no overlay needed at that point. */
        ui_loading::hide();
        p->exec(exec);
    }

void Launch::launch_Exec(const std::string &exec, bool keep_root)
    {
        SLOGI("Launching external app: %s (keep_root=%d)", exec.c_str(), keep_root);
        /* Show overlay BEFORE we tear down LVGL input/timers so the user
         * gets immediate feedback when ENTER was pressed. The overlay
         * stays drawn on the framebuffer right up until the child takes
         * it over via cp0_process_exec_blocking(). */
        ui_loading::show("Loading...");
        lv_disp_t *disp = lv_disp_get_default();
        lv_indev_t *indev = lv_indev_get_next(NULL);
        LVGL_RUN_FLAGE = 0;
        if (indev)
            lv_indev_set_group(indev, NULL);
        lv_timer_enable(false);
        lv_refr_now(disp);

        int ret = cp0_process_exec_blocking(exec.c_str(), &LVGL_HOME_KEY_FLAG, keep_root ? 1 : 0);
        SLOGI("App %s exited with code %d", exec.c_str(), ret);
        lv_timer_enable(true);
        if (indev)
            lv_indev_set_group(indev, UILaunchPage::home_input_group());
        if (launch_page_)
            launch_page_->show_home_screen();
        /* Child process has returned; we are back on the launcher home.
         * Hide the overlay so it doesn't linger. */
        ui_loading::hide();
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(disp);
        LVGL_RUN_FLAGE = 1;
    }

void Launch::select_next_app()
    {
        int next = normalized_app_index(current_app + 1);
        if (next >= 0)
            current_app = next;
    }

void Launch::select_previous_app()
    {
        int previous = normalized_app_index(current_app - 1);
        if (previous >= 0)
            current_app = previous;
    }

const app *Launch::carousel_slot_app(size_t slot) const
{
    if (slot >= kHomeCarouselSlotCount)
        return nullptr;
    return app_at_index(current_app + static_cast<int>(slot) - kHomeCarouselCenterSlot);
}

void Launch::applications_load()
    {
        const std::string app_dir_path = cp0_file_path("applications");
        const char *app_dir = app_dir_path.c_str();
        DIR *dir = opendir(app_dir);
        if (!dir)
        {
            perror("applications_load: opendir failed");
            return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            // Process only *.desktop files
            const char *name = entry->d_name;
            size_t len = strlen(name);
            if (len <= 8 || strcmp(name + len - 8, ".desktop") != 0)
                continue;

            std::string filepath = std::string(app_dir) + "/" + name;
            std::ifstream ifs(filepath);
            if (!ifs.is_open())
            {
                fprintf(stderr, "applications_load: cannot open %s\n", filepath.c_str());
                continue;
            }

            // Parse the INI file
            std::string page_title, app_icon, app_exec;
            bool app_terminal = false;
            bool app_sysplause = true;
            bool in_desktop_entry = false;

            std::string line;
            while (std::getline(ifs, line))
            {
                // Remove trailing \r (Windows newline)
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                // Skip empty lines and comments
                if (line.empty() || line[0] == '#' || line[0] == ';')
                    continue;

                // Detect section headers
                if (line[0] == '[')
                {
                    in_desktop_entry = (line == "[Desktop Entry]");
                    continue;
                }

                if (!in_desktop_entry)
                    continue;

                // Parse key=value
                auto eq = line.find('=');
                if (eq == std::string::npos)
                    continue;

                std::string key = line.substr(0, eq);
                std::string value = line.substr(eq + 1);

                // Trim leading/trailing spaces from the key
                auto ltrim = [](std::string &s)
                {
                    size_t i = 0;
                    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
                        ++i;
                    s = s.substr(i);
                };
                auto rtrim = [](std::string &s)
                {
                    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
                        s.pop_back();
                };
                ltrim(key);
                rtrim(key);
                ltrim(value);
                rtrim(value);

                if (key == "Name")
                    page_title = value;
                else if (key == "Icon")
                    app_icon = value;
                else if (key == "Exec")
                    app_exec = value;
                else if (key == "Terminal")
                    app_terminal = (value == "true" || value == "True" || value == "1");
                else if (key == "Sysplause")
                    app_sysplause = (value == "true" || value == "True" || value == "1");
            }

            // Name and Exec are required for registration
            if (page_title.empty() || app_exec.empty())
            {
                fprintf(stderr, "applications_load: skip %s (missing Name or Exec)\n", filepath.c_str());
                continue;
            }
            bool in_list = false;
            for (const auto &it : app_list)
            {
                if (it.Exec == app_exec)
                {
                    in_list = true;
                    break;
                }
            }
            if (in_list)
            {
                fprintf(stderr, "applications_load: skip %s (duplicate Exec)\n", filepath.c_str());
                continue;
            }

            app_list.emplace_back(page_title, app_icon, app_exec, app_terminal, app_sysplause);
        }

        closedir(dir);
    }

    // ============================================================
    // Initialize inotify in non-blocking mode and watch the applications directory
    // ============================================================
void Launch::inotify_init_watch()
    {
        const std::string app_dir_path = cp0_file_path("applications");
        dir_watcher = cp0_dir_watch_start(app_dir_path.c_str());
    }

    // ============================================================
    // Refresh home carousel slots from current_app
    // ============================================================
void Launch::refresh_home_carousel()
    {
        int normalized = normalized_app_index(current_app);
        if (normalized < 0)
            return;
        current_app = normalized;
        if (launch_page_)
            launch_page_->refresh_carousel();
    }

    // ============================================================
    // Reload the dynamic app list (keep fixed entries and rescan applications directory)
    // ============================================================
void Launch::applications_reload()
    {
        int sz = (int)app_list.size();
        if (sz > fixed_count)
        {
            auto it = std::next(app_list.begin(), fixed_count);
            app_list.erase(it, app_list.end());
        }
        applications_load();
        refresh_home_carousel();
    }

int Launch::normalized_app_index(int index) const
{
    int size = static_cast<int>(app_list.size());
    if (size == 0)
        return -1;

    index %= size;
    return index < 0 ? index + size : index;
}

const app *Launch::app_at_index(int index) const
{
    int normalized = normalized_app_index(index);
    if (normalized < 0)
        return nullptr;

    return &*std::next(app_list.begin(), normalized);
}

    // ============================================================
    // LVGL timer callback: check inotify events and refresh the list on changes
    // ============================================================
void Launch::app_dir_watch_cb(lv_timer_t *timer)
    {
        auto *self = static_cast<Launch *>(lv_timer_get_user_data(timer));
        if (!self || !self->dir_watcher)
            return;

        if (cp0_dir_watch_poll(self->dir_watcher) > 0)
        {
            SLOGI("app_dir_watch_cb: applications dir changed, reloading...");
            self->applications_reload();
        }
    }


// ============================================================
// app constructor implementation (placed after Launch definition)
// ============================================================
inline app::app(std::string name,
                std::string icon,
                std::string exec,
                bool terminal)
    : Name(std::move(name)), Icon(std::move(icon)){
    launch = [exec = std::move(exec), terminal](Launch *ctx)
    {
        if (terminal)
            ctx->launch_Exec_in_terminal(exec);
        else
            ctx->launch_Exec(exec);
    };
}

inline app::app(std::string name,
                std::string icon,
                std::string exec,
                bool terminal,
                bool sysplause)
    : Name(std::move(name)), Icon(std::move(icon)){
    launch = [exec = std::move(exec), terminal, sysplause](Launch *ctx)
    {
        if (terminal)
            ctx->launch_Exec_in_terminal(exec, sysplause);
        else
            ctx->launch_Exec(exec);
    };
}

inline app::app(std::string name,
                std::string icon,
                std::string exec,
                bool terminal,
                bool sysplause,
                bool run_as_root)
    : Name(std::move(name)), Icon(std::move(icon)){
    launch = [exec = std::move(exec), terminal, sysplause, run_as_root](Launch *ctx)
    {
        if (terminal)
            ctx->launch_Exec_in_terminal(exec, sysplause);
        else
            ctx->launch_Exec(exec, run_as_root);
    };
}

template <class PageT>
app::app(std::string name,
         std::string icon,
         page_t<PageT> /*tag*/)
    : Name(std::move(name)), Icon(std::move(icon)){
    launch = [](Launch *self)
    {
        /* Instant feedback: show the overlay, then force an immediate
         * redraw so it actually paints BEFORE the (sometimes slow) page
         * construction starts. Without lv_refr_now() the overlay would
         * only hit the framebuffer after the constructor returns, which
         * defeats the whole point. */
        ui_loading::show("Loading...");
        lv_refr_now(NULL);
        auto p = std::make_shared<PageT>();
        self->app_Page = p;
        lv_disp_load_scr(p->screen());
        lv_indev_set_group(lv_indev_get_next(NULL),
                           p->input_group());
        p->navigate_home =
            std::bind(&Launch::go_back_home, self);
        /* Page is now attached and drawable; hide the overlay. The
         * next LVGL frame will paint the new page without it. */
        ui_loading::hide();
    };
}

// ============================================================
// Launch destructor implementation
// ============================================================
Launch::~Launch()
{
    if (watch_timer)
    {
        lv_timer_delete(watch_timer);
        watch_timer = nullptr;
    }
    if (dir_watcher)
    {
        cp0_dir_watch_stop(dir_watcher);
        dir_watcher = NULL;
    }
}

Launch::Launch() = default;

void Launch::set_launch_page(std::shared_ptr<UILaunchPage> launch_page)
{
    launch_page_ = std::move(launch_page);
}
