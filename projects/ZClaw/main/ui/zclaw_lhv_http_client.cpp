#include "zclaw_lhv_http_client.h"

#include <utility>

namespace zclaw {

struct LhvHttpClient::State {
    std::mutex mutex;
    std::unordered_map<std::uint64_t, Completion> pending;
    std::uint64_t next_id = 1;
};

LhvHttpClient::LhvHttpClient()
    : client_(std::make_unique<hv::HttpClient>()),
      state_(std::make_shared<State>())
{
}

LhvHttpClient::~LhvHttpClient()
{
    shutdown();
}

bool LhvHttpClient::send(HttpRequestPtr request, Completion completion)
{
    if (!request || !completion)
        return false;
    request->retry_count = 0;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!client_)
        return false;
    std::uint64_t id = 0;
    {
        std::lock_guard<std::mutex> state_lock(state_->mutex);
        id = state_->next_id++;
        state_->pending.emplace(id, std::move(completion));
    }
    const std::shared_ptr<State> state = state_;
    const int submitted = client_->sendAsync(
        std::move(request), [state, id](const HttpResponsePtr &response) {
            Completion completion;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                const auto found = state->pending.find(id);
                if (found == state->pending.end())
                    return;
                completion = std::move(found->second);
                state->pending.erase(found);
            }
            completion(response);
        });
    if (submitted == 0)
        return true;
    std::lock_guard<std::mutex> state_lock(state_->mutex);
    state_->pending.erase(id);
    return false;
}

void LhvHttpClient::shutdown() noexcept
{
    std::unique_ptr<hv::HttpClient> client;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        client = std::move(client_);
    }
    client.reset();
    std::unordered_map<std::uint64_t, Completion> pending;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        pending.swap(state_->pending);
    }
    for (auto &entry : pending)
        entry.second(nullptr);
}

}  // namespace zclaw
