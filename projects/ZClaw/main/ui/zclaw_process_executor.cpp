#include "zclaw_process_executor.h"

#include "zclaw_text.h"

#include <cerrno>
#include <chrono>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace zclaw {
namespace {

CommandResult cancelled_result()
{
    return {-1, "command cancelled"};
}

bool close_on_exec(int descriptor)
{
    const int flags = ::fcntl(descriptor, F_GETFD);
    return flags >= 0 && ::fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) == 0;
}

}  // namespace

ProcessExecutor::~ProcessExecutor()
{
    shutdown();
}

CommandResult ProcessExecutor::run(const std::vector<std::string> &arguments)
{
    if (arguments.empty() || arguments.front().empty())
        return {-1, "missing command"};
    std::vector<char *> argv;
    argv.reserve(arguments.size() + 1);
    for (const std::string &argument : arguments)
        argv.push_back(const_cast<char *>(argument.c_str()));
    argv.push_back(nullptr);

    int output_pipe[2] = {-1, -1};
    pid_t process = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_)
            return cancelled_result();
        if (::pipe(output_pipe) != 0)
            return {-1, "failed to create command pipe"};
        if (!close_on_exec(output_pipe[0]) || !close_on_exec(output_pipe[1])) {
            ::close(output_pipe[0]);
            ::close(output_pipe[1]);
            return {-1, "failed to configure command pipe"};
        }
        process = ::fork();
        if (process > 0) {
            ::setpgid(process, process);
            try {
                active_processes_.insert(process);
            } catch (...) {
                ::kill(-process, SIGKILL);
                ::kill(process, SIGKILL);
                ::close(output_pipe[0]);
                ::close(output_pipe[1]);
                while (::waitpid(process, nullptr, 0) < 0 && errno == EINTR) {
                }
                throw;
            }
        }
    }

    if (process == 0) {
        ::setpgid(0, 0);
        ::close(output_pipe[0]);
        ::dup2(output_pipe[1], STDOUT_FILENO);
        ::dup2(output_pipe[1], STDERR_FILENO);
        ::close(output_pipe[1]);
        ::execvp(argv[0], argv.data());
        ::_exit(127);
    }

    ::close(output_pipe[1]);
    if (process < 0) {
        ::close(output_pipe[0]);
        return {-1, "failed to start command"};
    }

    const pid_t active_process = process;
    bool reaped = false;
    CommandResult result;
    try {
        bool read_failed = false;
        char buffer[256];
        while (true) {
            const ssize_t bytes = ::read(output_pipe[0], buffer, sizeof(buffer));
            if (bytes > 0) {
                result.output.append(buffer, static_cast<std::size_t>(bytes));
                continue;
            }
            if (bytes < 0 && errno == EINTR)
                continue;
            read_failed = bytes < 0;
            break;
        }
        ::close(output_pipe[0]);
        output_pipe[0] = -1;

        if (read_failed) {
            ::kill(-process, SIGKILL);
            ::kill(process, SIGKILL);
        }

        int status = -1;
        while (::waitpid(process, &status, 0) < 0 && errno == EINTR) {
        }
        reaped = true;
        result.status = status;
        result.output = trim_ascii_whitespace(result.output);
    } catch (...) {
        if (output_pipe[0] >= 0)
            ::close(output_pipe[0]);
        if (!reaped) {
            ::kill(-process, SIGKILL);
            ::kill(process, SIGKILL);
            while (::waitpid(process, nullptr, 0) < 0 && errno == EINTR) {
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            active_processes_.erase(active_process);
        }
        changed_.notify_all();
        throw;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_processes_.erase(active_process);
    }
    changed_.notify_all();
    return result;
}

void ProcessExecutor::wait(unsigned int seconds) const
{
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait_for(lock, std::chrono::seconds(seconds),
                      [this] { return shutdown_; });
}

void ProcessExecutor::shutdown() noexcept
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (!shutdown_) {
        shutdown_ = true;
        for (pid_t process : active_processes_) {
            if (::kill(-process, SIGKILL) != 0)
                ::kill(process, SIGKILL);
        }
        changed_.notify_all();
    }
    changed_.wait(lock, [this] { return active_processes_.empty(); });
}

bool ProcessExecutor::shutdown_requested() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
}

}  // namespace zclaw
