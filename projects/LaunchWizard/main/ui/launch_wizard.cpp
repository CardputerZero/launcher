#include "main.h"

#include <stdint.h>

#include "global_config.h"
#include "keyboard_input.h"
#include "lvgl/lvgl.h"

#ifdef __linux__
#include <linux/input.h>
#else
#include "compat/input_keys.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef LAUNCH_WIZARD_DRY_RUN
#if defined(CONFIG_V9_5_LV_USE_SDL)
#define LAUNCH_WIZARD_DRY_RUN 1
#else
#define LAUNCH_WIZARD_DRY_RUN 0
#endif
#endif

namespace {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 170;
constexpr int kTitleHeight = 24;
constexpr int kFieldHeight = 28;
constexpr uid_t kDefaultUserUid = 1000;

enum class Focus {
    User,
    Password,
    Confirm,
};

struct CommandResult {
    int code = 0;
    std::string output;
};

struct UiState {
    Focus focus = Focus::User;
    bool busy = false;
    bool done = false;
    bool worker_finished = false;
    bool existing_user_hint_shown = false;
    int exit_ticks = 0;
    int auto_detect_ticks = 0;
    std::string username = "pi";
    std::string password;
    std::string worker_message;

    lv_obj_t *screen = nullptr;
    lv_obj_t *user_box = nullptr;
    lv_obj_t *pass_box = nullptr;
    lv_obj_t *confirm_button = nullptr;
    lv_obj_t *user_value = nullptr;
    lv_obj_t *pass_value = nullptr;
    lv_obj_t *status = nullptr;
    lv_timer_t *poll_timer = nullptr;
    std::mutex mutex;
};

UiState g_ui;

void style_plain(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void set_label(lv_obj_t *label, const std::string &text)
{
    lv_label_set_text(label, text.c_str());
}

std::string masked_password()
{
    if (g_ui.password.empty())
        return "";
    return std::string(g_ui.password.size(), '*');
}

bool validate_username(const std::string &name, std::string &error)
{
    if (name.empty()) {
        error = "Username required";
        return false;
    }
    if (name.size() > 32) {
        error = "Username too long";
        return false;
    }
    if (name == "root") {
        error = "Do not create root";
        return false;
    }
    unsigned char first = static_cast<unsigned char>(name[0]);
    if (!(islower(first) || name[0] == '_')) {
        error = "Use lowercase user name";
        return false;
    }
    for (size_t i = 1; i < name.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(name[i]);
        bool ok = islower(ch) || isdigit(ch) || name[i] == '_' || name[i] == '-';
        if (name[i] == '$' && i == name.size() - 1)
            ok = true;
        if (!ok) {
            error = "Invalid username char";
            return false;
        }
    }
    return true;
}

bool validate_password(const std::string &password, std::string &error)
{
    if (password.empty()) {
        error = "Password required";
        return false;
    }
    for (char ch : password) {
        if (ch == '\n' || ch == '\r' || ch == ':' || static_cast<unsigned char>(ch) < 0x20) {
            error = "Password has bad char";
            return false;
        }
    }
    return true;
}

void print_command(const std::vector<std::string> &args, const std::string *stdin_text)
{
    printf("LaunchWizard dry-run:");
    for (const std::string &arg : args) {
        bool needs_quotes = arg.empty();
        for (char ch : arg) {
            if (isspace(static_cast<unsigned char>(ch)) || ch == '\'' || ch == '"' || ch == '\\') {
                needs_quotes = true;
                break;
            }
        }
        if (!needs_quotes) {
            printf(" %s", arg.c_str());
            continue;
        }

        printf(" '");
        for (char ch : arg) {
            if (ch == '\'')
                printf("'\\''");
            else
                putchar(ch);
        }
        putchar('\'');
    }
    if (stdin_text)
        printf(" <stdin:%zu bytes>", stdin_text->size());
    putchar('\n');
    fflush(stdout);
}

CommandResult run_command(const std::vector<std::string> &args, const std::string *stdin_text = nullptr)
{
    CommandResult result;
#if LAUNCH_WIZARD_DRY_RUN
    print_command(args, stdin_text);
    return result;
#else
    int out_pipe[2] = {-1, -1};
    int in_pipe[2] = {-1, -1};

    if (pipe(out_pipe) != 0) {
        result.code = 127;
        result.output = strerror(errno);
        return result;
    }
    if (stdin_text && pipe(in_pipe) != 0) {
        close(out_pipe[0]);
        close(out_pipe[1]);
        result.code = 127;
        result.output = strerror(errno);
        return result;
    }

    pid_t pid = fork();
    if (pid == 0) {
        if (stdin_text) {
            dup2(in_pipe[0], STDIN_FILENO);
            close(in_pipe[0]);
            close(in_pipe[1]);
        }
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(out_pipe[0]);
        close(out_pipe[1]);

        std::vector<char *> argv;
        argv.reserve(args.size() + 1);
        for (const std::string &arg : args)
            argv.push_back(const_cast<char *>(arg.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(out_pipe[1]);
    if (stdin_text) {
        close(in_pipe[0]);
        const char *data = stdin_text->c_str();
        size_t left = stdin_text->size();
        while (left > 0) {
            ssize_t written = write(in_pipe[1], data, left);
            if (written <= 0)
                break;
            data += written;
            left -= static_cast<size_t>(written);
        }
        close(in_pipe[1]);
    }

    char buffer[256];
    ssize_t read_count = 0;
    while ((read_count = read(out_pipe[0], buffer, sizeof(buffer))) > 0) {
        if (result.output.size() < 800)
            result.output.append(buffer, static_cast<size_t>(read_count));
    }
    close(out_pipe[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        result.code = 127;
        result.output = strerror(errno);
    } else if (WIFEXITED(status)) {
        result.code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.code = 128 + WTERMSIG(status);
    } else {
        result.code = 127;
    }

    while (!result.output.empty() &&
           (result.output.back() == '\n' || result.output.back() == '\r'))
        result.output.pop_back();
    return result;
#endif
}

bool command_ok(const std::vector<std::string> &args, std::string &error)
{
    CommandResult result = run_command(args);
    if (result.code == 0)
        return true;
    error = args.empty() ? "command failed" : args[0] + " failed";
    if (!result.output.empty())
        error += ": " + result.output;
    return false;
}

bool user_exists(const std::string &name)
{
#if LAUNCH_WIZARD_DRY_RUN
    (void)name;
    return false;
#else
    return getpwnam(name.c_str()) != nullptr;
#endif
}

bool uid_exists(uid_t uid)
{
#if LAUNCH_WIZARD_DRY_RUN
    (void)uid;
    return false;
#else
    return getpwuid(uid) != nullptr;
#endif
}

uid_t user_uid(const std::string &user)
{
#if LAUNCH_WIZARD_DRY_RUN
    (void)user;
    return kDefaultUserUid;
#else
    struct passwd *pw = getpwnam(user.c_str());
    return pw ? pw->pw_uid : static_cast<uid_t>(0);
#endif
}

std::string user_name_for_uid(uid_t uid)
{
#if LAUNCH_WIZARD_DRY_RUN
    (void)uid;
    return "";
#else
    struct passwd *pw = getpwuid(uid);
    return pw ? std::string(pw->pw_name) : std::string();
#endif
}

std::vector<std::string> initial_user_groups()
{
#if LAUNCH_WIZARD_DRY_RUN
    return {"adm", "dialout", "cdrom", "sudo", "audio", "video", "plugdev",
            "games", "users", "input", "render", "netdev", "gpio", "i2c", "spi"};
#else
    static const char *candidates[] = {
        "adm", "dialout", "cdrom", "sudo", "audio", "video", "plugdev",
        "games", "users", "input", "render", "netdev", "gpio", "i2c", "spi",
    };
    std::vector<std::string> groups;
    for (const char *group : candidates) {
        if (getgrnam(group))
            groups.emplace_back(group);
    }
    return groups;
#endif
}

std::string join_groups(const std::vector<std::string> &groups)
{
    std::string joined;
    for (const std::string &group : groups) {
        if (!joined.empty())
            joined += ",";
        joined += group;
    }
    return joined;
}

bool create_user(const std::string &user, std::string &error)
{
    if (user_exists(user)) {
        error = "Target user already exists";
        return false;
    }
    if (uid_exists(kDefaultUserUid)) {
        error = "UID 1000 already exists";
        return false;
    }

    std::vector<std::string> args = {
        "useradd",
        "--create-home",
        "--user-group",
        "--uid", std::to_string(kDefaultUserUid),
        "--shell", "/bin/bash",
    };
    std::vector<std::string> groups = initial_user_groups();
    if (!groups.empty()) {
        args.push_back("--groups");
        args.push_back(join_groups(groups));
    }
    args.push_back(user);
    return command_ok(args, error);
}

bool set_user_password(const std::string &user, const std::string &password, std::string &error)
{
    std::string chpasswd_input = user + ":" + password + "\n";
    CommandResult pass_result = run_command({"chpasswd"}, &chpasswd_input);
    if (pass_result.code == 0)
        return true;

    error = "chpasswd failed";
    if (!pass_result.output.empty())
        error += ": " + pass_result.output;
    return false;
}

void disable_piwiz(const std::string &user)
{
    run_command({"systemctl", "disable", "--now", "piwiz.service"});
    run_command({"systemctl", "mask", "piwiz.service"});
    run_command({"pkill", "-x", "piwiz"});
    run_command({"rm", "-f",
                 "/etc/xdg/autostart/piwiz.desktop.dpkg-new",
                 "/etc/xdg/autostart/piwiz.desktop.dpkg-dist",
                 "/etc/xdg/autostart/piwiz.desktop.dpkg-old"});
    run_command({"install", "-d", "-m", "0755", "/etc/xdg/autostart"});
    const std::string hidden_entry =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=piwiz\n"
        "Hidden=true\n";
    run_command({"tee", "/etc/xdg/autostart/piwiz.desktop"}, &hidden_entry);
    run_command({"chmod", "0644", "/etc/xdg/autostart/piwiz.desktop"});

    const std::string user_autostart = "/home/" + user + "/.config/autostart";
    const std::string user_piwiz = user_autostart + "/piwiz.desktop";
    run_command({"install", "-d", "-m", "0755", "-o", user, "-g", user, user_autostart});
    run_command({"tee", user_piwiz}, &hidden_entry);
    run_command({"chown", user + ":" + user, user_piwiz});
    run_command({"chmod", "0644", user_piwiz});
}

std::string configure_desktop_startup(const std::string &user)
{
    std::string warning;

    disable_piwiz(user);

    run_command({"systemctl", "set-default", "graphical.target"});

    CommandResult raspi_config = run_command({"raspi-config", "nonint", "do_boot_behaviour", "B4"});
    if (raspi_config.code != 0) {
        warning = "raspi-config desktop autologin failed";
        if (!raspi_config.output.empty())
            warning += ": " + raspi_config.output;
    }

    run_command({"install", "-d", "-m", "0755", "/etc/lightdm/lightdm.conf.d"});
    const std::string lightdm_conf =
        "[Seat:*]\n"
        "autologin-user=" + user + "\n"
        "autologin-user-timeout=0\n";
    CommandResult write_conf = run_command(
        {"tee", "/etc/lightdm/lightdm.conf.d/50-launchwizard-autologin.conf"},
        &lightdm_conf);
    if (write_conf.code != 0) {
        warning = "LightDM autologin config failed";
        if (!write_conf.output.empty())
            warning += ": " + write_conf.output;
    }
    run_command({"chmod", "0644", "/etc/lightdm/lightdm.conf.d/50-launchwizard-autologin.conf"});

    run_command({"systemctl", "enable", "lightdm.service"});
    CommandResult desktop = run_command({"systemctl", "restart", "lightdm.service"});
    if (desktop.code != 0) {
        CommandResult start = run_command({"systemctl", "start", "lightdm.service"});
        if (start.code != 0 && warning.empty()) {
            warning = "LightDM start failed";
            if (!start.output.empty())
                warning += ": " + start.output;
        }
    }

    return warning;
}

std::string start_applaunch_service(const std::string &user, uid_t uid)
{
    std::string warning;
    std::string uid_text = std::to_string(uid);
    std::string runtime_dir = "XDG_RUNTIME_DIR=/run/user/" + uid_text;
    std::string bus_address = "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/" + uid_text + "/bus";

    run_command({"loginctl", "enable-linger", user});
    run_command({"systemctl", "daemon-reload"});
    run_command({"systemctl", "start", "user@" + uid_text + ".service"});
    run_command({"runuser", "-u", user, "--", "env",
                 runtime_dir,
                 bus_address,
                 "systemctl", "--user", "daemon-reload"});
    run_command({"runuser", "-u", user, "--", "env",
                 runtime_dir,
                 bus_address,
                 "systemctl", "--user", "enable", "APPLaunch.service"});
    CommandResult restart = run_command({"runuser", "-u", user, "--", "env",
                                         runtime_dir,
                                         bus_address,
                                         "systemctl", "--user", "restart", "APPLaunch.service"});
    if (restart.code != 0) {
        CommandResult start = run_command({"runuser", "-u", user, "--", "env",
                                           runtime_dir,
                                           bus_address,
                                           "systemctl", "--user", "start", "APPLaunch.service"});
        if (start.code != 0) {
            warning = "APPLaunch start failed";
            if (!start.output.empty())
                warning += ": " + start.output;
        }
    }
    return warning;
}

std::string finish_setup_for_user(const std::string &user, uid_t uid)
{
    if (uid == 0)
        return "User UID not found";

    std::string desktop_warning = configure_desktop_startup(user);

    std::string service_warning = start_applaunch_service(user, uid);
    if (!service_warning.empty())
        return service_warning;

    run_command({"systemctl", "disable", "--now", "LaunchWizard.service"});
    if (!desktop_warning.empty()) {
        printf("LaunchWizard: desktop warning: %s\n", desktop_warning.c_str());
        fflush(stdout);
    }
    return "";
}

std::string apply_user_config(std::string target_user, std::string password)
{
#if LAUNCH_WIZARD_DRY_RUN
    printf("LaunchWizard dry-run: configure user=%s uid=%u\n",
           target_user.c_str(), static_cast<unsigned>(kDefaultUserUid));
    fflush(stdout);
#else
    if (geteuid() != 0)
        return "LaunchWizard must run as root";
#endif

    std::string error;
    uid_t uid = 0;
    if (user_exists(target_user)) {
        uid = user_uid(target_user);
        if (uid == 0)
            return "Existing user UID not found";
    } else {
        if (!create_user(target_user, error))
            return error;
        uid = user_uid(target_user);
    }

    if (!set_user_password(target_user, password, error))
        return error;

    return finish_setup_for_user(target_user, uid);
}

void refresh_ui()
{
    const bool user_focus = g_ui.focus == Focus::User;
    const bool pass_focus = g_ui.focus == Focus::Password;
    const bool confirm_focus = g_ui.focus == Focus::Confirm;

    lv_obj_set_style_border_color(g_ui.user_box,
                                  lv_color_hex(user_focus ? 0x2f80ed : 0x3a3f45), 0);
    lv_obj_set_style_border_width(g_ui.user_box, user_focus ? 2 : 1, 0);
    lv_obj_set_style_border_color(g_ui.pass_box,
                                  lv_color_hex(pass_focus ? 0x2f80ed : 0x3a3f45), 0);
    lv_obj_set_style_border_width(g_ui.pass_box, pass_focus ? 2 : 1, 0);
    lv_obj_set_style_bg_color(g_ui.confirm_button,
                              lv_color_hex(confirm_focus ? 0x2f80ed : 0x23272e), 0);
    lv_obj_set_style_text_color(g_ui.confirm_button,
                                lv_color_hex(confirm_focus ? 0xffffff : 0xd7dde5), 0);
    set_label(g_ui.user_value, g_ui.username.empty() ? " " : g_ui.username);
    set_label(g_ui.pass_value, g_ui.password.empty() ? " " : masked_password());
}

void set_status(const char *text)
{
    lv_label_set_text(g_ui.status, text);
}

void move_focus(int delta)
{
    int idx = 0;
    if (g_ui.focus == Focus::Password)
        idx = 1;
    else if (g_ui.focus == Focus::Confirm)
        idx = 2;
    idx = (idx + delta + 3) % 3;
    g_ui.focus = idx == 0 ? Focus::User : (idx == 1 ? Focus::Password : Focus::Confirm);
    refresh_ui();
}

void start_apply()
{
    if (g_ui.busy || g_ui.done)
        return;

    std::string error;
    if (!validate_username(g_ui.username, error) || !validate_password(g_ui.password, error)) {
        set_status(error.c_str());
        return;
    }

    g_ui.busy = true;
    set_status("Configuring...");
    refresh_ui();

    const std::string username = g_ui.username;
    const std::string password = g_ui.password;
    std::thread([username, password]() {
        std::string message = apply_user_config(username, password);
        std::lock_guard<std::mutex> lock(g_ui.mutex);
        g_ui.worker_message = message.empty() ? "Done. Starting APPLaunch..." : message;
        g_ui.worker_finished = true;
        g_ui.done = message.empty();
    }).detach();
}

void show_existing_user_hint(const std::string &user)
{
    if (g_ui.busy || g_ui.done || g_ui.existing_user_hint_shown)
        return;

    g_ui.username = user;
    g_ui.existing_user_hint_shown = true;
    set_status("Existing user found. Enter password.");
    refresh_ui();
}

void append_char(char ch)
{
    if (g_ui.busy || g_ui.done)
        return;
    std::string *target = g_ui.focus == Focus::User ? &g_ui.username :
                          (g_ui.focus == Focus::Password ? &g_ui.password : nullptr);
    if (!target)
        return;
    if (target->size() >= 32)
        return;
    target->push_back(ch);
    refresh_ui();
}

void backspace()
{
    if (g_ui.busy || g_ui.done)
        return;
    std::string *target = g_ui.focus == Focus::User ? &g_ui.username :
                          (g_ui.focus == Focus::Password ? &g_ui.password : nullptr);
    if (target && !target->empty()) {
        target->pop_back();
        refresh_ui();
    }
}

void handle_enter()
{
    if (g_ui.focus == Focus::User)
        g_ui.focus = Focus::Password;
    else if (g_ui.focus == Focus::Password)
        g_ui.focus = Focus::Confirm;
    else
        start_apply();
    refresh_ui();
}

void keyboard_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD))
        return;

    auto *key = static_cast<key_item *>(lv_event_get_param(event));
    if (!key || key->key_state == KBD_KEY_RELEASED)
        return;

    switch (key->key_code) {
    case KEY_TAB:
    case KEY_DOWN:
    case KEY_RIGHT:
        move_focus(1);
        return;
    case KEY_UP:
    case KEY_LEFT:
        move_focus(-1);
        return;
    case KEY_ENTER:
        handle_enter();
        return;
    case KEY_BACKSPACE:
    case KEY_DELETE:
        backspace();
        return;
    default:
        break;
    }

    if (key->utf8[0] != '\0' && key->utf8[1] == '\0') {
        unsigned char ch = static_cast<unsigned char>(key->utf8[0]);
        if (ch >= 0x20 && ch < 0x7f)
            append_char(static_cast<char>(ch));
    }
}

void field_click_cb(lv_event_t *event)
{
    intptr_t value = reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
    g_ui.focus = value == 0 ? Focus::User : Focus::Password;
    refresh_ui();
}

void confirm_click_cb(lv_event_t *event)
{
    (void)event;
    g_ui.focus = Focus::Confirm;
    refresh_ui();
    start_apply();
}

lv_obj_t *create_label(lv_obj_t *parent, const char *text, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xf3f5f7), 0);
    return label;
}

lv_obj_t *create_field(lv_obj_t *parent, int y, const char *caption,
                       lv_obj_t **value_label, Focus focus, intptr_t user_data)
{
    lv_obj_t *label = create_label(parent, caption, &lv_font_montserrat_12);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 16, y + 7);
    lv_obj_set_style_text_color(label, lv_color_hex(0x99a2ad), 0);

    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 210, kFieldHeight);
    lv_obj_align(box, LV_ALIGN_TOP_LEFT, 92, y);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x171a20), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x3a3f45), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_pad_left(box, 8, 0);
    lv_obj_set_style_pad_right(box, 8, 0);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(box, field_click_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(user_data));

    *value_label = create_label(box, "", &lv_font_montserrat_14);
    lv_label_set_long_mode(*value_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(*value_label, 190);
    lv_obj_align(*value_label, LV_ALIGN_LEFT_MID, 0, 0);

    if (focus == Focus::User)
        g_ui.user_box = box;
    else
        g_ui.pass_box = box;
    return box;
}

void poll_worker_cb(lv_timer_t *timer)
{
    (void)timer;
    bool done = false;
    {
        std::lock_guard<std::mutex> lock(g_ui.mutex);
        if (g_ui.worker_finished) {
            g_ui.busy = false;
            g_ui.worker_finished = false;
            set_status(g_ui.worker_message.c_str());
        }
        done = g_ui.done;
    }

    if (done) {
        ++g_ui.exit_ticks;
        if (g_ui.exit_ticks > 25)
            exit(0);
        return;
    }

#if !LAUNCH_WIZARD_DRY_RUN
    if (!g_ui.busy) {
        ++g_ui.auto_detect_ticks;
        if (g_ui.auto_detect_ticks >= 10) {
            g_ui.auto_detect_ticks = 0;
            std::string user = user_name_for_uid(kDefaultUserUid);
            if (!user.empty() && user != "root")
                show_existing_user_hint(user);
        }
    }
#endif
}

void build_ui()
{
    g_ui.screen = lv_screen_active();
    style_plain(g_ui.screen);
    lv_obj_set_size(g_ui.screen, kScreenWidth, kScreenHeight);
    lv_obj_set_style_bg_color(g_ui.screen, lv_color_hex(0x0f1115), 0);
    lv_obj_set_style_bg_opa(g_ui.screen, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(g_ui.screen, keyboard_event_cb,
                        static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), nullptr);

    lv_obj_t *title_bar = lv_obj_create(g_ui.screen);
    style_plain(title_bar);
    lv_obj_set_size(title_bar, LV_PCT(100), kTitleHeight);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x181b21), 0);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title = create_label(title_bar, "First Boot Setup", &lv_font_montserrat_14);
    lv_obj_center(title);

    create_field(g_ui.screen, 42, "User", &g_ui.user_value, Focus::User, 0);
    create_field(g_ui.screen, 78, "Password", &g_ui.pass_value, Focus::Password, 1);

    g_ui.confirm_button = lv_button_create(g_ui.screen);
    lv_obj_remove_style_all(g_ui.confirm_button);
    lv_obj_set_size(g_ui.confirm_button, 128, 28);
    lv_obj_align(g_ui.confirm_button, LV_ALIGN_TOP_MID, 0, 116);
    lv_obj_set_style_radius(g_ui.confirm_button, 6, 0);
    lv_obj_set_style_bg_color(g_ui.confirm_button, lv_color_hex(0x23272e), 0);
    lv_obj_set_style_bg_opa(g_ui.confirm_button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_ui.confirm_button, 0, 0);
    lv_obj_add_event_cb(g_ui.confirm_button, confirm_click_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *confirm_label = create_label(g_ui.confirm_button, "Confirm", &lv_font_montserrat_14);
    lv_obj_center(confirm_label);

    g_ui.status = create_label(g_ui.screen, "Enter user and password", &lv_font_montserrat_12);
    lv_obj_set_style_text_color(g_ui.status, lv_color_hex(0x99a2ad), 0);
    lv_label_set_long_mode(g_ui.status, LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_ui.status, 300);
    lv_obj_align(g_ui.status, LV_ALIGN_BOTTOM_MID, 0, -6);

    g_ui.poll_timer = lv_timer_create(poll_worker_cb, 200, nullptr);
    refresh_ui();
}

} // namespace

void ui_init(void)
{
    build_ui();
}
