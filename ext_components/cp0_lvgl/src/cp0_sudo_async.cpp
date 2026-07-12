#include "cp0_lvgl_app.h"
#include "compat/input_keys.h"
#include "hal_lvgl_bsp.h"
#include "keyboard_input.h"

#include "lvgl/lvgl.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

constexpr int kMaxAuthAttempts = 3;
constexpr size_t kMaxPasswordBytes = 128;

struct SudoRequest {
    std::vector<std::string> argv;
    bool use_login_shell = false;
    cp0_sudo_callback_thread_t callback_thread = CP0_SUDO_CALLBACK_LVGL;
    cp0_sudo_output_cb_t output_cb = nullptr;
    cp0_sudo_complete_cb_t complete_cb = nullptr;
    void *user = nullptr;
    int auth_attempts = 0;
    uint64_t id = 0;
    int auth_timeout_ms = 0;
    int exec_timeout_ms = 0;
    std::atomic<bool> cancel_requested{false};
};

struct UserIdentity {
    std::string name;
    std::string home;
    std::string shell;
#if !defined(_WIN32)
    uid_t uid = static_cast<uid_t>(-1);
    gid_t gid = static_cast<gid_t>(-1);
#endif
};

struct ProcessResult {
    int exit_code = -1;
    std::string output;
};

struct CompletionEvent {
    std::shared_ptr<SudoRequest> request;
    cp0_sudo_result_t result = CP0_SUDO_RESULT_EXEC_FAILED;
    int exit_code = -1;
};

struct OutputEvent {
    std::shared_ptr<SudoRequest> request;
    std::string data;
};

std::mutex g_mutex;
std::deque<std::shared_ptr<SudoRequest>> g_queue;
std::unordered_map<uint64_t, std::shared_ptr<SudoRequest>> g_requests;
std::deque<std::unique_ptr<CompletionEvent>> g_completions;
std::shared_ptr<SudoRequest> g_current;
lv_obj_t *g_overlay = nullptr;
lv_obj_t *g_box = nullptr;
lv_obj_t *g_password_label = nullptr;
lv_obj_t *g_hint_label = nullptr;
lv_obj_t *g_key_hook = nullptr;
lv_group_t *g_saved_group = nullptr;
lv_group_t *g_prompt_group = nullptr;
lv_timer_t *g_completion_timer = nullptr;
std::string g_password;
bool g_running = false;
std::atomic<uint64_t> g_next_request_id{1};

void start_next_request();
void key_event_cb(lv_event_t *event);

std::string config_get_string(const char *key)
{
    std::string value;
    cp0_signal_config_api({"GetStr", key ? std::string(key) : std::string(), value},
                          [&](int code, std::string data) {
                              if (code == 0)
                                  value = std::move(data);
                          });
    return value;
}

#if !defined(_WIN32)
bool is_nologin_shell(const char *shell)
{
    return !shell || !shell[0] || std::strstr(shell, "nologin") || std::strstr(shell, "/bin/false");
}

bool resolve_run_user(UserIdentity &identity)
{
    std::string configured;
    if (geteuid() == 0)
        configured = config_get_string("run_as_user");
    else {
        struct passwd *current = getpwuid(geteuid());
        if (current && current->pw_name)
            configured = current->pw_name;
    }
    struct passwd *pwd = nullptr;

    if (!configured.empty()) {
        pwd = getpwnam(configured.c_str());
    } else {
        std::string fallback_name;
        setpwent();
        while ((pwd = getpwent()) != nullptr) {
            if (pwd->pw_uid >= 1000 && pwd->pw_uid < 65534 && !is_nologin_shell(pwd->pw_shell)) {
                fallback_name = pwd->pw_name ? pwd->pw_name : "";
                break;
            }
        }
        endpwent();
        pwd = getpwnam(fallback_name.empty() ? "pi" : fallback_name.c_str());
    }

    if (!pwd || pwd->pw_uid == 0 || is_nologin_shell(pwd->pw_shell))
        return false;

    identity.name = pwd->pw_name ? pwd->pw_name : "";
    identity.home = pwd->pw_dir && pwd->pw_dir[0] ? pwd->pw_dir : "/";
    identity.shell = pwd->pw_shell && pwd->pw_shell[0] ? pwd->pw_shell : "/bin/sh";
    identity.uid = pwd->pw_uid;
    identity.gid = pwd->pw_gid;
    return !identity.name.empty();
}

bool drop_to_user(const UserIdentity &identity)
{
    if (geteuid() != 0)
        return true;
    if (identity.name.empty())
        return true;
    if (initgroups(identity.name.c_str(), identity.gid) != 0)
        return false;
    if (setgid(identity.gid) != 0)
        return false;
    if (setuid(identity.uid) != 0)
        return false;
    setenv("HOME", identity.home.c_str(), 1);
    setenv("USER", identity.name.c_str(), 1);
    setenv("LOGNAME", identity.name.c_str(), 1);
    setenv("SHELL", identity.shell.c_str(), 1);
    setenv("LC_ALL", "C", 1);
    return true;
}

std::vector<char *> raw_argv(std::vector<std::string> &argv)
{
    std::vector<char *> raw;
    raw.reserve(argv.size() + 1);
    for (std::string &arg : argv)
        raw.push_back(arg.data());
    raw.push_back(nullptr);
    return raw;
}

ProcessResult run_process(const UserIdentity &identity,
                          std::vector<std::string> argv,
                          const std::string *stdin_text,
                          const std::function<void(const char *, size_t)> &output_callback = {},
                          const std::shared_ptr<SudoRequest> &request = {},
                          int timeout_ms = 0)
{
    ProcessResult result;
    int stdin_pipe[2] = {-1, -1};
    int output_pipe[2] = {-1, -1};
    if (pipe(output_pipe) != 0) {
        result.exit_code = -errno;
        return result;
    }
    if (stdin_text && pipe(stdin_pipe) != 0) {
        result.exit_code = -errno;
        close(output_pipe[0]);
        close(output_pipe[1]);
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        result.exit_code = -errno;
        close(output_pipe[0]);
        close(output_pipe[1]);
        if (stdin_text) {
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
        }
        return result;
    }

    if (pid == 0) {
        setpgid(0, 0);
        if (stdin_text) {
            close(stdin_pipe[1]);
            dup2(stdin_pipe[0], STDIN_FILENO);
        } else {
            int devnull = open("/dev/null", O_RDONLY);
            if (devnull >= 0) {
                dup2(devnull, STDIN_FILENO);
                if (devnull > STDIN_FILENO)
                    close(devnull);
            }
        }
        close(output_pipe[0]);
        dup2(output_pipe[1], STDOUT_FILENO);
        dup2(output_pipe[1], STDERR_FILENO);
        if (output_pipe[1] > STDERR_FILENO)
            close(output_pipe[1]);
        if (stdin_text && stdin_pipe[0] > STDIN_FILENO)
            close(stdin_pipe[0]);
        setenv("LC_ALL", "C", 1);
        if (!drop_to_user(identity))
            _exit(126);
        auto raw = raw_argv(argv);
        execvp(raw[0], raw.data());
        _exit(127);
    }

    close(output_pipe[1]);
    if (stdin_text) {
        close(stdin_pipe[0]);
        sigset_t blocked_set, old_set, pending_set;
        sigemptyset(&blocked_set);
        sigaddset(&blocked_set, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &blocked_set, &old_set);
        sigpending(&pending_set);
        const bool sigpipe_was_pending = sigismember(&pending_set, SIGPIPE);
        const char *data = stdin_text->data();
        size_t remaining = stdin_text->size();
        while (remaining > 0) {
            ssize_t written = write(stdin_pipe[1], data, remaining);
            if (written < 0) {
                if (errno == EINTR)
                    continue;
                break;
            }
            data += written;
            remaining -= static_cast<size_t>(written);
        }
        close(stdin_pipe[1]);
        sigpending(&pending_set);
        if (!sigpipe_was_pending && sigismember(&pending_set, SIGPIPE)) {
            struct timespec no_wait {0, 0};
            sigtimedwait(&blocked_set, nullptr, &no_wait);
        }
        pthread_sigmask(SIG_SETMASK, &old_set, nullptr);
    }

    setpgid(pid, pid);
    int flags = fcntl(output_pipe[0], F_GETFL, 0);
    if (flags >= 0) fcntl(output_pipe[0], F_SETFL, flags | O_NONBLOCK);
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : INT_MAX);
    bool terminated = false;
    char buffer[1024];
    for (;;) {
        ssize_t count = read(output_pipe[0], buffer, sizeof(buffer));
        if (count > 0) {
            result.output.append(buffer, static_cast<size_t>(count));
            if (output_callback)
                output_callback(buffer, static_cast<size_t>(count));
            continue;
        }
        if (count < 0 && errno == EINTR)
            continue;
        int status = 0;
        pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) {
            if (!terminated) {
                if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
                else if (WIFSIGNALED(status)) result.exit_code = 128 + WTERMSIG(status);
            }
            break;
        }
        const bool cancelled = request && request->cancel_requested.load();
        const bool timed_out = timeout_ms > 0 && std::chrono::steady_clock::now() >= deadline;
        if ((cancelled || timed_out) && !terminated) {
            kill(-pid, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            kill(-pid, SIGKILL);
            result.exit_code = cancelled ? -ECANCELED : -ETIMEDOUT;
            terminated = true;
        }
        struct pollfd pfd { output_pipe[0], POLLIN, 0 };
        poll(&pfd, 1, 50);
    }
    close(output_pipe[0]);
    return result;
}

bool authentication_error(const std::string &output)
{
    static const char *markers[] = {
        "incorrect password", "incorrect password attempt", "authentication failure", "sorry, try again",
        "a password is required", "no password was provided", "3 incorrect password attempts"
    };
    std::string lowered = output;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    for (const char *marker : markers) {
        if (lowered.find(marker) != std::string::npos)
            return true;
    }
    return false;
}
#endif

void secure_clear(std::string &value)
{
    volatile char *data = value.empty() ? nullptr : &value[0];
    for (size_t i = 0; data && i < value.size(); ++i)
        data[i] = 0;
    value.clear();
}

void output_event_cb(void *user_data)
{
    std::unique_ptr<OutputEvent> event(static_cast<OutputEvent *>(user_data));
    if (event->request->output_cb && !event->data.empty())
        event->request->output_cb(event->data.data(), event->data.size(), event->request->user);
}

void dispatch_output(const std::shared_ptr<SudoRequest> &request, std::string data)
{
    if (!request->output_cb || data.empty())
        return;
    if (request->callback_thread == CP0_SUDO_CALLBACK_WORKER) {
        request->output_cb(data.data(), data.size(), request->user);
        return;
    }
    auto *event = new (std::nothrow) OutputEvent{request, std::move(data)};
    if (!event)
        return;
    if (lv_async_call(output_event_cb, event) != LV_RESULT_OK)
        delete event;
}

void destroy_prompt()
{
    if (g_key_hook) {
        lv_obj_remove_event_cb_with_user_data(g_key_hook, key_event_cb, nullptr);
        g_key_hook = nullptr;
    }
    lv_group_set_default(g_saved_group);
    lv_indev_t *indev = lv_indev_get_next(nullptr);
    if (indev)
        lv_indev_set_group(indev, g_saved_group);
    if (g_prompt_group) {
        lv_group_delete(g_prompt_group);
        g_prompt_group = nullptr;
    }
    if (g_box) {
        lv_obj_del(g_box);
        g_box = nullptr;
    }
    if (g_overlay) {
        lv_obj_del(g_overlay);
        g_overlay = nullptr;
    }
    g_password_label = nullptr;
    g_hint_label = nullptr;
    secure_clear(g_password);
}

void finish_request(const std::shared_ptr<SudoRequest> &request,
                    cp0_sudo_result_t result,
                    int exit_code)
{
    destroy_prompt();
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_current == request)
            g_current.reset();
        g_requests.erase(request->id);
        g_running = false;
    }

    auto complete_cb = request->complete_cb;
    request->complete_cb = nullptr;
    if (complete_cb) {
        if (request->callback_thread == CP0_SUDO_CALLBACK_WORKER) {
            std::thread([request, complete_cb, result, exit_code]() {
                complete_cb(result, exit_code, request->user);
            }).detach();
        } else {
            complete_cb(result, exit_code, request->user);
        }
    }
    start_next_request();
}

void update_password_label()
{
    if (!g_password_label)
        return;
    std::string masked(g_password.size(), '*');
    masked.push_back('_');
    lv_label_set_text(g_password_label, masked.c_str());
}

void show_auth_error()
{
    if (g_hint_label) {
        char text[64];
        std::snprintf(text, sizeof(text), "Wrong password (%d/%d). Try again.",
                      g_current ? g_current->auth_attempts : 0, kMaxAuthAttempts);
        lv_label_set_text(g_hint_label, text);
        lv_obj_set_style_text_color(g_hint_label, lv_color_hex(0xFF5555), 0);
    }
    secure_clear(g_password);
    update_password_label();
}

void worker_done_cb(void *user_data)
{
    std::unique_ptr<CompletionEvent> event(static_cast<CompletionEvent *>(user_data));
    auto request = event->request;
    if (request->cancel_requested.load()) {
        finish_request(request, CP0_SUDO_RESULT_CANCELLED, -ECANCELED);
        return;
    }
    if (event->result == CP0_SUDO_RESULT_AUTH_FAILED && request->auth_attempts < kMaxAuthAttempts) {
        g_running = false;
        show_auth_error();
        return;
    }
    finish_request(request, event->result, event->exit_code);
}

void run_request_worker(std::shared_ptr<SudoRequest> request, std::string password)
{
    cp0_sudo_result_t result = CP0_SUDO_RESULT_EXEC_FAILED;
    int exit_code = -ENOTSUP;

#if defined(_WIN32)
    (void)request;
#elif defined(HAL_PLATFORM_SDL)
    UserIdentity identity;
    std::vector<std::string> command = request->argv;
    if (request->use_login_shell) {
        std::string shell = std::getenv("SHELL") ? std::getenv("SHELL") : "/bin/sh";
        command = {shell, "-c", request->argv.front()};
    }
    ProcessResult execution = run_process(
        identity, std::move(command), nullptr,
        [request](const char *data, size_t size) {
            dispatch_output(request, std::string(data, size));
        }, request, request->exec_timeout_ms);
    exit_code = execution.exit_code;
    if (exit_code == -ECANCELED) result = CP0_SUDO_RESULT_CANCELLED;
    else if (exit_code == -ETIMEDOUT) result = CP0_SUDO_RESULT_TIMED_OUT;
    else result = exit_code == 0 ? CP0_SUDO_RESULT_SUCCESS : CP0_SUDO_RESULT_EXEC_FAILED;
#else
    UserIdentity identity;
    if (resolve_run_user(identity)) {
        std::string password_line = password + "\n";
        ProcessResult auth = run_process(identity,
                                         {"sudo", "-k", "-S", "-p", "", "-v"},
                                         &password_line, {}, request, request->auth_timeout_ms);
        secure_clear(password_line);
        if (auth.exit_code == -ECANCELED) {
            result = CP0_SUDO_RESULT_CANCELLED;
            exit_code = auth.exit_code;
        } else if (auth.exit_code == -ETIMEDOUT) {
            result = CP0_SUDO_RESULT_TIMED_OUT;
            exit_code = auth.exit_code;
        } else if (auth.exit_code != 0) {
            exit_code = auth.exit_code;
            result = authentication_error(auth.output)
                         ? CP0_SUDO_RESULT_AUTH_FAILED
                         : CP0_SUDO_RESULT_EXEC_FAILED;
        } else {
            std::vector<std::string> command;
            if (request->use_login_shell)
                command = {"sudo", "-n", "--", identity.shell, "-c", request->argv.front()};
            else {
                command = {"sudo", "-n", "--"};
                command.insert(command.end(), request->argv.begin(), request->argv.end());
            }
            ProcessResult execution = run_process(
                identity, std::move(command), nullptr,
                [request](const char *data, size_t size) {
                    dispatch_output(request, std::string(data, size));
                }, request, request->exec_timeout_ms);
            exit_code = execution.exit_code;
            if (exit_code == -ECANCELED) result = CP0_SUDO_RESULT_CANCELLED;
            else if (exit_code == -ETIMEDOUT) result = CP0_SUDO_RESULT_TIMED_OUT;
            else result = exit_code == 0 ? CP0_SUDO_RESULT_SUCCESS : CP0_SUDO_RESULT_EXEC_FAILED;
        }
    } else {
        exit_code = -EPERM;
    }
#endif

    secure_clear(password);
    auto event = std::make_unique<CompletionEvent>();
    event->request = request;
    event->result = result;
    event->exit_code = exit_code;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_completions.push_back(std::move(event));
    }
}

void completion_timer_cb(lv_timer_t *)
{
    for (;;) {
        std::unique_ptr<CompletionEvent> event;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_completions.empty()) break;
            event = std::move(g_completions.front());
            g_completions.pop_front();
        }
        worker_done_cb(event.release());
    }
}

void complete_queued_request(const std::shared_ptr<SudoRequest> &request,
                             cp0_sudo_result_t result = CP0_SUDO_RESULT_CANCELLED,
                             int exit_code = -ECANCELED)
{
    cp0_sudo_complete_cb_t callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_requests.erase(request->id);
        callback = request->complete_cb;
        request->complete_cb = nullptr;
    }
    if (!callback) return;
    if (request->callback_thread == CP0_SUDO_CALLBACK_WORKER) {
        std::thread([request, callback, result, exit_code]() {
            callback(result, exit_code, request->user);
        }).detach();
    } else {
        callback(result, exit_code, request->user);
    }
}

void submit_password()
{
    if (!g_current || g_running)
        return;
    g_running = true;
    ++g_current->auth_attempts;
    if (g_hint_label) {
        lv_label_set_text(g_hint_label, "Authenticating...");
        lv_obj_set_style_text_color(g_hint_label, lv_color_hex(0xA0A0A0), 0);
    }
    std::string password = g_password;
    secure_clear(g_password);
    update_password_label();
    std::thread(run_request_worker, g_current, std::move(password)).detach();
}

void key_event_cb(lv_event_t *event)
{
    auto *key = static_cast<struct key_item *>(lv_event_get_param(event));
    if (!key || key->key_state != KBD_KEY_RELEASED)
        return;
    lv_event_stop_processing(event);
    if (g_running)
        {
            if (key->key_code == KEY_ESC && g_current)
                g_current->cancel_requested.store(true);
            return;
        }
    if (key->key_code == KEY_ESC) {
        finish_request(g_current, CP0_SUDO_RESULT_CANCELLED, -ECANCELED);
        return;
    }
    if (key->key_code == KEY_ENTER) {
        submit_password();
        return;
    }
    if (key->key_code == KEY_BACKSPACE) {
        if (!g_password.empty())
            g_password.pop_back();
        update_password_label();
        return;
    }
    if (key->utf8[0] && static_cast<unsigned char>(key->utf8[0]) >= 0x20 &&
        g_password.size() + std::strlen(key->utf8) <= kMaxPasswordBytes) {
        g_password += key->utf8;
        update_password_label();
    }
}

void create_prompt()
{
    lv_obj_t *layer = lv_layer_top();
    g_overlay = lv_obj_create(layer);
    lv_obj_remove_style_all(g_overlay);
    lv_obj_set_size(g_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_overlay, LV_OPA_70, 0);
    lv_obj_clear_flag(g_overlay, LV_OBJ_FLAG_SCROLLABLE);

    g_box = lv_obj_create(layer);
    lv_obj_set_size(g_box, 240, 116);
    lv_obj_align(g_box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_box, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_color(g_box, lv_color_hex(0x4A9EFF), 0);
    lv_obj_set_style_border_width(g_box, 1, 0);
    lv_obj_set_style_radius(g_box, 6, 0);
    lv_obj_clear_flag(g_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(g_box);
    lv_label_set_text(title, "Sudo Password");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_text_color(title, lv_color_hex(0x4A9EFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    g_password_label = lv_label_create(g_box);
    lv_obj_set_width(g_password_label, 200);
    lv_obj_set_style_text_align(g_password_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(g_password_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(g_password_label, LV_ALIGN_TOP_MID, 0, 38);
    update_password_label();

    g_hint_label = lv_label_create(g_box);
    lv_label_set_text(g_hint_label, "Enter:OK  ESC:Cancel");
    lv_obj_set_width(g_hint_label, 220);
    lv_obj_set_style_text_align(g_hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(g_hint_label, lv_color_hex(0x808090), 0);
    lv_obj_align(g_hint_label, LV_ALIGN_BOTTOM_MID, 0, -8);

    g_key_hook = lv_screen_active();
    if (g_key_hook)
        lv_obj_add_event_cb(g_key_hook, key_event_cb,
                            static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), nullptr);
    g_saved_group = lv_group_get_default();
    g_prompt_group = lv_group_create();
    lv_group_add_obj(g_prompt_group, g_overlay);
    lv_group_set_default(g_prompt_group);
    lv_indev_t *indev = lv_indev_get_next(nullptr);
    if (indev)
        lv_indev_set_group(indev, g_prompt_group);
}

void start_next_request()
{
    for (;;) {
        std::shared_ptr<SudoRequest> cancelled;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_current || g_queue.empty()) return;
            auto next = g_queue.front();
            g_queue.pop_front();
            if (next->cancel_requested.load()) cancelled = next;
            else g_current = next;
        }
        if (!cancelled) break;
        complete_queued_request(cancelled);
    }
    create_prompt();
}

void enqueue_cb(void *user_data)
{
    std::unique_ptr<std::shared_ptr<SudoRequest>> holder(
        static_cast<std::shared_ptr<SudoRequest> *>(user_data));
    if (!g_completion_timer)
        g_completion_timer = lv_timer_create(completion_timer_cb, 20, nullptr);
    if (!g_completion_timer) {
        complete_queued_request(*holder, CP0_SUDO_RESULT_EXEC_FAILED, -ENOMEM);
        return;
    }
    bool cancelled = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        cancelled = (*holder)->cancel_requested.load();
        if (!cancelled) g_queue.push_back(*holder);
    }
    if (cancelled) {
        complete_queued_request(*holder);
        return;
    }
    start_next_request();
}

void cancel_request_cb(void *user_data)
{
    std::unique_ptr<uint64_t> id(static_cast<uint64_t *>(user_data));
    std::shared_ptr<SudoRequest> cancelled;
    std::shared_ptr<SudoRequest> current;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_current && g_current->id == *id && !g_running)
            current = g_current;
        for (auto it = g_queue.begin(); it != g_queue.end(); ++it) {
            if ((*it)->id == *id) {
                cancelled = *it;
                g_queue.erase(it);
                break;
            }
        }
    }
    if (current) {
        finish_request(current, CP0_SUDO_RESULT_CANCELLED, -ECANCELED);
        return;
    }
    if (cancelled) complete_queued_request(cancelled);
}

int enqueue_request(std::shared_ptr<SudoRequest> request)
{
    auto *holder = new (std::nothrow) std::shared_ptr<SudoRequest>(std::move(request));
    if (!holder)
        return -ENOMEM;
    if (lv_async_call(enqueue_cb, holder) != LV_RESULT_OK) {
        delete holder;
        return -EIO;
    }
    return 0;
}

bool valid_callback_thread(cp0_sudo_callback_thread_t callback_thread)
{
    return callback_thread == CP0_SUDO_CALLBACK_LVGL ||
           callback_thread == CP0_SUDO_CALLBACK_WORKER;
}

} // namespace

extern "C" int cp0_sudo_run_argv_async(const char *const *argv,
                                         cp0_sudo_callback_thread_t callback_thread,
                                         cp0_sudo_output_cb_t output_cb,
                                         cp0_sudo_complete_cb_t complete_cb,
                                         void *user)
{
    return cp0_sudo_run_argv_async_ex(argv, callback_thread, output_cb, complete_cb,
                                     user, 0, 0, nullptr);
}

extern "C" int cp0_sudo_run_argv_async_ex(const char *const *argv,
                                            cp0_sudo_callback_thread_t callback_thread,
                                            cp0_sudo_output_cb_t output_cb,
                                            cp0_sudo_complete_cb_t complete_cb,
                                            void *user, int auth_timeout_ms,
                                            int exec_timeout_ms, uint64_t *request_id)
{
    if (!argv || !argv[0] || !argv[0][0] || !valid_callback_thread(callback_thread))
        return -EINVAL;
    auto request = std::make_shared<SudoRequest>();
    request->callback_thread = callback_thread;
    request->output_cb = output_cb;
    request->complete_cb = complete_cb;
    request->user = user;
    request->id = g_next_request_id.fetch_add(1);
    request->auth_timeout_ms = auth_timeout_ms;
    request->exec_timeout_ms = exec_timeout_ms;
    for (size_t i = 0; argv[i]; ++i)
        request->argv.emplace_back(argv[i]);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_requests[request->id] = request;
    }
    int rc = enqueue_request(request);
    if (rc != 0) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_requests.erase(request->id);
        return rc;
    }
    if (request_id) *request_id = request->id;
    return 0;
}

extern "C" int cp0_sudo_cancel(uint64_t request_id)
{
    auto *id = new (std::nothrow) uint64_t(request_id);
    if (!id) return -ENOMEM;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto found = g_requests.find(request_id);
        if (found == g_requests.end()) {
            delete id;
            return -ENOENT;
        }
        found->second->cancel_requested.store(true);
    }
    if (lv_async_call(cancel_request_cb, id) != LV_RESULT_OK) {
        delete id;
        return -EIO;
    }
    return 0;
}

extern "C" int cp0_sudo_run_shell_async(const char *command,
                                          cp0_sudo_callback_thread_t callback_thread,
                                          cp0_sudo_output_cb_t output_cb,
                                          cp0_sudo_complete_cb_t complete_cb,
                                          void *user)
{
    if (!command || !command[0] || !valid_callback_thread(callback_thread))
        return -EINVAL;
    auto request = std::make_shared<SudoRequest>();
    request->argv.emplace_back(command);
    request->use_login_shell = true;
    request->callback_thread = callback_thread;
    request->output_cb = output_cb;
    request->complete_cb = complete_cb;
    request->user = user;
    request->id = g_next_request_id.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_requests[request->id] = request;
    }
    int rc = enqueue_request(request);
    if (rc != 0) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_requests.erase(request->id);
    }
    return rc;
}
