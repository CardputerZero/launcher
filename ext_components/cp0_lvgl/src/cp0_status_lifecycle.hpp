#pragma once

#include <functional>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace cp0::status {

struct Resource {
    bool valid = false;
    std::function<void()> release;
};

class Lifecycle {
public:
    using Factory = std::function<Resource()>;

    ~Lifecycle();
    bool start(const std::vector<Factory> &factories);
    void stop();
    bool active() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool active_ = false;
    bool starting_ = false;
    bool cancel_start_ = false;
    std::vector<std::function<void()>> releases_;
};

} // namespace cp0::status
