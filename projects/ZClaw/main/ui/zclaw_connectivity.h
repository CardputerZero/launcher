#pragma once

#include <memory>
#include <functional>
#include <string>

namespace zclaw {

class LhvHttpClient;

class ConnectivityProbe {
public:
    using Completion = std::function<void(bool, std::string)>;
    virtual ~ConnectivityProbe() = default;

    virtual bool get(const std::string &url, Completion completion) = 0;
    virtual void cancel() noexcept = 0;
};

class ConnectivityChecker {
public:
    using Completion = std::function<void(bool, std::string)>;
    ConnectivityChecker();
    explicit ConnectivityChecker(std::shared_ptr<LhvHttpClient> client);
    explicit ConnectivityChecker(std::shared_ptr<ConnectivityProbe> probe);

    bool check(Completion completion);
    void cancel() noexcept;

private:
    std::shared_ptr<ConnectivityProbe> probe_;
};

}  // namespace zclaw
