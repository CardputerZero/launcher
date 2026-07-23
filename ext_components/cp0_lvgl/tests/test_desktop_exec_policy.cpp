#include "../src/cp0/cp0_desktop_exec_policy.hpp"

#include <cassert>
#include <cstdlib>
#include <string>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

int main()
{
    using cp0_desktop_exec_policy::evaluate;

    assert(!evaluate(nullptr).allowed);
    assert(evaluate(nullptr).reason == "empty Exec");
    assert(!evaluate("").allowed);
    assert(evaluate("python3 app.py").allowed);
    assert(evaluate("vim /tmp/file").allowed);
    assert(!evaluate("unknown-app --flag").allowed);
    assert(evaluate("unknown-app --flag").reason == "executable name is not allowlisted");
    assert(!evaluate("python3 app.py; reboot").allowed);
    assert(evaluate("python3 app.py; reboot").reason == "Exec contains shell metacharacters");
    assert(!evaluate(std::string(513, 'a').c_str()).allowed);

#if !defined(_WIN32)
    char path[] = "/tmp/cp0-exec-policy-XXXXXX";
    const int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    assert(!evaluate(path).allowed);
    assert(chmod(path, 0700) == 0);
    assert(evaluate(path).allowed);
    unlink(path);
#endif

    return 0;
}
