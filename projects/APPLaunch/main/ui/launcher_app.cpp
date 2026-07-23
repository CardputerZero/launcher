#include "launch.h"

#include <utility>

app::app(std::string name, std::string icon, std::string exec, bool terminal)
    : Name(std::move(name)), Icon(std::move(icon)), Exec(exec)
{
    launch = [exec = std::move(exec), terminal](Launch *owner) {
        if (!owner || exec.empty()) return;
        if (terminal) owner->launch_Exec_in_terminal(exec);
        else owner->launch_Exec(exec);
    };
}

app::app(std::string name, std::string icon, std::string exec, bool terminal, bool sysplause)
    : Name(std::move(name)), Icon(std::move(icon)), Exec(exec)
{
    launch = [exec = std::move(exec), terminal, sysplause](Launch *owner) {
        if (!owner || exec.empty()) return;
        if (terminal) owner->launch_Exec_in_terminal(exec, sysplause);
        else owner->launch_Exec(exec);
    };
}

app::app(std::string name,
         std::string icon,
         std::string exec,
         bool terminal,
         bool sysplause,
         bool run_as_root)
    : Name(std::move(name)), Icon(std::move(icon)), Exec(exec)
{
    launch = [exec = std::move(exec), terminal, sysplause, run_as_root](Launch *owner) {
        if (!owner || exec.empty()) return;
        if (terminal) owner->launch_Exec_in_terminal(exec, sysplause);
        else owner->launch_Exec(exec, run_as_root);
    };
}
