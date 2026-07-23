#pragma once

#include "zclaw_types.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>

namespace zclaw {

struct ApprovalWaitResult {
    std::string decision = "deny";
    bool timed_out = false;
};

class ApprovalController {
public:
    bool begin(const ApprovalRequest &request);
    bool pending() const;
    bool pending_request(const std::string &request_id) const;
    bool answer(const std::string &decision);
    void cancel();
    void shutdown();

    int selected_index() const;
    void move_selection(int delta);
    std::string selected_decision() const;

    ApprovalWaitResult wait_for(const std::string &request_id,
                                std::chrono::milliseconds timeout);

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::string request_id_;
    std::string decision_;
    bool waiting_ = false;
    bool shutdown_ = false;
    std::string waiter_request_id_;
    int selected_ = 0;
};

}  // namespace zclaw
