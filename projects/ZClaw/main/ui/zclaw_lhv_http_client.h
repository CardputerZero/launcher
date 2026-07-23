#pragma once

#include <hv/HttpClient.h>

#include <memory>
#include <mutex>
#include <cstdint>
#include <unordered_map>

namespace zclaw {

class LhvHttpClient {
public:
    using Completion = HttpResponseCallback;

    LhvHttpClient();
    ~LhvHttpClient();

    LhvHttpClient(const LhvHttpClient &) = delete;
    LhvHttpClient &operator=(const LhvHttpClient &) = delete;

    bool send(HttpRequestPtr request, Completion completion);
    void shutdown() noexcept;

private:
    struct State;
    std::mutex mutex_;
    std::unique_ptr<hv::HttpClient> client_;
    std::shared_ptr<State> state_;
};

}  // namespace zclaw
