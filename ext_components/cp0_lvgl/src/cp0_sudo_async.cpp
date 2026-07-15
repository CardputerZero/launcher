#include "cp0_lvgl_app.h"
#include "compat/input_keys.h"
#include "hal_lvgl_bsp.h"
#include "keyboard_input.h"
#include "cp0_process_runner.hpp"
#include "cp0_sudo_coordinator.hpp"
#include "cp0_dispatch_testable.hpp"

#include "lvgl/lvgl.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <pwd.h>
#include <unistd.h>
#endif

namespace {

using cp0_sudo::Action;
using cp0_sudo::ActionType;
using cp0_sudo::Request;

constexpr size_t kMaxPasswordBytes = 128;
constexpr size_t kMaxCapturedOutputBytes = 64 * 1024;
cp0_sudo::Coordinator g_coordinator;
std::atomic<uint64_t> g_next_request_id{1};

struct SignalRequestContext {
    std::function<void(int, int)> complete;
};

void signal_request_complete(cp0_sudo_result_t result, int exit_code, void *user)
{
    std::unique_ptr<SignalRequestContext> context(static_cast<SignalRequestContext *>(user));
    if (context->complete) context->complete(static_cast<int>(result), exit_code);
}

lv_obj_t *g_overlay = nullptr;
lv_obj_t *g_box = nullptr;
lv_obj_t *g_password_label = nullptr;
lv_obj_t *g_hint_label = nullptr;
lv_obj_t *g_key_hook = nullptr;
lv_group_t *g_saved_group = nullptr;
lv_group_t *g_prompt_group = nullptr;
lv_timer_t *g_timer = nullptr;
std::shared_ptr<Request> g_prompt_request;
std::string g_password;
std::mutex g_queued_password_mutex;
std::string g_queued_password;
int64_t g_queued_password_deadline_ms = 0;

int64_t now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void secure_clear(std::string &value)
{
    volatile char *data = value.empty() ? nullptr : &value[0];
    for (size_t i = 0; data && i < value.size(); ++i) data[i] = 0;
    value.clear();
}

#if !defined(_WIN32)
struct UserIdentity {
    std::string name, home, shell;
    uint32_t uid = UINT32_MAX, gid = UINT32_MAX;
};

std::string config_get_string(const char *key)
{
    std::string value;
    cp0_signal_config_api({"GetStr", key ? std::string(key) : std::string(), value},
        [&](int code, std::string data) { if (code == 0) value = std::move(data); });
    return value;
}

bool is_nologin_shell(const char *shell)
{
    return !shell || !shell[0] || std::strstr(shell, "nologin") ||
           std::strstr(shell, "/bin/false");
}

bool resolve_run_user(UserIdentity &identity)
{
    std::string configured;
    if (geteuid() == 0) configured = config_get_string("run_as_user");
    else {
        passwd *current = getpwuid(geteuid());
        if (current && current->pw_name) configured = current->pw_name;
    }
    passwd *pwd = nullptr;
    if (!configured.empty()) pwd = getpwnam(configured.c_str());
    else {
        std::string fallback;
        setpwent();
        while ((pwd = getpwent()) != nullptr) {
            if (pwd->pw_uid >= 1000 && pwd->pw_uid < 65534 &&
                !is_nologin_shell(pwd->pw_shell)) {
                fallback = pwd->pw_name ? pwd->pw_name : "";
                break;
            }
        }
        endpwent();
        pwd = getpwnam(fallback.empty() ? "pi" : fallback.c_str());
    }
    if (!pwd || pwd->pw_uid == 0 || is_nologin_shell(pwd->pw_shell)) return false;
    identity.name = pwd->pw_name ? pwd->pw_name : "";
    identity.home = pwd->pw_dir && pwd->pw_dir[0] ? pwd->pw_dir : "/";
    identity.shell = pwd->pw_shell && pwd->pw_shell[0] ? pwd->pw_shell : "/bin/sh";
    identity.uid = static_cast<uint32_t>(pwd->pw_uid);
    identity.gid = static_cast<uint32_t>(pwd->pw_gid);
    return !identity.name.empty();
}

bool authentication_error(const std::string &output)
{
    static const char *markers[] = {"incorrect password", "incorrect password attempt",
        "authentication failure", "sorry, try again", "a password is required",
        "no password was provided", "3 incorrect password attempts"};
    std::string lowered = output;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    for (const char *marker : markers)
        if (lowered.find(marker) != std::string::npos) return true;
    return false;
}
#endif

void key_event_cb(lv_event_t *event);
void execute_actions(std::vector<Action> actions);
void submit_password();
void apply_queued_password(void *);

void clear_queued_password()
{
    std::lock_guard<std::mutex> lock(g_queued_password_mutex);
    secure_clear(g_queued_password);
    g_queued_password_deadline_ms = 0;
}

bool take_queued_password(std::string &password)
{
    std::lock_guard<std::mutex> lock(g_queued_password_mutex);
    if (g_queued_password.empty()) return false;
    if (g_queued_password_deadline_ms > 0 && now_ms() > g_queued_password_deadline_ms) {
        secure_clear(g_queued_password);
        g_queued_password_deadline_ms = 0;
        return false;
    }
    password = std::move(g_queued_password);
    g_queued_password.clear();
    g_queued_password_deadline_ms = 0;
    return true;
}

void destroy_prompt()
{
    if (g_key_hook) {
        lv_obj_remove_event_cb_with_user_data(g_key_hook, key_event_cb, nullptr);
        g_key_hook = nullptr;
    }
    lv_group_set_default(g_saved_group);
    if (lv_indev_t *indev = lv_indev_get_next(nullptr)) lv_indev_set_group(indev, g_saved_group);
    if (g_prompt_group) { lv_group_delete(g_prompt_group); g_prompt_group = nullptr; }
    if (g_box) { lv_obj_del(g_box); g_box = nullptr; }
    if (g_overlay) { lv_obj_del(g_overlay); g_overlay = nullptr; }
    g_password_label = nullptr;
    g_hint_label = nullptr;
    g_prompt_request.reset();
    secure_clear(g_password);
    clear_queued_password();
}

void update_password_label()
{
    if (!g_password_label) return;
    std::string masked(g_password.size(), '*');
    masked.push_back('_');
    lv_label_set_text(g_password_label, masked.c_str());
}

void create_prompt(const std::shared_ptr<Request> &request)
{
    if (g_overlay) return;
    g_prompt_request = request;
    lv_obj_t *layer = lv_layer_top();
    g_overlay = lv_obj_create(layer);
    lv_obj_remove_style_all(g_overlay);
    lv_obj_set_size(g_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_overlay, lv_color_hex(0), 0);
    lv_obj_set_style_bg_opa(g_overlay, LV_OPA_70, 0);
    lv_obj_clear_flag(g_overlay, LV_OBJ_FLAG_SCROLLABLE);
    g_box = lv_obj_create(layer);
    lv_obj_remove_style_all(g_box);
    lv_obj_set_size(g_box, 240, 116);
    lv_obj_align(g_box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_box, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(g_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_box, lv_color_hex(0x4A9EFF), 0);
    lv_obj_set_style_border_width(g_box, 1, 0);
    lv_obj_set_style_radius(g_box, 6, 0);
    lv_obj_set_style_pad_all(g_box, 0, 0);
    lv_obj_clear_flag(g_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title = lv_label_create(g_box);
    lv_label_set_text(title, "Sudo Password");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);
    lv_obj_set_style_text_color(title, lv_color_hex(0x4A9EFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    g_password_label = lv_label_create(g_box);
    lv_obj_set_width(g_password_label, 200);
    lv_obj_set_style_text_align(g_password_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(g_password_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(g_password_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_password_label, LV_ALIGN_CENTER, 0, -1);
    update_password_label();
    g_hint_label = lv_label_create(g_box);
    lv_label_set_text(g_hint_label, "Enter:OK  ESC:Cancel");
    lv_obj_set_width(g_hint_label, 220);
    lv_obj_set_style_text_align(g_hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(g_hint_label, lv_color_hex(0x808090), 0);
    lv_obj_set_style_text_font(g_hint_label, &lv_font_montserrat_10, 0);
    lv_obj_align(g_hint_label, LV_ALIGN_BOTTOM_MID, 0, -12);
    g_key_hook = lv_screen_active();
    if (g_key_hook) lv_obj_add_event_cb(g_key_hook, key_event_cb,
        static_cast<lv_event_code_t>(LV_EVENT_KEYBOARD), nullptr);
    g_saved_group = lv_group_get_default();
    g_prompt_group = lv_group_create();
    lv_group_add_obj(g_prompt_group, g_overlay);
    lv_group_set_default(g_prompt_group);
    if (lv_indev_t *indev = lv_indev_get_next(nullptr)) lv_indev_set_group(indev, g_prompt_group);
    lv_async_call(apply_queued_password, nullptr);
}

void show_auth_error(const std::shared_ptr<Request> &request)
{
    g_prompt_request = request;
    if (g_hint_label) {
        char text[64];
        std::snprintf(text, sizeof(text), "Wrong password (%d/3). Try again.", request->auth_attempts);
        lv_label_set_text(g_hint_label, text);
        lv_obj_set_style_text_color(g_hint_label, lv_color_hex(0xFF5555), 0);
    }
    secure_clear(g_password);
    update_password_label();
}

struct ActionHolder { std::vector<Action> actions; };
void action_cb(void *user)
{
    std::unique_ptr<ActionHolder> holder(static_cast<ActionHolder *>(user));
    execute_actions(std::move(holder->actions));
}

bool post_actions(std::vector<Action> actions)
{
    if (actions.empty()) return true;
    auto *holder = new (std::nothrow) ActionHolder{std::move(actions)};
    if (!holder) return false;
    bool scheduled = cp0_testable::dispatch_task(
        [holder](auto callback) { return lv_async_call(callback, holder) == LV_RESULT_OK; },
        action_cb);
    if (!scheduled) { delete holder; return false; }
    return true;
}

void stream_output(const std::shared_ptr<Request> &request, const char *data, size_t size)
{
    if (!request->output_cb || !size) return;
    if (request->callback_thread == CP0_SUDO_CALLBACK_WORKER) {
        if (request->cancel_requested.load()) return;
        request->output_cb(data, size, request->user);
        return;
    }
    size_t offset = 0;
    while (offset < size) {
        size_t chunk_size = std::min(size - offset, g_coordinator.max_output_chunk());
        for (;;) {
            std::string chunk(data + offset, chunk_size);
            auto outcome = g_coordinator.worker_output(request->id, chunk);
            if (outcome == cp0_sudo::OutputResult::ACCEPTED) { offset += chunk_size; break; }
            if (outcome == cp0_sudo::OutputResult::TERMINAL) return;
            if (outcome == cp0_sudo::OutputResult::TOO_LARGE) {
                if (chunk_size > 1) chunk_size /= 2;
                else std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

void run_worker(std::shared_ptr<Request> request, std::string password)
{
    cp0_sudo_result_t result = CP0_SUDO_RESULT_EXEC_FAILED;
    int exit_code = -ENOTSUP;
#if defined(_WIN32)
    (void)request;
#elif defined(HAL_PLATFORM_SDL)
    std::vector<std::string> command = request->argv;
    if (request->use_login_shell) {
        const char *shell = std::getenv("SHELL");
        command = {shell ? shell : "/bin/sh", "-c", request->argv.front()};
    }
    auto execution = cp0_runner::run(std::move(command), nullptr,
        [request](const char *data, size_t size) { stream_output(request, data, size); },
        &request->cancel_requested, request->exec_timeout_ms);
    exit_code = execution.exit_code;
    result = exit_code == -ECANCELED ? CP0_SUDO_RESULT_CANCELLED :
             exit_code == -ETIMEDOUT ? CP0_SUDO_RESULT_TIMED_OUT :
             exit_code == 0 ? CP0_SUDO_RESULT_SUCCESS : CP0_SUDO_RESULT_EXEC_FAILED;
#else
    UserIdentity identity;
    if (!resolve_run_user(identity)) exit_code = -EPERM;
    else {
        std::string password_line = password + "\n";
        std::vector<std::string> command;
        if (request->use_login_shell)
            command = {"sudo", "-k", "-S", "-p", "", "--", identity.shell, "-c",
                       request->argv.front()};
        else {
            command = {"sudo", "-k", "-S", "-p", "", "--"};
            command.insert(command.end(), request->argv.begin(), request->argv.end());
        }
        auto execution = cp0_runner::run(std::move(command), &password_line,
            [request](const char *data, size_t size) { stream_output(request, data, size); },
            &request->cancel_requested, request->exec_timeout_ms, kMaxCapturedOutputBytes,
            identity.uid, identity.gid, identity.name, identity.home, identity.shell);
        secure_clear(password_line);
        exit_code = execution.exit_code;
        result = exit_code == -ECANCELED ? CP0_SUDO_RESULT_CANCELLED :
                 exit_code == -ETIMEDOUT ? CP0_SUDO_RESULT_TIMED_OUT :
                 authentication_error(execution.output) ? CP0_SUDO_RESULT_AUTH_FAILED :
                 exit_code == 0 ? CP0_SUDO_RESULT_SUCCESS : CP0_SUDO_RESULT_EXEC_FAILED;
        if (result == CP0_SUDO_RESULT_AUTH_FAILED) {
            auto actions = g_coordinator.worker_auth_result(request->id, result, exit_code, now_ms());
            secure_clear(password);
            if (!actions.empty() && !post_actions(actions))
                g_coordinator.requeue_actions(std::move(actions));
            return;
        }
    }
#endif
    secure_clear(password);
    g_coordinator.worker_complete(request->id, result, exit_code);
}

void execute_actions(std::vector<Action> actions)
{
    if (!g_timer) {
        g_timer = lv_timer_create([](lv_timer_t *) {
            execute_actions(g_coordinator.tick(now_ms(), {}));
        }, 20, nullptr);
        if (!g_timer) {
            actions = g_coordinator.fail_all(-ENOMEM, now_ms());
            for (;;) {
                auto drained = g_coordinator.tick(now_ms(), {SIZE_MAX, SIZE_MAX});
                if (drained.empty()) break;
                actions.insert(actions.end(), std::make_move_iterator(drained.begin()),
                               std::make_move_iterator(drained.end()));
            }
        }
    }
    for (auto &action : actions) {
        switch (action.type) {
        case ActionType::SHOW_PROMPT: create_prompt(action.request); break;
        case ActionType::SHOW_AUTH_ERROR: show_auth_error(action.request); break;
        case ActionType::DESTROY_PROMPT: destroy_prompt(); break;
        case ActionType::START_WORKER: {
            std::string password = g_password;
            secure_clear(g_password);
            update_password_label();
            if (g_hint_label) lv_label_set_text(g_hint_label, "Authenticating...");
            std::thread(run_worker, action.request, std::move(password)).detach();
            break;
        }
        case ActionType::CALL_OUTPUT:
            if (action.request->output_cb && !action.data.empty())
                action.request->output_cb(action.data.data(), action.data.size(), action.request->user);
            g_coordinator.output_delivered(action.request->id, action.data.size());
            break;
        case ActionType::CALL_COMPLETE: {
            auto callback = action.request->complete_cb;
            action.request->complete_cb = nullptr;
            if (!callback) break;
            if (action.request->callback_thread == CP0_SUDO_CALLBACK_WORKER)
                std::thread([request = action.request, callback, result = action.result,
                             exit_code = action.exit_code]() {
                    callback(result, exit_code, request->user);
                }).detach();
            else callback(action.result, action.exit_code, action.request->user);
            break;
        }
        }
    }
}

void submit_password()
{
    if (!g_prompt_request) return;
    execute_actions(g_coordinator.submit_password(g_prompt_request->id));
}

void apply_queued_password(void *)
{
    if (!g_prompt_request || g_coordinator.state(g_prompt_request->id) != cp0_sudo::State::PROMPT)
        return;
    std::string password;
    if (!take_queued_password(password)) return;
    secure_clear(g_password);
    g_password = std::move(password);
    update_password_label();
    submit_password();
}

void key_event_cb(lv_event_t *event)
{
    auto *key = static_cast<key_item *>(lv_event_get_param(event));
    if (!key || key->key_state != KBD_KEY_RELEASED) return;
    lv_event_stop_processing(event);
    if (!g_prompt_request) return;
    if (g_coordinator.state(g_prompt_request->id) == cp0_sudo::State::RUNNING) {
        if (key->key_code == KEY_ESC) {
            std::vector<Action> actions;
            g_coordinator.cancel(g_prompt_request->id, actions, now_ms());
            execute_actions(std::move(actions));
        }
        return;
    }
    if (key->key_code == KEY_ESC) {
        std::vector<Action> actions;
        g_coordinator.cancel(g_prompt_request->id, actions, now_ms());
        execute_actions(std::move(actions));
    } else if (key->key_code == KEY_ENTER) submit_password();
    else if (key->key_code == KEY_BACKSPACE) {
        if (!g_password.empty()) g_password.pop_back();
        update_password_label();
    } else if (key->utf8[0] && static_cast<unsigned char>(key->utf8[0]) >= 0x20 &&
               g_password.size() + std::strlen(key->utf8) <= kMaxPasswordBytes) {
        g_password += key->utf8;
        update_password_label();
    }
}

bool valid_thread(cp0_sudo_callback_thread_t thread)
{
    return thread == CP0_SUDO_CALLBACK_LVGL || thread == CP0_SUDO_CALLBACK_WORKER;
}

void enqueue_cb(void *user)
{
    std::unique_ptr<std::shared_ptr<Request>> holder(
        static_cast<std::shared_ptr<Request> *>(user));
    execute_actions(g_coordinator.commit_reserved((*holder)->id, now_ms()));
}

int enqueue(std::shared_ptr<Request> request, uint64_t *request_id)
{
    if (!g_coordinator.reserve(request)) return -EEXIST;
    auto *holder = new (std::nothrow) std::shared_ptr<Request>(request);
    if (!holder) {
        auto actions = g_coordinator.release_reserved(request->id, now_ms());
        if (!actions.empty() && !post_actions(actions))
            g_coordinator.requeue_actions(std::move(actions));
        return -ENOMEM;
    }
    bool scheduled = cp0_testable::dispatch_task(
        [holder](auto callback) { return lv_async_call(callback, holder) == LV_RESULT_OK; },
        enqueue_cb);
    if (!scheduled) {
        delete holder;
        auto actions = g_coordinator.release_reserved(request->id, now_ms());
        if (!actions.empty() && !post_actions(actions))
            g_coordinator.requeue_actions(std::move(actions));
        return -EIO;
    }
    if (request_id) *request_id = request->id;
    return 0;
}

} // namespace

extern "C" int cp0_sudo_run_argv_async_ex(const char *const *argv,
    cp0_sudo_callback_thread_t callback_thread, cp0_sudo_output_cb_t output_cb,
    cp0_sudo_complete_cb_t complete_cb, void *user, int auth_timeout_ms,
    int exec_timeout_ms, uint64_t *request_id)
{
    if (!argv || !argv[0] || !argv[0][0] || !valid_thread(callback_thread)) return -EINVAL;
    auto request = std::make_shared<Request>();
    request->id = g_next_request_id.fetch_add(1);
    request->callback_thread = callback_thread;
    request->output_cb = output_cb;
    request->complete_cb = complete_cb;
    request->user = user;
    request->auth_timeout_ms = auth_timeout_ms;
    request->exec_timeout_ms = exec_timeout_ms;
    for (size_t i = 0; argv[i]; ++i) request->argv.emplace_back(argv[i]);
    return enqueue(std::move(request), request_id);
}

extern "C" int cp0_sudo_run_argv_async(const char *const *argv,
    cp0_sudo_callback_thread_t callback_thread, cp0_sudo_output_cb_t output_cb,
    cp0_sudo_complete_cb_t complete_cb, void *user)
{
    return cp0_sudo_run_argv_async_ex(argv, callback_thread, output_cb, complete_cb,
                                     user, 0, 0, nullptr);
}

extern "C" int cp0_sudo_run_shell_async(const char *command,
    cp0_sudo_callback_thread_t callback_thread, cp0_sudo_output_cb_t output_cb,
    cp0_sudo_complete_cb_t complete_cb, void *user)
{
    if (!command || !command[0] || !valid_thread(callback_thread)) return -EINVAL;
    auto request = std::make_shared<Request>();
    request->id = g_next_request_id.fetch_add(1);
    request->argv.emplace_back(command);
    request->use_login_shell = true;
    request->callback_thread = callback_thread;
    request->output_cb = output_cb;
    request->complete_cb = complete_cb;
    request->user = user;
    return enqueue(std::move(request), nullptr);
}

extern "C" int cp0_sudo_cancel(uint64_t request_id)
{
    std::vector<Action> actions;
    int rc = g_coordinator.cancel(request_id, actions, now_ms());
    if (rc != 0) return rc;
    if (!post_actions(actions)) g_coordinator.requeue_actions(std::move(actions));
    return 0;
}

extern "C" int cp0_sudo_queue_password(const char *password)
{
    if (!password || !password[0]) return -EINVAL;
    const size_t length = std::strlen(password);
    if (length > kMaxPasswordBytes) return -E2BIG;
    {
        std::lock_guard<std::mutex> lock(g_queued_password_mutex);
        secure_clear(g_queued_password);
        g_queued_password.assign(password, length);
        g_queued_password_deadline_ms = now_ms() + 60000;
    }
    if (lv_async_call(apply_queued_password, nullptr) != LV_RESULT_OK) {
        clear_queued_password();
        return -EIO;
    }
    return 0;
}

extern "C" void init_sudo_signals(void)
{
    cp0_signal_sudo_argv_async.append([](std::list<std::string> args, int auth_timeout_ms,
        int exec_timeout_ms, std::function<void(int, int)> complete,
        std::function<void(int, uint64_t)> started) {
        std::vector<std::string> storage(args.begin(), args.end());
        std::vector<const char *> argv;
        argv.reserve(storage.size() + 1);
        for (const std::string &arg : storage) argv.push_back(arg.c_str());
        argv.push_back(nullptr);
        auto *context = new (std::nothrow) SignalRequestContext{std::move(complete)};
        if (!context) {
            if (started) started(-ENOMEM, 0);
            return;
        }
        uint64_t request_id = 0;
        int ret = cp0_sudo_run_argv_async_ex(argv.data(), CP0_SUDO_CALLBACK_LVGL, nullptr,
            signal_request_complete, context, auth_timeout_ms, exec_timeout_ms, &request_id);
        if (ret != 0) delete context;
        if (started) started(ret, request_id);
    });
    cp0_signal_sudo_cancel.append([](uint64_t request_id, std::function<void(int)> done) {
        int ret = cp0_sudo_cancel(request_id);
        if (done) done(ret);
    });
    cp0_signal_system_admin_async.append([](std::list<std::string> args, int auth_timeout_ms,
        int exec_timeout_ms, std::function<void(int, int)> complete,
        std::function<void(int, uint64_t)> started) {
        if (args.empty()) {
            if (started) started(-EINVAL, 0);
            return;
        }
        const std::string command = args.front();
        const std::string value = args.size() > 1 ? *std::next(args.begin()) : std::string();
        std::list<std::string> argv;
        if (command == "AdbSet") {
            argv = {cp0_file_path("adb_helper"), value == "1" ? "enable" : "disable"};
        } else if (command == "NtpSet") {
            argv = {"timedatectl", "set-ntp", value == "1" ? "true" : "false"};
        } else if (command == "TimeSet") {
            const bool valid_shape = value.size() == 19 && value[4] == '-' && value[7] == '-' &&
                value[10] == ' ' && value[13] == ':' && value[16] == ':';
            bool digits_valid = valid_shape;
            for (size_t i = 0; digits_valid && i < value.size(); ++i) {
                if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) continue;
                digits_valid = std::isdigit(static_cast<unsigned char>(value[i])) != 0;
            }
            int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
            const bool parsed = digits_valid && std::sscanf(value.c_str(), "%d-%d-%d %d:%d:%d",
                &year, &month, &day, &hour, &minute, &second) == 6;
            const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
            static constexpr int days_per_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            const int max_day = month == 2 ? days_per_month[month] + (leap ? 1 : 0)
                                           : (month >= 1 && month <= 12 ? days_per_month[month] : 0);
            if (!parsed || year < 1970 || month < 1 || month > 12 || day < 1 || day > max_day ||
                hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
                if (started) started(-EINVAL, 0);
                return;
            }
            argv = {"/bin/sh", "-c", "date -s '" + value + "'; hwclock -w"};
        } else {
            if (started) started(-EINVAL, 0);
            return;
        }
        cp0_signal_sudo_argv_async(std::move(argv), auth_timeout_ms, exec_timeout_ms,
                                   std::move(complete), std::move(started));
    });
}
