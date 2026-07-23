#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

namespace zclaw {

class HttpCancellationRegistry;

class HttpCancellationRegistration {
public:
    HttpCancellationRegistration() = default;
    ~HttpCancellationRegistration();

    HttpCancellationRegistration(HttpCancellationRegistration &&other) noexcept;
    HttpCancellationRegistration &operator=(HttpCancellationRegistration &&other) noexcept;

    HttpCancellationRegistration(const HttpCancellationRegistration &) = delete;
    HttpCancellationRegistration &operator=(const HttpCancellationRegistration &) = delete;

    bool active() const;
    void reset() noexcept;

private:
    struct State;
    friend class HttpCancellationRegistry;

    HttpCancellationRegistration(std::shared_ptr<State> state, std::uint64_t id);

    std::shared_ptr<State> state_;
    std::uint64_t id_ = 0;
};

class HttpCancellationRegistry {
public:
    using Stop = std::function<void()>;

    HttpCancellationRegistry();

    HttpCancellationRegistration register_request(Stop stop);
    void shutdown() noexcept;
    bool shutdown_requested() const;

private:
    std::shared_ptr<HttpCancellationRegistration::State> state_;
};

}  // namespace zclaw
