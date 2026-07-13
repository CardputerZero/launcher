#include "../src/cp0_process_runner.hpp"
#include <cassert>
#include <cerrno>
#include <chrono>
#include <string>
#include <thread>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main()
{
    std::atomic<bool> cancel{false};
    std::string streamed;
    auto ok = cp0_runner::run({"/bin/sh", "-c", "printf first; printf second"}, nullptr,
        [&](const char *p, size_t n) { streamed.append(p, n); }, &cancel, 1000, 16);
    assert(ok.exit_code == 0 && streamed == "firstsecond" && ok.output == "firstsecond");

    setenv("CP0_RUNNER_KEEP", "preserved", 1);
    auto environment = cp0_runner::run(
        {"/bin/sh", "-c", "printf '%s|%s|%s|%s|%s|%s' \"$HOME\" \"$USER\" \"$LOGNAME\" \"$SHELL\" \"$LC_ALL\" \"$CP0_RUNNER_KEEP\""},
        nullptr, {}, &cancel, 1000, 1024, UINT32_MAX, UINT32_MAX,
        "runner-user", "/runner-home", "/bin/runner-shell");
    assert(environment.exit_code == 0);
    assert(environment.output == "/runner-home|runner-user|runner-user|/bin/runner-shell|C|preserved");

    setenv("HOME", "/sdl-home", 1);
    setenv("USER", "sdl-user", 1);
    auto sdl_environment = cp0_runner::run(
        {"/bin/sh", "-c", "printf '%s|%s|%s' \"$HOME\" \"$USER\" \"$LC_ALL\""},
        nullptr, {}, &cancel, 1000);
    assert(sdl_environment.output == "/sdl-home|sdl-user|C");

    for (int i = 0; i < 50; ++i) {
        auto marker = cp0_runner::run({"/bin/sh", "-c", "printf body; printf END-MARKER"},
            nullptr, {}, &cancel, 1000);
        assert(marker.exit_code == 0 && marker.output == "bodyEND-MARKER");
    }

    auto descendant_start = std::chrono::steady_clock::now();
    auto inherited_pipe = cp0_runner::run(
        {"/bin/sh", "-c", "sleep 5 & printf parent-done"}, nullptr, {}, &cancel, 0);
    auto descendant_elapsed = std::chrono::steady_clock::now() - descendant_start;
    assert(inherited_pipe.exit_code == 0 && inherited_pipe.output == "parent-done");
    assert(descendant_elapsed < std::chrono::seconds(1));

    for (int i = 0; i < 100; ++i) {
        auto delayed_marker = cp0_runner::run(
            {"/bin/sh", "-c", "sleep 0.001; printf DELAYED-END"}, nullptr, {}, &cancel, 1000);
        assert(delayed_marker.exit_code == 0 && delayed_marker.output == "DELAYED-END");
    }

    char old_cwd[4096]; assert(getcwd(old_cwd, sizeof(old_cwd)) != nullptr);
    char temp_dir[] = "/tmp/cp0-runner-path-XXXXXX";
    assert(mkdtemp(temp_dir) != nullptr && chdir(temp_dir) == 0);
    int script = open("cwd-command", O_CREAT | O_WRONLY | O_TRUNC, 0700);
    assert(script >= 0);
    const char script_body[] = "#!/bin/sh\nprintf cwd-path-ok";
    assert(write(script, script_body, sizeof(script_body) - 1) == static_cast<ssize_t>(sizeof(script_body) - 1));
    close(script);
    const char *old_path_value = getenv("PATH");
    std::string old_path = old_path_value ? old_path_value : "";
    setenv("PATH", ":/usr/bin:/bin", 1);
    auto leading_empty_path = cp0_runner::run({"cwd-command"}, nullptr, {}, &cancel, 1000);
    assert(leading_empty_path.exit_code == 0 && leading_empty_path.output == "cwd-path-ok");
    setenv("PATH", "/usr/bin::/bin", 1);
    auto middle_empty_path = cp0_runner::run({"cwd-command"}, nullptr, {}, &cancel, 1000);
    assert(middle_empty_path.exit_code == 0 && middle_empty_path.output == "cwd-path-ok");
    setenv("PATH", old_path.c_str(), 1);
    assert(chdir(old_cwd) == 0);
    std::string script_path = std::string(temp_dir) + "/cwd-command";
    assert(unlink(script_path.c_str()) == 0 && rmdir(temp_dir) == 0);

    streamed.clear();
    auto large = cp0_runner::run({"/bin/sh", "-c", "head -c 100000 /dev/zero | tr '\\0' x"}, nullptr,
        [&](const char *p, size_t n) { streamed.append(p, n); }, &cancel, 2000, 65536);
    assert(large.exit_code == 0 && streamed.size() == 100000 && large.output.size() == 65536);
    assert(large.output == std::string(65536, 'x'));

    auto timeout = cp0_runner::run({"/bin/sh", "-c", "sleep 5"}, nullptr, {}, &cancel, 50);
    assert(timeout.exit_code == -ETIMEDOUT);

    auto noisy_timeout = cp0_runner::run({"/bin/sh", "-c", "while :; do printf xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx; done"},
        nullptr, {}, &cancel, 50);
    assert(noisy_timeout.exit_code == -ETIMEDOUT);

    cancel = false;
    std::thread trigger([&] { std::this_thread::sleep_for(std::chrono::milliseconds(50)); cancel = true; });
    auto cancelled = cp0_runner::run({"/bin/sh", "-c", "sleep 5"}, nullptr, {}, &cancel, 5000);
    trigger.join();
    assert(cancelled.exit_code == -ECANCELED);

    cancel = false;
    std::thread noisy_trigger([&] { std::this_thread::sleep_for(std::chrono::milliseconds(50)); cancel = true; });
    auto noisy_cancel = cp0_runner::run({"/bin/sh", "-c", "while :; do printf yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy; done"},
        nullptr, {}, &cancel, 5000);
    noisy_trigger.join();
    assert(noisy_cancel.exit_code == -ECANCELED);

    std::string huge_input(4 * 1024 * 1024, 'z');
    cancel = false;
    auto blocked_input_timeout = cp0_runner::run({"/bin/sh", "-c", "sleep 5"},
        &huge_input, {}, &cancel, 50);
    assert(blocked_input_timeout.exit_code == -ETIMEDOUT);
    cancel = false;
    std::thread input_trigger([&] { std::this_thread::sleep_for(std::chrono::milliseconds(50)); cancel = true; });
    auto blocked_input_cancel = cp0_runner::run({"/bin/sh", "-c", "sleep 5"},
        &huge_input, {}, &cancel, 5000);
    input_trigger.join();
    assert(blocked_input_cancel.exit_code == -ECANCELED);

    cancel = false;
    sigset_t pipe_set{}, old_set{}, pending{};
    sigemptyset(&pipe_set); sigaddset(&pipe_set, SIGPIPE);
    assert(pthread_sigmask(SIG_BLOCK, &pipe_set, &old_set) == 0);
    assert(pthread_kill(pthread_self(), SIGPIPE) == 0);
    auto closed_stdin = cp0_runner::run({"/bin/sh", "-c", "exec 0<&-; sleep 0.05"},
        &huge_input, {}, &cancel, 1000);
    assert(closed_stdin.exit_code == 0);
    assert(sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE));
    timespec zero{0, 0}; assert(sigtimedwait(&pipe_set, nullptr, &zero) == SIGPIPE);
    assert(pthread_sigmask(SIG_SETMASK, &old_set, nullptr) == 0);
}
