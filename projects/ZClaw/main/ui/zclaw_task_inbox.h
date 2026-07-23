#pragma once

#include <functional>
#include <mutex>
#include <vector>

namespace zclaw {

class TaskInbox {
public:
    using Task = std::function<void()>;

    void activate();
    bool post(Task task);
    std::vector<Task> take_pending();
    void shutdown();
    bool active() const;

private:
    mutable std::mutex mutex_;
    std::vector<Task> pending_;
    bool active_ = false;
};

}  // namespace zclaw
