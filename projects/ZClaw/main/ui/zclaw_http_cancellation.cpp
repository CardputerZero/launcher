#include "zclaw_http_cancellation.h"

#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace zclaw {

struct HttpCancellationRegistration::State {
    mutable std::mutex mutex;
    std::unordered_map<std::uint64_t, HttpCancellationRegistry::Stop> requests;
    std::uint64_t next_id = 1;
    bool shutdown = false;
};

HttpCancellationRegistration::HttpCancellationRegistration(
    std::shared_ptr<State> state, std::uint64_t id)
    : state_(std::move(state)), id_(id)
{
}

HttpCancellationRegistration::~HttpCancellationRegistration()
{
    reset();
}

HttpCancellationRegistration::HttpCancellationRegistration(
    HttpCancellationRegistration &&other) noexcept
    : state_(std::move(other.state_)), id_(other.id_)
{
    other.id_ = 0;
}

HttpCancellationRegistration &HttpCancellationRegistration::operator=(
    HttpCancellationRegistration &&other) noexcept
{
    if (this != &other) {
        reset();
        state_ = std::move(other.state_);
        id_ = other.id_;
        other.id_ = 0;
    }
    return *this;
}

bool HttpCancellationRegistration::active() const
{
    return id_ != 0;
}

void HttpCancellationRegistration::reset() noexcept
{
    if (state_ && id_ != 0) {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->requests.erase(id_);
    }
    state_.reset();
    id_ = 0;
}

HttpCancellationRegistry::HttpCancellationRegistry()
    : state_(std::make_shared<HttpCancellationRegistration::State>())
{
}

HttpCancellationRegistration HttpCancellationRegistry::register_request(Stop stop)
{
    if (!stop)
        return {};
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->shutdown)
        return {};
    const std::uint64_t id = state_->next_id++;
    state_->requests.emplace(id, std::move(stop));
    return {state_, id};
}

void HttpCancellationRegistry::shutdown() noexcept
{
    std::vector<Stop> stops;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->shutdown)
            return;
        state_->shutdown = true;
        stops.reserve(state_->requests.size());
        for (auto &request : state_->requests)
            stops.push_back(std::move(request.second));
        state_->requests.clear();
    }
    for (Stop &stop : stops) {
        try {
            stop();
        } catch (...) {
        }
    }
}

bool HttpCancellationRegistry::shutdown_requested() const
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->shutdown;
}

}  // namespace zclaw
