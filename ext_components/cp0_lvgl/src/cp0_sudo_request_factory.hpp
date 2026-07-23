#pragma once

#include "cp0_sudo_coordinator.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

namespace cp0_sudo {

struct RequestBuildResult {
    int error = 0;
    std::shared_ptr<Request> request;
};

class RequestFactory {
public:
    explicit RequestFactory(uint64_t first_id = 1) : next_id_(first_id) {}

    RequestBuildResult create_argv(const char *const *argv,
                                   cp0_sudo_callback_thread_t callback_thread,
                                   cp0_sudo_output_cb_t output_cb,
                                   cp0_sudo_complete_cb_t complete_cb,
                                   void *user, int auth_timeout_ms,
                                   int exec_timeout_ms);
    RequestBuildResult create_shell(const char *command,
                                    cp0_sudo_callback_thread_t callback_thread,
                                    cp0_sudo_output_cb_t output_cb,
                                    cp0_sudo_complete_cb_t complete_cb,
                                    void *user);

private:
    static bool valid_thread(cp0_sudo_callback_thread_t thread);
    std::shared_ptr<Request> create(cp0_sudo_callback_thread_t callback_thread,
                                    cp0_sudo_output_cb_t output_cb,
                                    cp0_sudo_complete_cb_t complete_cb,
                                    void *user);

    std::atomic<uint64_t> next_id_;
};

} // namespace cp0_sudo
