#include "../ui.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include "hal/hal_paths.h"
#include "hal/hal_filesystem.h"
#include "hal/hal_process.h"
#include "hal/hal_settings.h"
#include "hal/hal_audio.h"
#include <unordered_map>
#include <list>
#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <fstream>
#include <sstream>
#include "ui_launch_page.hpp"
#include "ui_app_store.hpp"
#include "ui_app_music.hpp"
#include "ui_app_setup.hpp"
#include "ui_app_console.hpp"
#include "ui_app_IpPanel.hpp"
#include "ui_app_stock.hpp"
#include "ui_app_chat.hpp"
#include "ui_app_email.hpp"
#include "ui_app_file.hpp"
#include "ui_app_ssh.hpp"
#include "ui_app_hack.hpp"
#include "ui_app_mesh.hpp"
#include "ui_app_rec.hpp"
#include "ui_app_camera.hpp"
#include "ui_app_game.hpp"
#include "ui_app_UnitEnv.hpp"
#include "ui_app_midi.hpp"
#include "ui_app_gpio.hpp"
#include "../ui_loading.h"
/* img_path() now defined in ui_app_page.hpp */

// ============================================================
// 启动快捷方式示例
// ============================================================
/*
root@pi:/home/pi# cat /usr/share/APPLaunch/applications/vim.desktop
[Desktop Entry]
Name=Vim
TryExec=vim
Exec=vim
Terminal=true
Icon=share/images/email.png
*/




// 前向声明
class app_launch_S;

// ============================================================
// 类型标签
// ============================================================
template <class PageT>
struct page_t
{
    using type = PageT;
};
template <class PageT>
inline constexpr page_t<PageT> page_v{};

// ============================================================
// app:统一的应用描述 + 发射器
// ============================================================
struct app
{
    std::string Name;
    std::string Icon;
    std::string Exec;
    std::function<void(app_launch_S *)> launch;

    // ① 外部命令
    app(std::string name,
        std::string icon,
        std::string exec,
        bool terminal);

    // ① 外部命令
    app(std::string name,
        std::string icon,
        std::string exec,
        bool terminal, bool sysplause);

    // ② 内置 UI 页面
    template <class PageT>
    app(std::string name,
        std::string icon,
        page_t<PageT> /*tag*/);
};

// ============================================================
// app_launch_S
// ============================================================
class app_launch_S
{
private:
    int current_app = 2;
    hal_watcher_t dir_watcher = NULL;
    lv_timer_t *watch_timer = nullptr; // LVGL 3s 定时器
    lv_timer_t *status_timer = nullptr; // 状态栏刷新定时器
    int fixed_count;

public:
    std::list<app> app_list;
    std::shared_ptr<void> app_Page;

public:
    app_launch_S()
    {
        // 固定图标，不允许用户修改
        app_list.emplace_back("Python",
                              img_path("PYTHON_logo.png"), "python3", true, false);
        app_list.emplace_back("STORE",
                              img_path("Store_logo.png"), page_v<UIStorePage>);
        app_list.emplace_back("CLI",
                              img_path("CLI_logo.png"), "bash", true, false);
        app_list.emplace_back("CLAW",
                              img_path("CLAW_logo.png"), "/home/pi/zeroclaw agent", true);
        app_list.emplace_back("SETTING",
                              img_path("SETTING_logo.png"), page_v<UISetupPage>);

        {
            auto it = std::next(app_list.begin(), 0);
            lv_label_set_text(ui_zuoLabelout, it->Name.c_str());
            lv_obj_set_style_bg_img_src(ui_outPanelzuo, it->Icon.c_str(),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            auto it = std::next(app_list.begin(), 1);
            lv_label_set_text(ui_zuoLabel, it->Name.c_str());
            lv_obj_set_style_bg_img_src(ui_zuoPanel, it->Icon.c_str(),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            auto it = std::next(app_list.begin(), 2);
            lv_label_set_text(ui_switchLabel, it->Name.c_str());
            lv_obj_set_style_bg_img_src(ui_switchPanel, it->Icon.c_str(),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            auto it = std::next(app_list.begin(), 3);
            lv_label_set_text(ui_youLabel, it->Name.c_str());
            lv_obj_set_style_bg_img_src(ui_youPanel, it->Icon.c_str(),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            auto it = std::next(app_list.begin(), 4);
            lv_label_set_text(ui_youLabelout, it->Name.c_str());
            lv_obj_set_style_bg_img_src(ui_outPanelyou, it->Icon.c_str(),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }


        // 动态图标，允许用户自定义
        app_list.emplace_back("MUSIC",
                              img_path("MUSIC_logo.png"), page_v<UIMusicPage>);
        app_list.emplace_back("AUDIO_PLAYER",
                              img_path("MUSIC_logo.png"),
                              "tinyplay -D1 -d0 /home/pi/zhou.wav",
                              true);
        app_list.emplace_back("IP_PANEL",
                              img_path("ssh.png"), page_v<UIIpPanelPage>);

        app_list.emplace_back("MATH",
                              img_path("math.png"), 
                              "/home/pi/M5CardputerZero-Calculator-linux-aarch64", false);


        app_list.emplace_back("STOCKS",
                              img_path("stocks_macos_bigsur_icon_189691.png"), page_v<UIStockPage>);

        app_list.emplace_back("CHAT",
                              img_path("chat.png"), page_v<UIchatPage>);
        app_list.emplace_back("e-Mail",
                              img_path("email.png"), page_v<UIEmailPage>);
        app_list.emplace_back("FILE",
                              img_path("CLI_logo.png"), page_v<UIFilePage>);
        app_list.emplace_back("SSH",
                              img_path("ssh.png"), page_v<UISSHPage>);
        app_list.emplace_back("HACK",
                              img_path("hack.png"), page_v<UIHackPage>);
        app_list.emplace_back("MESH",
                              img_path("mesh.png"), page_v<UIMeshPage>);
        app_list.emplace_back("REC",
                              img_path("rec.png"), page_v<UIRecPage>);
        app_list.emplace_back("CAMERA",
                              img_path("camera.png"), page_v<UICameraPage>);
        app_list.emplace_back("GAME",
                              img_path("gmae.png"), page_v<UIGamePage>);
        app_list.emplace_back("UnitEnv",
                              img_path("hack.png"), page_v<UIUnitEnvPage>);
        app_list.emplace_back("Midi",
                              img_path("hack.png"), page_v<UIMidiPage>);

        app_list.emplace_back("Gpio",
                              img_path("hack.png"), page_v<UIGpioPage>);



        fixed_count = app_list.size();
        applications_load();

        // 初始化 inotify，监听 applications 目录
        inotify_init_watch();

        // 创建 LVGL 3s 定时器，周期性检查目录变化
        watch_timer = lv_timer_create(app_dir_watch_cb, 3000, this);

        // 状态栏定时刷新（时间 + 电量），每5秒更新一次
        update_home_status_bar();
        status_timer = lv_timer_create(home_status_timer_cb, 5000, this);
    }

    void launch_app()
    {
        auto it = std::next(app_list.begin(), current_app);
        it->launch(this);
    }

    static void lv_go_back_home(void *arg)
    {
        auto self = (app_launch_S *)arg;
        printf("[HOME] lv_go_back_home executing (page=%p)\n", self->app_Page.get());
        lv_timer_enable(true);
        lv_indev_set_group(lv_indev_get_next(NULL), Screen1group);
        lv_disp_load_scr(ui_Screen1);
        lv_refr_now(NULL);
        if (self->app_Page)
            self->app_Page.reset();
        printf("[HOME] lv_go_back_home done, on launcher home\n");
    }

    void go_back_home()
    {
        printf("[HOME] go_back_home() requested, scheduling async call (page=%p)\n", app_Page.get());
        lv_async_call(lv_go_back_home, this);
    }

    // 改为接收 std::string，不再依赖 app::Exec 成员
    void launch_Exec_in_terminal(const std::string &exec, bool sysplause = true)
    {
        printf("Launching terminal app: %s\n", exec.c_str());
        /* Instant visual feedback; paint before the (potentially slow)
         * Console page construction so the user sees it right away. */
        ui_loading_show("Loading...");
        lv_refr_now(NULL);
        auto p = std::make_shared<UIConsolePage>();
        app_Page = p;
        lv_disp_load_scr(p->get_ui());
        lv_indev_set_group(lv_indev_get_next(NULL), p->get_key_group());
        p->go_back_home = std::bind(&app_launch_S::go_back_home, this);
        p->terminal_sysplause = sysplause;
        /* Console page fully covers APP_Container; safe to hide now.
         * The heavy exec() call below will still run while the terminal
         * page is on-screen — no overlay needed at that point. */
        ui_loading_hide();
        p->exec(exec);
    }

    void launch_Exec(const std::string &exec)
    {
        printf("Launching external app: %s\n", exec.c_str());
        /* Show overlay BEFORE we tear down LVGL input/timers so the user
         * gets immediate feedback when ENTER was pressed. The overlay
         * stays drawn on the framebuffer right up until the child takes
         * it over via hal_process_exec_blocking(). */
        ui_loading_show("Loading...");
        lv_disp_t *disp = lv_disp_get_default();
        lv_indev_t *indev = lv_indev_get_next(NULL);
        LVGL_RUN_FLAGE = 0;
        if (indev)
            lv_indev_set_group(indev, NULL);
        lv_timer_enable(false);
        lv_refr_now(disp);

        int ret = hal_process_exec_blocking(exec.c_str(), &LVGL_HOME_KEY_FLAGE);
        printf("App %s exited with code %d\n", exec.c_str(), ret);
        lv_timer_enable(true);
        if (indev)
            lv_indev_set_group(indev, Screen1group);
        lv_disp_load_scr(ui_Screen1);
        /* Child process has returned; we are back on the launcher home.
         * Hide the overlay so it doesn't linger. */
        ui_loading_hide();
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(disp);
        LVGL_RUN_FLAGE = 1;
    }


    void zuo(lv_obj_t *panel, lv_obj_t *label)
    {
        current_app = current_app == (int)app_list.size() - 1 ? 0 : current_app + 1;
        int next_app = current_app;
        next_app = next_app == (int)app_list.size() - 1 ? 0 : next_app + 1;
        next_app = next_app == (int)app_list.size() - 1 ? 0 : next_app + 1;
        auto it = std::next(app_list.begin(), next_app);
        lv_label_set_text(label, it->Name.c_str());
        lv_obj_set_style_bg_img_src(panel, it->Icon.c_str(),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void you(lv_obj_t *panel, lv_obj_t *label)
    {
        current_app = current_app == 0 ? (int)app_list.size() - 1 : current_app - 1;
        int next_app = current_app;
        next_app = next_app == 0 ? (int)app_list.size() - 1 : next_app - 1;
        next_app = next_app == 0 ? (int)app_list.size() - 1 : next_app - 1;
        auto it = std::next(app_list.begin(), next_app);
        lv_label_set_text(label, it->Name.c_str());
        lv_obj_set_style_bg_img_src(panel, it->Icon.c_str(),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    void applications_load()
    {
        const char *app_dir = hal_path_applications_dir();
        DIR *dir = opendir(app_dir);
        if (!dir)
        {
            perror("applications_load: opendir failed");
            return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            // 仅处理 *.desktop 文件
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

            // 解析 INI 文件
            std::string app_name, app_icon, app_exec;
            bool app_terminal = false;
            bool app_sysplause = true;
            bool in_desktop_entry = false;

            std::string line;
            while (std::getline(ifs, line))
            {
                // 去除行尾的 \r（Windows 换行符）
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                // 跳过空行和注释
                if (line.empty() || line[0] == '#' || line[0] == ';')
                    continue;

                // 检测节头
                if (line[0] == '[')
                {
                    in_desktop_entry = (line == "[Desktop Entry]");
                    continue;
                }

                if (!in_desktop_entry)
                    continue;

                // 解析 key=value
                auto eq = line.find('=');
                if (eq == std::string::npos)
                    continue;

                std::string key   = line.substr(0, eq);
                std::string value = line.substr(eq + 1);

                // 去除 key 首尾空格
                auto ltrim = [](std::string &s) {
                    size_t i = 0;
                    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
                    s = s.substr(i);
                };
                auto rtrim = [](std::string &s) {
                    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
                        s.pop_back();
                };
                ltrim(key); rtrim(key);
                ltrim(value); rtrim(value);

                if (key == "Name")
                    app_name = value;
                else if (key == "Icon")
                    app_icon = value;
                else if (key == "Exec")
                    app_exec = value;
                else if (key == "Terminal")
                    app_terminal = (value == "true" || value == "True" || value == "1");
                else if (key == "Sysplause")
                    app_sysplause = (value == "true" || value == "True" || value == "1");
            }

            // 必须有 Name 和 Exec 才能注册
            if (app_name.empty() || app_exec.empty())
            {
                fprintf(stderr, "applications_load: skip %s (missing Name or Exec)\n", filepath.c_str());
                continue;
            }
            bool in_list = false;
            for(auto it: app_list)
            {
                if(it.Exec == app_exec)
                {
                    in_list = true;
                    break;
                }
            }
            if(in_list)
            {
                fprintf(stderr, "applications_load: skip %s (duplicate Exec)\n", filepath.c_str());
                continue;
            }
                
            app_list.emplace_back(app_name, app_icon, app_exec, app_terminal, app_sysplause);
        }

        closedir(dir);
    }

    // ============================================================
    // inotify 初始化：以非阻塞模式监听 applications 目录
    // ============================================================
    void inotify_init_watch()
    {
        dir_watcher = hal_dir_watch_start(hal_path_applications_dir());
    }

    // ============================================================
    // 刷新 UI 面板（根据当前 current_app 更新 5 个槽位）
    // ============================================================
    void refresh_ui_panels()
    {
        int sz = (int)app_list.size();
        if (sz == 0) return;

        // 确保 current_app 在合法范围内
        if (current_app >= sz)
            current_app = sz - 1;

        auto app_at = [&](int idx) -> app & {
            idx = ((idx % sz) + sz) % sz;
            return *std::next(app_list.begin(), idx);
        };

        // 最左外（隐藏）
        {
            auto &a = app_at(current_app - 2);
            lv_label_set_text(ui_zuoLabelout, a.Name.c_str());
            lv_obj_set_style_bg_img_src(ui_outPanelzuo, a.Icon.c_str(),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        // 左
        {
            auto &a = app_at(current_app - 1);
            lv_label_set_text(ui_zuoLabel, a.Name.c_str());
            lv_obj_set_style_bg_img_src(ui_zuoPanel, a.Icon.c_str(),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        // 中心
        {
            auto &a = app_at(current_app);
            lv_label_set_text(ui_switchLabel, a.Name.c_str());
            lv_obj_set_style_bg_img_src(ui_switchPanel, a.Icon.c_str(),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        // 右
        {
            auto &a = app_at(current_app + 1);
            lv_label_set_text(ui_youLabel, a.Name.c_str());
            lv_obj_set_style_bg_img_src(ui_youPanel, a.Icon.c_str(),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        // 最右外（隐藏）
        {
            auto &a = app_at(current_app + 2);
            lv_label_set_text(ui_youLabelout, a.Name.c_str());
            lv_obj_set_style_bg_img_src(ui_outPanelyou, a.Icon.c_str(),
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    // ============================================================
    // 重新加载动态应用列表（保留固定条目，重扫描 applications 目录）
    // ============================================================
    void applications_reload()
    {
        int sz = (int)app_list.size();
        if (sz > fixed_count)
        {
            auto it = std::next(app_list.begin(), fixed_count);
            app_list.erase(it, app_list.end());
        }
        applications_load();
        refresh_ui_panels();
    }

    // ============================================================
    // 主页状态栏刷新：时间 + 电量（BQ27220）
    // ============================================================
    static void home_status_timer_cb(lv_timer_t *timer)
    {
        auto *self = static_cast<app_launch_S *>(lv_timer_get_user_data(timer));
        if (self) self->update_home_status_bar();
    }

    void update_home_status_bar()
    {
        char time_buf[16];
        hal_time_str(time_buf, sizeof(time_buf));
        lv_label_set_text(ui_timeLabel, time_buf);

        hal_battery_info_t bat = hal_battery_read();
        if (bat.valid) {
            int soc = bat.soc;
            if (soc > 100) soc = 100;
            if (soc < 0) soc = 0;
            lv_bar_set_value(ui_Bar1, soc, LV_ANIM_ON);

            char pwr_buf[16];
            snprintf(pwr_buf, sizeof(pwr_buf), "%d%%", soc);
            lv_label_set_text(ui_powerLabel, pwr_buf);

            uint32_t color = 0x66CC33;
            if (soc <= 20) color = 0xE74C3C;
            else if (soc <= 50) color = 0xF39C12;
            lv_obj_set_style_bg_color(ui_Bar1, lv_color_hex(color),
                                      LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
    }

    // ============================================================
    // LVGL 定时器回调：检测 inotify 事件，有变化则刷新列表
    // ============================================================
    static void app_dir_watch_cb(lv_timer_t *timer)
    {
        auto *self = static_cast<app_launch_S *>(lv_timer_get_user_data(timer));
        if (!self || !self->dir_watcher)
            return;

        if (hal_dir_watch_poll(self->dir_watcher) > 0)
        {
            printf("app_dir_watch_cb: applications dir changed, reloading...\n");
            self->applications_reload();
        }
    }

    ~app_launch_S();
};

// ============================================================
// app 构造函数的实现(放到 app_launch_S 定义之后)
// ============================================================
inline app::app(std::string name,
                std::string icon,
                std::string exec,
                bool terminal)
    : Name(std::move(name)), Icon(std::move(icon))
{
    launch = [exec = std::move(exec), terminal](app_launch_S *ctx)
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
    : Name(std::move(name)), Icon(std::move(icon))
{
    launch = [exec = std::move(exec), terminal, sysplause](app_launch_S *ctx)
    {
        if (terminal)
            ctx->launch_Exec_in_terminal(exec, sysplause);
        else
            ctx->launch_Exec(exec);
    };
}

template <class PageT>
app::app(std::string name,
         std::string icon,
         page_t<PageT> /*tag*/)
    : Name(std::move(name)), Icon(std::move(icon))
{
    launch = [](app_launch_S *self)
    {
        /* Instant feedback: show the overlay, then force an immediate
         * redraw so it actually paints BEFORE the (sometimes slow) page
         * construction starts. Without lv_refr_now() the overlay would
         * only hit the framebuffer after the constructor returns, which
         * defeats the whole point. */
        ui_loading_show("Loading...");
        lv_refr_now(NULL);
        auto p = std::make_shared<PageT>();
        self->app_Page = p;
        lv_disp_load_scr(p->get_ui());
        lv_indev_set_group(lv_indev_get_next(NULL),
                           p->get_key_group());
        p->go_back_home =
            std::bind(&app_launch_S::go_back_home, self);
        /* Page is now attached and drawable; hide the overlay. The
         * next LVGL frame will paint the new page without it. */
        ui_loading_hide();
    };
}

// ============================================================
// app_launch_S 析构函数实现
// ============================================================
app_launch_S::~app_launch_S()
{
    if (status_timer)
    {
        lv_timer_delete(status_timer);
        status_timer = nullptr;
    }
    if (watch_timer)
    {
        lv_timer_delete(watch_timer);
        watch_timer = nullptr;
    }
    if (dir_watcher)
    {
        hal_dir_watch_stop(dir_watcher);
        dir_watcher = NULL;
    }
}

// ============================================================
std::unique_ptr<app_launch_S> app_launch_Ser;

extern "C"
{
    
    void ui_info_bind()
    {
        app_launch_Ser = std::make_unique<app_launch_S>();        
    }
    void cpp_app_zuo(lv_obj_t *panel, lv_obj_t *label)
    {
        app_launch_Ser->zuo(panel, label);
    }
    void cpp_app_you(lv_obj_t *panel, lv_obj_t *label)
    {
        app_launch_Ser->you(panel, label);
    }
    void cpp_app_launch()
    {
        app_launch_Ser->launch_app();
    }
}