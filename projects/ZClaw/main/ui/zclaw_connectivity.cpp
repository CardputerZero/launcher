#include "zclaw_connectivity.h"

#include "zclaw_http_client_policy.h"
#include "zclaw_lhv_http_client.h"
#include "zclaw_protocol.h"

#include <array>
#include <mutex>
#include <utility>

namespace zclaw {
namespace {

class HttpConnectivityProbe final
    : public ConnectivityProbe,
      public std::enable_shared_from_this<HttpConnectivityProbe> {
public:
    explicit HttpConnectivityProbe(std::shared_ptr<LhvHttpClient> client)
        : client_(std::move(client))
    {
    }

    bool get(const std::string &url, Completion completion) override
    {
        if (!completion || !client_)
            return false;
        bool cancelled = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelled = cancelled_;
        }
        if (cancelled) {
            completion(false, "Connectivity check cancelled.");
            return true;
        }
        auto request = std::make_shared<HttpRequest>();
        request->method = HTTP_GET;
        request->url = url;
        configure_http_request(*request, HttpClientProfile::Probe);
        const auto self = shared_from_this();
        return client_->send(
            std::move(request),
            [self, completion = std::move(completion)](
                const HttpResponsePtr &response) mutable {
                bool cancelled = false;
                {
                    std::lock_guard<std::mutex> lock(self->mutex_);
                    cancelled = self->cancelled_;
                }
                if (cancelled) {
                    completion(false, "Connectivity check cancelled.");
                    return;
                }
                completion(static_cast<bool>(response),
                           response ? std::string() : "Request failed.");
            });
    }

    void cancel() noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = true;
    }

private:
    std::shared_ptr<LhvHttpClient> client_;
    std::mutex mutex_;
    bool cancelled_ = false;
};

struct CheckState {
    std::shared_ptr<ConnectivityProbe> probe;
    ConnectivityChecker::Completion completion;
    std::size_t index = 0;
    std::string last_error;
    std::function<void()> next;
};

constexpr std::array<const char *, 3> kProbes = {
    "https://www.cloudflare.com/cdn-cgi/trace",
    "https://www.msftconnecttest.com/connecttest.txt",
    "https://github.com/",
};

}  // namespace

ConnectivityChecker::ConnectivityChecker()
    : probe_(std::make_shared<HttpConnectivityProbe>(
          std::make_shared<LhvHttpClient>()))
{
}

ConnectivityChecker::ConnectivityChecker(std::shared_ptr<LhvHttpClient> client)
    : probe_(std::make_shared<HttpConnectivityProbe>(std::move(client)))
{
}

ConnectivityChecker::ConnectivityChecker(std::shared_ptr<ConnectivityProbe> probe)
    : probe_(std::move(probe))
{
}

bool ConnectivityChecker::check(Completion completion)
{
    if (!probe_ || !completion)
        return false;
    auto state = std::make_shared<CheckState>();
    state->probe = probe_;
    state->completion = std::move(completion);
    std::weak_ptr<CheckState> weak_state = state;
    state->next = [weak_state] {
        const auto state = weak_state.lock();
        if (!state)
            return;
        if (state->index >= kProbes.size()) {
            auto completion = std::move(state->completion);
            state->next = {};
            completion(false, state->last_error.empty()
                                  ? "No public endpoint responded."
                                  : std::move(state->last_error));
            return;
        }
        const char *url = kProbes[state->index++];
        if (!state->probe->get(
                url, [state](bool online, std::string error) {
                    if (online) {
                        auto completion = std::move(state->completion);
                        state->next = {};
                        completion(true, {});
                        return;
                    }
                    state->last_error = std::move(error);
                    state->next();
                })) {
            auto completion = std::move(state->completion);
            state->next = {};
            completion(false, "Connectivity request was not submitted.");
        }
    };
    state->next();
    return true;
}

void ConnectivityChecker::cancel() noexcept
{
    if (probe_)
        probe_->cancel();
}

}  // namespace zclaw
