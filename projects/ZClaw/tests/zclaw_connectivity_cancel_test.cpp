#include "zclaw_connectivity.h"

#include <cassert>
#include <memory>
#include <string>
#include <utility>

namespace {

class PendingProbe final : public zclaw::ConnectivityProbe {
public:
    bool get(const std::string &, Completion completion) override
    {
        if (cancelled_) {
            completion(false, "cancelled");
            return true;
        }
        completion_ = std::move(completion);
        return true;
    }

    void cancel() noexcept override
    {
        cancelled_ = true;
        Completion completion = std::move(completion_);
        if (completion)
            completion(false, "cancelled");
    }

private:
    Completion completion_;
    bool cancelled_ = false;
};

}  // namespace

int main()
{
    auto probe = std::make_shared<PendingProbe>();
    zclaw::ConnectivityChecker checker(probe);
    bool completed = false;
    bool online = true;
    std::string error;
    assert(checker.check([&](bool value, std::string detail) {
        completed = true;
        online = value;
        error = std::move(detail);
    }));
    assert(!completed);
    checker.cancel();
    checker.cancel();
    assert(completed);
    assert(!online);
    assert(error == "cancelled");
    return 0;
}
