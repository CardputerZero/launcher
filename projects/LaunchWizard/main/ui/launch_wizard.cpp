#include "main.h"

#include <stdint.h>

#include "global_config.h"
#include "keyboard_input.h"
#include "lvgl/lvgl.h"

#ifdef __linux__
#include <linux/input.h>
#else
#include "input_keys.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
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

// ---------------------------------------------------------------------------
// Layout / design tokens (CardputerZero OOBE Figma, 320x170 cards)
// ---------------------------------------------------------------------------
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 170;
constexpr uid_t kDefaultUserUid = 1000;
constexpr const char *kFirstBootWizardUser = "rpi-first-boot-wizard";

constexpr uint32_t kColorBg = 0x000000;
constexpr uint32_t kColorDivider = 0x121212;
constexpr uint32_t kColorBrand = 0xffffff;
constexpr uint32_t kColorMuted = 0x8f8f8f;
constexpr uint32_t kColorFieldFocusBg = 0x303030;
constexpr uint32_t kColorFieldBg = 0x151515;
constexpr uint32_t kColorFieldBorder = 0x424242;
constexpr uint32_t kColorRowSelBg = 0x202020;

// Per-screen accent colors (sampled from the Figma design).
constexpr uint32_t kAccentWelcome = 0xff6a2a;   // orange
constexpr uint32_t kAccentRegion = 0x31d843;    // green
constexpr uint32_t kAccentHostname = 0xff2aa3;  // pink
constexpr uint32_t kAccentAccount = 0x2e90ff;   // blue
constexpr uint32_t kAccentNetwork = 0xffd23e;   // yellow
constexpr uint32_t kAccentTime = 0xff5a3c;      // orange-red
constexpr uint32_t kAccentSsh = 0x2ec5ff;       // cyan
constexpr uint32_t kAccentDone = 0x31d843;      // green

enum class Screen {
    Welcome,
    TimezoneList,
    Hostname,
    Account,
    Network,
    WifiList,
    WifiPassword,
    ManualTime,
    Ssh,
    Done,
    Applying,
};

struct Timezone {
    const char *name;
    const char *offset;
};

// Concise list of common international time zones (IANA names), ordered
// west-to-east. Country selection was removed (#111/#98); this is the only
// region step now.
constexpr Timezone kTimezones[] = {
    {"Pacific/Honolulu", "UTC-10"},
    {"America/Los_Angeles", "UTC-8"},
    {"America/Chicago", "UTC-6"},
    {"America/New_York", "UTC-5"},
    {"America/Sao_Paulo", "UTC-3"},
    {"UTC", "UTC+0"},
    {"Europe/London", "UTC+0"},
    {"Europe/Paris", "UTC+1"},
    {"Europe/Moscow", "UTC+3"},
    {"Asia/Dubai", "UTC+4"},
    {"Asia/Kolkata", "UTC+5:30"},
    {"Asia/Bangkok", "UTC+7"},
    {"Asia/Shanghai", "UTC+8"},
    {"Asia/Tokyo", "UTC+9"},
    {"Australia/Sydney", "UTC+10"},
    {"Pacific/Auckland", "UTC+12"},
};

constexpr int kTimezoneCount = static_cast<int>(sizeof(kTimezones) / sizeof(kTimezones[0]));

// Index of the default timezone (Asia/Shanghai) in kTimezones.
constexpr int kDefaultTimezoneIndex = 12;

struct WifiNetwork {
    std::string ssid;
    int signal = 0;  // 0..100
};

struct CommandResult {
    int code = 0;
    std::string output;
};

// ---------------------------------------------------------------------------
// Wizard model (all data collected across screens)
// ---------------------------------------------------------------------------
struct Model {
    Screen screen = Screen::Welcome;

    // Timezone
    int timezone_index = kDefaultTimezoneIndex;
    int timezone_sel = kDefaultTimezoneIndex;

    // Hostname
    std::string hostname = "CardputerZero";

    // Account
    std::string username = "pi";
    std::string password;
    std::string confirm;
    int account_focus = 0;  // 0 user, 1 pass, 2 confirm
    std::string form_error;  // inline validation message (e.g. password mismatch)

    // Network
    int network_focus = 0;  // 0 wifi, 1 ethernet
    bool network_skipped = false;
    bool use_ethernet = false;

    // Wi-Fi
    std::vector<WifiNetwork> wifi_list;
    int wifi_sel = 0;
    std::string wifi_ssid;
    std::string wifi_password;

    // Wi-Fi async scan state (guarded by mutex where noted).
    bool wifi_scanning = false;
    bool wifi_scan_ready = false;
    std::vector<WifiNetwork> wifi_scan_result;

    // Manual time
    std::string manual_date = "2026-06-16";
    std::string manual_time = "20:30";
    int time_focus = 0;  // 0 date, 1 time

    // SSH
    bool ssh_enabled = true;
    int ssh_focus = 0;  // 0 yes, 1 no

    // Apply worker
    bool busy = false;
    bool worker_finished = false;
    bool succeeded = false;
    int exit_ticks = 0;
    std::string worker_message;
    std::mutex mutex;

    lv_obj_t *screen_obj = nullptr;
    lv_timer_t *poll_timer = nullptr;
};

Model g;

const Timezone &current_timezone() { return kTimezones[g.timezone_index]; }

// ---------------------------------------------------------------------------
// Shell command helpers (dry-run aware) -- preserved from the original wizard.
// ---------------------------------------------------------------------------
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
        if (result.output.size() < 4096)
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

std::string trim_copy(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && isspace(static_cast<unsigned char>(value[start])))
        ++start;
    size_t end = value.size();
    while (end > start && isspace(static_cast<unsigned char>(value[end - 1])))
        --end;
    return value.substr(start, end - start);
}

void read_lightdm_autologin_file(const char *path, std::string &autologin_user)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return;

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), fp)) {
        std::string line = trim_copy(buffer);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        size_t comment = line.find_first_of("#;");
        if (comment != std::string::npos)
            line = trim_copy(line.substr(0, comment));
        size_t sep = line.find('=');
        if (sep == std::string::npos)
            continue;
        std::string key = trim_copy(line.substr(0, sep));
        if (key == "autologin-user")
            autologin_user = trim_copy(line.substr(sep + 1));
    }
    fclose(fp);
}

std::string lightdm_autologin_user()
{
#if LAUNCH_WIZARD_DRY_RUN
    return kFirstBootWizardUser;
#else
    std::string autologin_user;
    read_lightdm_autologin_file("/etc/lightdm/lightdm.conf", autologin_user);

    glob_t matches;
    memset(&matches, 0, sizeof(matches));
    if (glob("/etc/lightdm/lightdm.conf.d/*.conf", 0, nullptr, &matches) == 0) {
        for (size_t i = 0; i < matches.gl_pathc; ++i)
            read_lightdm_autologin_file(matches.gl_pathv[i], autologin_user);
    }
    globfree(&matches);
    return autologin_user;
#endif
}

bool piwiz_autostart_enabled()
{
#if LAUNCH_WIZARD_DRY_RUN
    return true;
#else
    FILE *fp = fopen("/etc/xdg/autostart/piwiz.desktop", "r");
    if (!fp)
        return false;

    bool hidden = false;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), fp)) {
        std::string line = trim_copy(buffer);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        size_t comment = line.find_first_of("#;");
        if (comment != std::string::npos)
            line = trim_copy(line.substr(0, comment));
        size_t sep = line.find('=');
        if (sep == std::string::npos)
            continue;
        std::string key = trim_copy(line.substr(0, sep));
        std::string value = trim_copy(line.substr(sep + 1));
        if (key == "Hidden" && value == "true")
            hidden = true;
    }
    fclose(fp);
    return !hidden;
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

// ---------------------------------------------------------------------------
// Validation helpers (preserved).
// ---------------------------------------------------------------------------
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

bool validate_hostname(const std::string &name, std::string &error)
{
    if (name.empty()) {
        error = "Hostname required";
        return false;
    }
    if (name.size() > 63) {
        error = "Hostname too long";
        return false;
    }
    for (char ch : name) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (!(isalnum(c) || ch == '-')) {
            error = "Invalid hostname char";
            return false;
        }
    }
    if (name.front() == '-' || name.back() == '-') {
        error = "Bad hostname dashes";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// System integration -- user/account (preserved).
// ---------------------------------------------------------------------------
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

// Name of the existing UID 1000 user (pi-gen always pre-creates one, e.g. "pi").
std::string current_first_user()
{
#if LAUNCH_WIZARD_DRY_RUN
    return "pi";
#else
    struct passwd *pw = getpwuid(kDefaultUserUid);
    return (pw && pw->pw_name) ? std::string(pw->pw_name) : std::string();
#endif
}

std::string current_first_group()
{
#if LAUNCH_WIZARD_DRY_RUN
    return "pi";
#else
    struct group *gr = getgrgid(kDefaultUserUid);
    return (gr && gr->gr_name) ? std::string(gr->gr_name) : std::string();
#endif
}

// True if the UID 1000 user has a real (unlocked) password set. This mirrors
// pi-gen's own rule ("FIRST_USER_NAME=pi with no FIRST_USER_PASS launches the
// wizard"): a factory image leaves the user --disabled-login (shadow "!"/"*"),
// whereas a configured one (Imager / a baked password / a finished OOBE) stores
// a real "$..." hash.
bool first_user_has_password()
{
#if LAUNCH_WIZARD_DRY_RUN
    return false;
#else
    struct passwd *pw = getpwuid(kDefaultUserUid);
    if (!pw || !pw->pw_name)
        return false;
    struct spwd *sp = getspnam(pw->pw_name);
    if (!sp || !sp->sp_pwdp)
        return false;
    return sp->sp_pwdp[0] == '$';
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

// Provision the owner account. pi-gen always pre-creates the UID 1000 user
// (default "pi", --disabled-login). Mirroring the official userconf tool we
// rename that existing user to the chosen name and set its password instead of
// running useradd (which would collide on UID 1000). Returns the account UID, or
// 0 on failure with `error` populated.
uid_t configure_account(const std::string &new_user, const std::string &password,
                        std::string &error)
{
    std::string old_user = current_first_user();

    if (old_user.empty()) {
        // No pre-created first user (unusual image): create one at UID 1000.
        if (!create_user(new_user, error))
            return 0;
    } else if (old_user != new_user) {
        if (user_exists(new_user)) {
            error = "Target username already in use";
            return 0;
        }
        // Rename the existing UID 1000 user/group (same steps as userconf).
        if (!command_ok({"usermod", "-l", new_user, old_user}, error))
            return 0;
        run_command({"usermod", "-m", "-d", "/home/" + new_user, new_user});
        std::string old_group = current_first_group();
        if (!old_group.empty() && old_group == old_user)
            run_command({"groupmod", "-n", new_user, old_group});
        // Keep subuid/subgid and the nopasswd sudoers entry consistent.
        run_command({"sed", "-i", "s/^" + old_user + ":/" + new_user + ":/",
                     "/etc/subuid"});
        run_command({"sed", "-i", "s/^" + old_user + ":/" + new_user + ":/",
                     "/etc/subgid"});
        run_command({"sed", "-i", "s/^" + old_user + " /" + new_user + " /",
                     "/etc/sudoers.d/010_pi-nopasswd"});
    }

    if (!set_user_password(new_user, password, error))
        return 0;
    return kDefaultUserUid;
}

void disable_piwiz(const std::string &user)
{
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
                 runtime_dir, bus_address,
                 "systemctl", "--user", "daemon-reload"});
    run_command({"runuser", "-u", user, "--", "env",
                 runtime_dir, bus_address,
                 "systemctl", "--user", "enable", "APPLaunch.service"});
    CommandResult restart = run_command({"runuser", "-u", user, "--", "env",
                                         runtime_dir, bus_address,
                                         "systemctl", "--user", "restart", "APPLaunch.service"});
    if (restart.code != 0) {
        CommandResult start = run_command({"runuser", "-u", user, "--", "env",
                                           runtime_dir, bus_address,
                                           "systemctl", "--user", "start", "APPLaunch.service"});
        if (start.code != 0) {
            warning = "APPLaunch start failed";
            if (!start.output.empty())
                warning += ": " + start.output;
        }
    }
    return warning;
}

// ---------------------------------------------------------------------------
// System integration -- new OOBE steps (timezone / hostname / wifi / time / ssh).
// ---------------------------------------------------------------------------
void apply_timezone(const std::string &timezone)
{
    if (timezone.empty())
        return;
    run_command({"timedatectl", "set-timezone", timezone});
}

void apply_hostname(const std::string &hostname)
{
    if (hostname.empty())
        return;
    run_command({"hostnamectl", "set-hostname", hostname});
    const std::string hosts =
        "127.0.0.1\tlocalhost\n"
        "127.0.1.1\t" + hostname + "\n";
    run_command({"tee", "/etc/hostname"}, &hostname);
    run_command({"tee", "/etc/hosts.launchwizard"}, &hosts);
}

void apply_manual_time(const std::string &date, const std::string &time)
{
    if (date.empty() || time.empty())
        return;
    run_command({"timedatectl", "set-ntp", "false"});
    run_command({"timedatectl", "set-time", date + " " + time + ":00"});
}

std::string apply_ssh(bool enabled)
{
    std::string warning;
    if (enabled) {
        run_command({"systemctl", "unmask", "ssh.service"});
        CommandResult ok = run_command({"systemctl", "enable", "--now", "ssh.service"});
        if (ok.code != 0) {
            CommandResult alt = run_command({"systemctl", "enable", "--now", "sshd.service"});
            if (alt.code != 0)
                warning = "SSH enable failed";
        }
    } else {
        run_command({"systemctl", "disable", "--now", "ssh.service"});
        run_command({"systemctl", "disable", "--now", "sshd.service"});
    }
    return warning;
}

std::string apply_wifi(const std::string &ssid, const std::string &password)
{
    if (ssid.empty())
        return "";
    run_command({"nmcli", "radio", "wifi", "on"});
    std::vector<std::string> args = {"nmcli", "device", "wifi", "connect", ssid};
    if (!password.empty()) {
        args.push_back("password");
        args.push_back(password);
    }
    CommandResult result = run_command(args);
    if (result.code != 0) {
        std::string warning = "Wi-Fi connect failed";
        if (!result.output.empty())
            warning += ": " + result.output;
        return warning;
    }
    return "";
}

std::vector<WifiNetwork> scan_wifi()
{
    std::vector<WifiNetwork> networks;
#if LAUNCH_WIZARD_DRY_RUN
    networks.push_back({"Studio_2.4G", 90});
    networks.push_back({"CardputerLab", 70});
    networks.push_back({"Home-5G", 45});
#else
    run_command({"nmcli", "radio", "wifi", "on"});
    run_command({"nmcli", "device", "wifi", "rescan"});
    CommandResult result = run_command(
        {"nmcli", "-t", "-f", "SSID,SIGNAL", "device", "wifi", "list"});
    std::string line;
    for (size_t i = 0; i <= result.output.size(); ++i) {
        char ch = (i < result.output.size()) ? result.output[i] : '\n';
        if (ch == '\n') {
            if (!line.empty()) {
                size_t sep = line.rfind(':');
                std::string ssid = sep == std::string::npos ? line : line.substr(0, sep);
                int signal = 0;
                if (sep != std::string::npos)
                    signal = atoi(line.c_str() + sep + 1);
                bool duplicate = false;
                for (const WifiNetwork &n : networks) {
                    if (n.ssid == ssid) {
                        duplicate = true;
                        break;
                    }
                }
                if (!ssid.empty() && !duplicate && networks.size() < 16)
                    networks.push_back({ssid, signal});
            }
            line.clear();
        } else {
            line.push_back(ch);
        }
    }
#endif
    return networks;
}

// ---------------------------------------------------------------------------
// Full apply (runs on a worker thread when the user confirms on the Done page).
// ---------------------------------------------------------------------------
std::string apply_all()
{
    const std::string timezone = current_timezone().name;
    const std::string hostname = g.hostname;
    const std::string username = g.username;
    const std::string password = g.password.empty() ? std::string("pi") : g.password;
    const bool network_skipped = g.network_skipped;
    const std::string wifi_ssid = g.wifi_ssid;
    const std::string wifi_password = g.wifi_password;
    const std::string manual_date = g.manual_date;
    const std::string manual_time = g.manual_time;
    const bool ssh_enabled = g.ssh_enabled;

#if !LAUNCH_WIZARD_DRY_RUN
    if (geteuid() != 0)
        return "LaunchWizard must run as root";
#endif

    apply_timezone(timezone);
    apply_hostname(hostname);

    if (network_skipped) {
        apply_manual_time(manual_date, manual_time);
    } else if (!g.use_ethernet && !wifi_ssid.empty()) {
        std::string wifi_warning = apply_wifi(wifi_ssid, wifi_password);
        if (!wifi_warning.empty()) {
            printf("LaunchWizard: %s\n", wifi_warning.c_str());
            fflush(stdout);
        }
    }

    apply_ssh(ssh_enabled);

    // Provision the owner account by renaming/repurposing the pre-created
    // UID 1000 user (pi-gen always ships one) and setting its password.
    std::string error;
    uid_t uid = configure_account(username, password, error);
    if (uid == 0)
        return error;

    std::string desktop_warning = configure_desktop_startup(username);
    std::string service_warning = start_applaunch_service(username, uid);
    if (!service_warning.empty())
        return service_warning;

    run_command({"systemctl", "disable", "--now", "LaunchWizard.service"});

    // Consume any "Run Setup Wizard" re-arm marker so the OOBE runs exactly once.
    remove("/var/lib/applaunch/run-oobe");
    remove("/var/lib/LaunchWizard/run-oobe");

    if (!desktop_warning.empty()) {
        printf("LaunchWizard: desktop warning: %s\n", desktop_warning.c_str());
        fflush(stdout);
    }

    // Reboot so autologin starts the desktop/APPLaunch as the freshly renamed
    // user with a clean session (matches Raspberry Pi's first-boot behaviour).
    printf("LaunchWizard: OOBE complete, rebooting\n");
    fflush(stdout);
    run_command({"systemctl", "reboot"});
    return "";
}

// ===========================================================================
// UI primitives
// ===========================================================================
const lv_font_t *font_xs() { return &lv_font_montserrat_10; }   // 8-9px
const lv_font_t *font_sm() { return &lv_font_montserrat_12; }   // 10-12px
const lv_font_t *font_md() { return &lv_font_montserrat_14; }   // 14px
const lv_font_t *font_lg() { return &lv_font_montserrat_16; }   // 16-17px
const lv_font_t *font_xl() { return &lv_font_montserrat_22; }   // big title

lv_obj_t *add_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                    uint32_t color, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
    return label;
}

lv_obj_t *add_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg,
                   int border_w, uint32_t border_color, int radius, lv_opa_t bg_opa = LV_OPA_COVER)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(box, w, h);
    lv_obj_align(box, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_radius(box, radius, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(box, bg_opa, 0);
    if (border_w > 0) {
        lv_obj_set_style_border_color(box, lv_color_hex(border_color), 0);
        lv_obj_set_style_border_width(box, border_w, 0);
    }
    return box;
}

// SETUP mode tag + CardputerZero brand + top divider + progress bar.
void add_chrome(uint32_t accent, int progress_fill, bool show_progress = true)
{
    lv_obj_t *p = g.screen_obj;

    // SETUP mode tag. (Figma shows a parallelogram, but the device's software
    // renderer does not draw skew-transformed rects, so use a rounded rect.)
    lv_obj_t *tag = add_rect(p, 12, 6, 82, 18, accent, 0, 0, 3);
    add_label(tag, "SETUP", font_xs(), 0xffffff, 16, 4);

    add_label(p, "CardputerZero", font_sm(), kColorBrand, 120, 7);
    add_rect(p, 8, 33, 304, 1, kColorDivider, 0, 0, 0);

    if (show_progress) {
        add_rect(p, 228, 13, 80, 4, accent, 0, 0, 2, LV_OPA_30);
        if (progress_fill < 4)
            progress_fill = 4;
        if (progress_fill > 80)
            progress_fill = 80;
        add_rect(p, 228, 13, progress_fill, 4, accent, 0, 0, 2);
    }
}

// One key hint pair, e.g. "ESC BACK".
void add_key_hint(int key_x, const char *key, int hint_x, const char *hint, uint32_t accent)
{
    add_label(g.screen_obj, key, font_xs(), accent, key_x, 152);
    add_label(g.screen_obj, hint, font_xs(), 0xffffff, hint_x, 152);
}

// A labeled value field (focused vs unfocused styling matches the Figma).
lv_obj_t *add_field(lv_obj_t *parent, int x, int y, int w, int h, bool focused,
                    uint32_t accent)
{
    if (focused)
        return add_rect(parent, x, y, w, h, kColorFieldFocusBg, 2, accent, 3);
    return add_rect(parent, x, y, w, h, kColorFieldBg, 1, kColorFieldBorder, 3);
}

std::string masked(const std::string &s) { return std::string(s.size(), '*'); }

// ===========================================================================
// Screen renderers
// ===========================================================================
void render_welcome()
{
    add_chrome(kAccentWelcome, 8);
    add_label(g.screen_obj, "First Setup", font_xl(), 0xffffff, 34, 50);
    add_label(g.screen_obj, "Set up CardputerZero in a few steps.", font_sm(),
              kColorMuted, 36, 100);
    add_key_hint(132, "OK", 156, "START", kAccentWelcome);
}

// Generic single-column list (timezone / wifi).
void render_list(const char *title, uint32_t accent,
                 const std::vector<std::string> &left,
                 const std::vector<std::string> &right, int sel,
                 const char *ok_hint)
{
    add_chrome(accent, 24);
    add_label(g.screen_obj, title, font_sm(), accent, 36, 40);

    const int count = static_cast<int>(left.size());
    const int visible = 4;
    int start = sel - 1;
    if (start < 0)
        start = 0;
    if (start > count - visible)
        start = count - visible;
    if (start < 0)
        start = 0;

    const int row_y0 = 60;
    const int row_h = 24;
    for (int i = 0; i < visible && start + i < count; ++i) {
        int idx = start + i;
        int y = row_y0 + i * row_h;
        bool is_sel = idx == sel;
        if (is_sel) {
            add_rect(g.screen_obj, 30, y - 1, 260, 22, kColorRowSelBg, 0, 0, 4);
            add_label(g.screen_obj, ">", font_md(), accent, 42, y + 1);
        }
        add_label(g.screen_obj, left[idx].c_str(), font_md(),
                  is_sel ? 0xffffff : kColorMuted, 66, y + 2);
        if (idx < static_cast<int>(right.size()) && !right[idx].empty()) {
            add_label(g.screen_obj, right[idx].c_str(), font_sm(),
                      is_sel ? 0xffffff : kColorMuted, 240, y + 3);
        }
    }

    add_key_hint(14, "ESC", 38, "BACK", accent);
    add_key_hint(132, "OK", 156, ok_hint, accent);
}

void render_timezone_list()
{
    std::vector<std::string> left, right;
    for (const Timezone &t : kTimezones) {
        left.emplace_back(t.name);
        right.emplace_back(t.offset);
    }
    render_list("TIMEZONE", kAccentRegion, left, right, g.timezone_sel, "CONFIRM");
}

void render_hostname()
{
    add_chrome(kAccentHostname, 36);
    add_label(g.screen_obj, "HOSTNAME", font_sm(), kAccentHostname, 36, 47);
    add_field(g.screen_obj, 36, 66, 248, 31, true, kAccentHostname);
    std::string value = g.hostname + "|";
    add_label(g.screen_obj, value.c_str(), font_lg(), 0xffffff, 44, 73);
    add_label(g.screen_obj, "Default: CardputerZero", font_sm(), kColorMuted, 38, 104);

    add_key_hint(14, "ESC", 38, "BACK", kAccentHostname);
    add_key_hint(132, "OK", 156, "CONFIRM", kAccentHostname);
}

void render_account()
{
    add_chrome(kAccentAccount, 48);

    add_label(g.screen_obj, "USERNAME", font_xs(), kAccentAccount, 37, 51);
    add_field(g.screen_obj, 36, 67, 92, 24, g.account_focus == 0, kAccentAccount);
    std::string user_value = g.username + (g.account_focus == 0 ? "|" : "");
    add_label(g.screen_obj, user_value.c_str(), font_md(), 0xffffff, 44, 70);

    // #80: the focused password field is shown in plaintext; once focus moves
    // away it reverts to masked. Only one field can be focused at a time.
    add_label(g.screen_obj, "PASSWORD", font_xs(), kAccentAccount, 143, 51);
    add_field(g.screen_obj, 142, 67, 142, 24, g.account_focus == 1, kAccentAccount);
    std::string pass_value = (g.account_focus == 1 ? g.password + "|" : masked(g.password));
    add_label(g.screen_obj, pass_value.c_str(), font_md(), 0xffffff, 150, 70);

    add_label(g.screen_obj, "CONFIRM PASSWORD", font_xs(), kAccentAccount, 143, 98);
    add_field(g.screen_obj, 142, 110, 142, 24, g.account_focus == 2, kAccentAccount);
    std::string confirm_value = (g.account_focus == 2 ? g.confirm + "|" : masked(g.confirm));
    add_label(g.screen_obj, confirm_value.c_str(), font_md(), 0xffffff, 150, 113);

    // #99: surface an inline error (e.g. password mismatch) instead of the
    // default hint so the user knows why NEXT/CONFIRM was blocked.
    if (!g.form_error.empty())
        add_label(g.screen_obj, g.form_error.c_str(), font_sm(), 0xff5a5a, 36, 113);
    else
        add_label(g.screen_obj, "Default: pi / pi", font_sm(), kColorMuted, 36, 113);

    add_key_hint(14, "ESC", 38, "BACK", kAccentAccount);
    add_key_hint(132, "OK", 156, "CONFIRM", kAccentAccount);
    add_key_hint(244, "TAB", 268, "NEXT", kAccentAccount);
}

void render_pill(lv_obj_t *parent, int x, int y, int w, const char *label,
                 bool focused, uint32_t accent)
{
    add_field(parent, x, y, w, 22, focused, accent);
    add_label(parent, label, font_sm(), focused ? 0xffffff : kColorMuted, x + 8, y + 3);
}

void render_network()
{
    add_chrome(kAccentNetwork, 60);
    add_label(g.screen_obj, "NETWORK", font_sm(), kAccentNetwork, 36, 38);
    render_pill(g.screen_obj, 36, 64, 132, "Wi-Fi", g.network_focus == 0, kAccentNetwork);
    render_pill(g.screen_obj, 36, 92, 132, "Ethernet", g.network_focus == 1, kAccentNetwork);

    add_key_hint(14, "ESC", 38, "BACK", kAccentNetwork);
    add_key_hint(132, "OK", 156, "CONFIRM", kAccentNetwork);
    add_key_hint(244, "TAB", 268, "SKIP", kAccentNetwork);
}

void draw_signal_bars(lv_obj_t *parent, int x, int y, int signal, uint32_t accent)
{
    // 4 ascending bars; lit count based on signal strength.
    int lit = signal >= 75 ? 4 : signal >= 50 ? 3 : signal >= 25 ? 2 : 1;
    for (int i = 0; i < 4; ++i) {
        int bar_h = 4 + i * 4;
        int bar_x = x + i * 7;
        int bar_y = y + (16 - bar_h);
        uint32_t color = i < lit ? accent : kColorFieldBorder;
        add_rect(parent, bar_x, bar_y, 4, bar_h, color, 0, 0, 1);
    }
}

void render_wifi_list()
{
    add_chrome(kAccentNetwork, 60);
    add_label(g.screen_obj, "SELECT WI-FI", font_sm(), kAccentNetwork, 36, 40);

    // #94: while the first scan is still running show a loading state with a
    // spinner so the user isn't staring at an empty/"Other network..." list.
    if (g.wifi_scanning && g.wifi_list.empty()) {
        lv_obj_t *spinner = lv_spinner_create(g.screen_obj);
        lv_obj_set_size(spinner, 28, 28);
        lv_spinner_set_anim_params(spinner, 1000, 60);
        lv_obj_align(spinner, LV_ALIGN_TOP_LEFT, 40, 78);
        lv_obj_set_style_arc_color(spinner, lv_color_hex(kColorFieldBorder), LV_PART_MAIN);
        lv_obj_set_style_arc_color(spinner, lv_color_hex(kAccentNetwork), LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(spinner, 4, LV_PART_MAIN);
        lv_obj_set_style_arc_width(spinner, 4, LV_PART_INDICATOR);
        add_label(g.screen_obj, "Scanning for networks...", font_sm(), kColorMuted, 80, 86);
        add_key_hint(14, "ESC", 38, "BACK", kAccentNetwork);
        return;
    }

    const int count = static_cast<int>(g.wifi_list.size()) + 1;  // + manual entry
    const int visible = 4;
    int start = g.wifi_sel - 1;
    if (start < 0)
        start = 0;
    if (start > count - visible)
        start = count - visible;
    if (start < 0)
        start = 0;

    const int row_y0 = 57;
    const int row_h = 23;
    for (int i = 0; i < visible && start + i < count; ++i) {
        int idx = start + i;
        int y = row_y0 + i * row_h;
        bool is_sel = idx == g.wifi_sel;
        if (is_sel) {
            add_rect(g.screen_obj, 30, y, 260, 21, kColorRowSelBg, 0, 0, 4);
            add_label(g.screen_obj, ">", font_md(), kAccentNetwork, 42, y + 1);
        }
        if (idx < static_cast<int>(g.wifi_list.size())) {
            add_label(g.screen_obj, g.wifi_list[idx].ssid.c_str(), font_sm(),
                      is_sel ? 0xffffff : kColorMuted, 66, y + 4);
            draw_signal_bars(g.screen_obj, 252, y + 1, g.wifi_list[idx].signal, kAccentNetwork);
        } else {
            add_label(g.screen_obj, "Other network...", font_sm(),
                      is_sel ? 0xffffff : kColorMuted, 66, y + 4);
        }
    }

    add_key_hint(14, "ESC", 38, "BACK", kAccentNetwork);
    add_key_hint(132, "OK", 156, "SELECT", kAccentNetwork);
}

void render_wifi_password()
{
    add_chrome(kAccentNetwork, 60);
    add_label(g.screen_obj, "WI-FI PASSWORD", font_sm(), kAccentNetwork, 36, 47);
    add_field(g.screen_obj, 36, 66, 248, 31, true, kAccentNetwork);
    std::string value = masked(g.wifi_password) + "|";
    add_label(g.screen_obj, value.c_str(), font_lg(), 0xffffff, 44, 73);
    std::string note = "SSID: " + g.wifi_ssid;
    add_label(g.screen_obj, note.c_str(), font_sm(), kColorMuted, 38, 104);

    add_key_hint(14, "ESC", 38, "BACK", kAccentNetwork);
    add_key_hint(132, "OK", 156, "CONFIRM", kAccentNetwork);
    add_key_hint(244, "TAB", 268, "NEXT", kAccentNetwork);
}

void render_manual_time()
{
    add_chrome(kAccentTime, 60);
    add_label(g.screen_obj, "MANUAL TIME", font_sm(), kAccentTime, 36, 41);
    add_label(g.screen_obj, "Set time manually because", font_sm(), kColorMuted, 36, 57);
    add_label(g.screen_obj, "there is no network connection.", font_sm(), kColorMuted, 36, 69);

    add_field(g.screen_obj, 36, 89, 126, 26, g.time_focus == 0, kAccentTime);
    std::string date_value = g.manual_date + (g.time_focus == 0 ? "|" : "");
    add_label(g.screen_obj, date_value.c_str(), font_md(), 0xffffff, 44, 93);

    add_field(g.screen_obj, 174, 89, 84, 26, g.time_focus == 1, kAccentTime);
    std::string time_value = g.manual_time + (g.time_focus == 1 ? "|" : "");
    add_label(g.screen_obj, time_value.c_str(), font_md(), 0xffffff, 182, 93);

    add_key_hint(14, "ESC", 38, "BACK", kAccentTime);
    add_key_hint(132, "OK", 156, "CONFIRM", kAccentTime);
    add_key_hint(244, "TAB", 268, "NEXT", kAccentTime);
}

void render_ssh()
{
    add_chrome(kAccentSsh, 70);
    add_label(g.screen_obj, "Enable SSH", font_sm(), kAccentSsh, 36, 41);
    render_pill(g.screen_obj, 36, 85, 70, "YES", g.ssh_focus == 0, kAccentSsh);
    render_pill(g.screen_obj, 118, 85, 70, "NO", g.ssh_focus == 1, kAccentSsh);

    add_key_hint(14, "ESC", 38, "BACK", kAccentSsh);
    add_key_hint(132, "OK", 156, "CONFIRM", kAccentSsh);
    add_key_hint(244, "TAB", 268, "NEXT", kAccentSsh);
}

void render_done()
{
    add_chrome(kAccentDone, 80);
    add_label(g.screen_obj, "Setup", font_xl(), 0xffffff, 36, 48);
    add_label(g.screen_obj, "finished!", font_xl(), 0xffffff, 36, 80);

    add_key_hint(14, "ESC", 38, "BACK", kAccentDone);
    add_key_hint(132, "OK", 156, "START", kAccentDone);
}

void render_applying()
{
    add_chrome(kAccentDone, 80, false);
    std::string message;
    {
        std::lock_guard<std::mutex> lock(g.mutex);
        message = g.worker_message.empty() ? "Configuring..." : g.worker_message;
    }
    add_label(g.screen_obj, "Applying setup", font_lg(), 0xffffff, 36, 62);
    lv_obj_t *msg = add_label(g.screen_obj, message.c_str(), font_sm(), kColorMuted, 36, 92);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_DOT);
    lv_obj_set_width(msg, 248);
}

void render()
{
    lv_obj_clean(g.screen_obj);
    switch (g.screen) {
    case Screen::Welcome: render_welcome(); break;
    case Screen::TimezoneList: render_timezone_list(); break;
    case Screen::Hostname: render_hostname(); break;
    case Screen::Account: render_account(); break;
    case Screen::Network: render_network(); break;
    case Screen::WifiList: render_wifi_list(); break;
    case Screen::WifiPassword: render_wifi_password(); break;
    case Screen::ManualTime: render_manual_time(); break;
    case Screen::Ssh: render_ssh(); break;
    case Screen::Done: render_done(); break;
    case Screen::Applying: render_applying(); break;
    }
}

void go(Screen screen)
{
    g.screen = screen;
    render();
}

// ===========================================================================
// Apply orchestration
// ===========================================================================
void start_apply()
{
    if (g.busy)
        return;
    g.busy = true;
    {
        std::lock_guard<std::mutex> lock(g.mutex);
        g.worker_message = "Configuring...";
    }
    go(Screen::Applying);

    std::thread([]() {
        std::string message = apply_all();
        std::lock_guard<std::mutex> lock(g.mutex);
        g.worker_message = message.empty() ? "Done. Starting APPLaunch..." : message;
        g.worker_finished = true;
        g.succeeded = message.empty();
    }).detach();
}

// ===========================================================================
// Input handling
// ===========================================================================
std::string *active_text_field()
{
    switch (g.screen) {
    case Screen::Hostname:
        return &g.hostname;
    case Screen::Account:
        return g.account_focus == 0 ? &g.username
             : g.account_focus == 1 ? &g.password
                                    : &g.confirm;
    case Screen::WifiPassword:
        return &g.wifi_password;
    case Screen::ManualTime:
        return g.time_focus == 0 ? &g.manual_date : &g.manual_time;
    default:
        return nullptr;
    }
}

void handle_text_char(char ch)
{
    std::string *field = active_text_field();
    if (!field)
        return;
    if (field->size() >= 63)
        return;
    field->push_back(ch);
    g.form_error.clear();
    render();
}

void handle_backspace()
{
    std::string *field = active_text_field();
    if (field && !field->empty()) {
        field->pop_back();
        g.form_error.clear();
        render();
    }
}

void enter_wifi_list()
{
    // #94: scan asynchronously so the list screen appears immediately with a
    // loading spinner and refreshes as soon as results arrive (poll_worker_cb).
    g.wifi_list.clear();
    g.wifi_sel = 0;
    {
        std::lock_guard<std::mutex> lock(g.mutex);
        g.wifi_scan_ready = false;
        g.wifi_scan_result.clear();
    }
    g.wifi_scanning = true;
    go(Screen::WifiList);

    std::thread([]() {
        std::vector<WifiNetwork> networks = scan_wifi();
        std::lock_guard<std::mutex> lock(g.mutex);
        g.wifi_scan_result = std::move(networks);
        g.wifi_scan_ready = true;
    }).detach();
}

// ESC navigation per screen.
void handle_back()
{
    switch (g.screen) {
    case Screen::Welcome: break;
    case Screen::TimezoneList: go(Screen::Welcome); break;
    case Screen::Hostname: go(Screen::TimezoneList); break;
    case Screen::Account: go(Screen::Hostname); break;
    case Screen::Network: go(Screen::Account); break;
    case Screen::WifiList: go(Screen::Network); break;
    case Screen::WifiPassword: go(Screen::WifiList); break;
    case Screen::ManualTime: go(Screen::Network); break;
    case Screen::Ssh: go(g.network_skipped ? Screen::ManualTime : Screen::Network); break;
    case Screen::Done: go(Screen::Ssh); break;
    case Screen::Applying: break;
    }
}

void move_focus(int delta)
{
    switch (g.screen) {
    case Screen::TimezoneList: {
        int n = kTimezoneCount;
        g.timezone_sel = (g.timezone_sel + delta + n) % n;
        render();
        break;
    }
    case Screen::Account:
        g.account_focus = (g.account_focus + delta + 3) % 3;
        g.form_error.clear();
        render();
        break;
    case Screen::Network:
        g.network_focus = (g.network_focus + delta + 2) % 2;
        render();
        break;
    case Screen::WifiList: {
        int n = static_cast<int>(g.wifi_list.size()) + 1;
        g.wifi_sel = (g.wifi_sel + delta + n) % n;
        render();
        break;
    }
    case Screen::ManualTime:
        g.time_focus = (g.time_focus + delta + 2) % 2;
        render();
        break;
    case Screen::Ssh:
        g.ssh_focus = (g.ssh_focus + delta + 2) % 2;
        render();
        break;
    default:
        break;
    }
}

void handle_enter()
{
    switch (g.screen) {
    case Screen::Welcome:
        g.timezone_sel = g.timezone_index;
        go(Screen::TimezoneList);
        break;
    case Screen::TimezoneList:
        g.timezone_index = g.timezone_sel;
        go(Screen::Hostname);
        break;
    case Screen::Hostname: {
        std::string error;
        if (g.hostname.empty())
            g.hostname = "CardputerZero";
        if (!validate_hostname(g.hostname, error))
            return;
        go(Screen::Account);
        break;
    }
    case Screen::Account:
        if (g.account_focus < 2) {
            ++g.account_focus;
            g.form_error.clear();
            render();
        } else {
            std::string error;
            if (g.username.empty())
                g.username = "pi";
            if (g.password.empty() && g.confirm.empty()) {
                g.password = "pi";
                g.confirm = "pi";
            }
            if (!validate_username(g.username, error)) {
                g.form_error = error;
                render();
                return;
            }
            if (!validate_password(g.password, error)) {
                g.form_error = error;
                render();
                return;
            }
            // #99: block progression and surface a clear message when the two
            // password entries differ, instead of silently doing nothing.
            if (g.password != g.confirm) {
                g.form_error = "Passwords do not match";
                render();
                return;
            }
            g.form_error.clear();
            go(Screen::Network);
        }
        break;
    case Screen::Network:
        if (g.network_focus == 0) {
            g.use_ethernet = false;
            g.network_skipped = false;
            enter_wifi_list();
        } else {
            g.use_ethernet = true;
            g.network_skipped = false;
            go(Screen::Ssh);
        }
        break;
    case Screen::WifiList:
        if (g.wifi_sel < static_cast<int>(g.wifi_list.size())) {
            g.wifi_ssid = g.wifi_list[g.wifi_sel].ssid;
        } else {
            g.wifi_ssid = "";  // Manual entry; SSID can be edited later.
        }
        g.wifi_password.clear();
        go(Screen::WifiPassword);
        break;
    case Screen::WifiPassword:
        go(Screen::Ssh);
        break;
    case Screen::ManualTime:
        go(Screen::Ssh);
        break;
    case Screen::Ssh:
        g.ssh_enabled = g.ssh_focus == 0;
        go(Screen::Done);
        break;
    case Screen::Done:
        start_apply();
        break;
    case Screen::Applying:
        break;
    }
}

// TAB: NEXT / SKIP depending on the screen.
void handle_tab()
{
    switch (g.screen) {
    case Screen::Account:
        if (g.username.empty())
            g.username = "pi";
        if (g.password.empty() && g.confirm.empty()) {
            g.password = "pi";
            g.confirm = "pi";
        }
        // #99: mismatched passwords must block skipping ahead too.
        if (g.password != g.confirm) {
            g.form_error = "Passwords do not match";
            render();
            return;
        }
        g.form_error.clear();
        go(Screen::Network);
        break;
    case Screen::Network:
        g.network_skipped = true;
        g.use_ethernet = false;
        go(Screen::ManualTime);
        break;
    case Screen::WifiPassword:
        go(Screen::Ssh);
        break;
    case Screen::ManualTime:
        go(Screen::Ssh);
        break;
    case Screen::Ssh:
        go(Screen::Done);
        break;
    default:
        break;
    }
}

void keyboard_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD))
        return;

    auto *key = static_cast<key_item *>(lv_event_get_param(event));
    if (!key || key->key_state == KBD_KEY_RELEASED)
        return;
    if (g.busy)
        return;

    switch (key->key_code) {
    case KEY_ESC:
        handle_back();
        return;
    case KEY_TAB:
        handle_tab();
        return;
    case KEY_UP:
    case KEY_LEFT:
        move_focus(-1);
        return;
    case KEY_DOWN:
    case KEY_RIGHT:
        move_focus(1);
        return;
    case KEY_ENTER:
        handle_enter();
        return;
    case KEY_BACKSPACE:
    case KEY_DELETE:
        handle_backspace();
        return;
    default:
        break;
    }

    if (key->utf8[0] != '\0' && key->utf8[1] == '\0') {
        unsigned char ch = static_cast<unsigned char>(key->utf8[0]);
        if (ch >= 0x20 && ch < 0x7f)
            handle_text_char(static_cast<char>(ch));
    }
}

void poll_worker_cb(lv_timer_t *timer)
{
    (void)timer;

    // #94: pull in async Wi-Fi scan results and refresh the list live.
    bool wifi_updated = false;
    {
        std::lock_guard<std::mutex> lock(g.mutex);
        if (g.wifi_scan_ready) {
            g.wifi_scan_ready = false;
            g.wifi_list = std::move(g.wifi_scan_result);
            g.wifi_scan_result.clear();
            g.wifi_scanning = false;
            wifi_updated = true;
        }
    }
    if (wifi_updated && g.screen == Screen::WifiList) {
        if (g.wifi_sel >= static_cast<int>(g.wifi_list.size()) + 1)
            g.wifi_sel = 0;
        render();
    }

    bool finished = false;
    bool succeeded = false;
    {
        std::lock_guard<std::mutex> lock(g.mutex);
        if (g.worker_finished) {
            g.worker_finished = false;
            finished = true;
            succeeded = g.succeeded;
        }
    }

    if (finished) {
        g.busy = false;
        render();  // refresh the applying screen with the final message
        if (!succeeded) {
            // Allow the user to go back and fix the error.
            g.busy = false;
        }
    }

    if (!g.busy && g.succeeded && g.screen == Screen::Applying) {
        ++g.exit_ticks;
        if (g.exit_ticks > 15)
            exit(0);
    }
}

void build_ui()
{
    g.screen_obj = lv_screen_active();
    lv_obj_remove_style_all(g.screen_obj);
    lv_obj_clear_flag(g.screen_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g.screen_obj, kScreenWidth, kScreenHeight);
    lv_obj_set_style_bg_color(g.screen_obj, lv_color_hex(kColorBg), 0);
    lv_obj_set_style_bg_opa(g.screen_obj, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(g.screen_obj, keyboard_event_cb,
                        static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), nullptr);

    g.poll_timer = lv_timer_create(poll_worker_cb, 200, nullptr);
    render();
}

}  // namespace

// ---------------------------------------------------------------------------
// First-boot detection.
//
// Use Raspberry Pi OS's own desktop handoff as the first-boot signal. The
// APPLaunch settings can explicitly re-arm the OOBE with a marker file. Without
// that marker, the native piwiz flow on current images is launched by XDG
// autostart after LightDM logs in rpi-first-boot-wizard, so LaunchWizard only
// replaces that exact flow. Otherwise the owner account is already expected to
// exist, so LaunchWizard skips the OOBE and only starts APPLaunch for that user.
// ---------------------------------------------------------------------------
bool launch_wizard_should_run(void)
{
#if LAUNCH_WIZARD_DRY_RUN
    // In the SDL emulator always show the OOBE so it can be developed/previewed.
    return true;
#else
    // Explicit re-arm marker. APPLaunch's "Run Setup Wizard" settings entry
    // drops this file and reboots, letting an already-configured device replay
    // the OOBE on demand. apply_all() clears it on completion, so the wizard
    // still runs exactly once.
    static const char *kRearmPaths[] = {
        "/var/lib/applaunch/run-oobe",
        "/var/lib/LaunchWizard/run-oobe",
    };
    for (const char *path : kRearmPaths) {
        if (access(path, F_OK) == 0)
            return true;
    }

    return lightdm_autologin_user() == kFirstBootWizardUser && piwiz_autostart_enabled();
#endif
}

int launch_wizard_finish_configured_system(void)
{
#if !LAUNCH_WIZARD_DRY_RUN
    if (geteuid() != 0) {
        fprintf(stderr, "LaunchWizard: configured-system finish must run as root\n");
        return 1;
    }
#endif

    std::string user = current_first_user();
    if (user.empty() || user == "root") {
        fprintf(stderr, "LaunchWizard: UID 1000 user not found; cannot start APPLaunch\n");
        return 1;
    }

    std::string service_warning = start_applaunch_service(user, kDefaultUserUid);
    if (!service_warning.empty()) {
        fprintf(stderr, "LaunchWizard: %s\n", service_warning.c_str());
        return 1;
    }

    run_command({"systemctl", "disable", "--now", "LaunchWizard.service"});
    printf("LaunchWizard: started APPLaunch for %s and disabled LaunchWizard.service\n",
           user.c_str());
    fflush(stdout);
    return 0;
}

void ui_init(void)
{
    build_ui();
}
