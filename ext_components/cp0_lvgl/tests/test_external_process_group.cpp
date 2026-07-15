#include "../src/cp0_external_process_group.hpp"

#include <cassert>
#include <chrono>
#include <csignal>
#include <thread>
#include <unistd.h>

int main()
{
    using namespace std::chrono;

    assert(cp0_process_group::enable_subreaper());

    pid_t leader = fork();
    assert(leader >= 0);
    if (leader == 0) {
        setpgid(0, 0);
        pid_t worker = fork();
        if (worker < 0) _exit(2);
        if (worker == 0) {
            std::this_thread::sleep_for(milliseconds(250));
            _exit(0);
        }
        _exit(0);
    }

    setpgid(leader, leader);
    int leader_status = 0;
    bool leader_reaped = false;
    const auto start = steady_clock::now();
    bool observed_leader_exit_with_live_group = false;

    while (cp0_process_group::exists(leader)) {
        cp0_process_group::reap_available(leader, leader, leader_status, leader_reaped);
        if (leader_reaped && cp0_process_group::exists(leader))
            observed_leader_exit_with_live_group = true;
        assert(steady_clock::now() - start < seconds(2));
        std::this_thread::sleep_for(milliseconds(10));
    }
    cp0_process_group::reap_available(leader, leader, leader_status, leader_reaped);

    assert(leader_reaped);
    assert(WIFEXITED(leader_status) && WEXITSTATUS(leader_status) == 0);
    assert(observed_leader_exit_with_live_group);
}
