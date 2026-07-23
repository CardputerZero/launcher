#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace cp0::dispatch {

class ShutdownGate {
public:
    class Lease;

    class Reservation {
    public:
        Reservation() = default;
        Reservation(const Reservation &) = delete;
        Reservation &operator=(const Reservation &) = delete;

        Reservation(Reservation &&other) noexcept
            : gate_(other.gate_), generation_(other.generation_)
        {
            other.gate_ = nullptr;
        }

        Reservation &operator=(Reservation &&other) noexcept
        {
            if (this == &other) return *this;
            release();
            gate_ = other.gate_;
            generation_ = other.generation_;
            other.gate_ = nullptr;
            return *this;
        }

        ~Reservation() { release(); }

        explicit operator bool() const noexcept { return gate_ != nullptr; }
        uint64_t generation() const noexcept { return generation_; }
        void release() noexcept
        {
            if (!gate_) return;
            ShutdownGate *gate = std::exchange(gate_, nullptr);
            gate->complete_pending();
        }

    private:
        friend class ShutdownGate;
        Reservation(ShutdownGate *gate, uint64_t generation)
            : gate_(gate), generation_(generation)
        {
        }

        ShutdownGate *gate_ = nullptr;
        uint64_t generation_ = 0;
    };

    class Lease {
    public:
        Lease() = default;
        Lease(const Lease &) = delete;
        Lease &operator=(const Lease &) = delete;

        Lease(Lease &&other) noexcept
            : gate_(other.gate_), allowed_(other.allowed_)
        {
            other.gate_ = nullptr;
            other.allowed_ = false;
        }

        Lease &operator=(Lease &&other) noexcept
        {
            if (this == &other) return *this;
            complete();
            gate_ = other.gate_;
            allowed_ = other.allowed_;
            other.gate_ = nullptr;
            other.allowed_ = false;
            return *this;
        }

        ~Lease() { complete(); }

        explicit operator bool() const noexcept { return allowed_; }

    private:
        friend class ShutdownGate;
        Lease(ShutdownGate *gate, bool allowed)
            : gate_(gate), allowed_(allowed)
        {
        }

        void complete() noexcept
        {
            if (!gate_) return;
            ShutdownGate *gate = std::exchange(gate_, nullptr);
            allowed_ = false;
            gate->complete_pending();
        }

        ShutdownGate *gate_ = nullptr;
        bool allowed_ = false;
    };

    std::optional<Reservation> reserve()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!accepting_) return std::nullopt;
        ++pending_;
        return Reservation(this, generation_);
    }

    Lease acquire(Reservation &&reservation) noexcept
    {
        ShutdownGate *gate = std::exchange(reservation.gate_, nullptr);
        if (gate != this) {
            if (gate) gate->complete_pending();
            return {};
        }
        bool allowed = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            allowed = accepting_ && reservation.generation_ == generation_;
        }
        return Lease(this, allowed);
    }

    uint64_t begin_shutdown() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        accepting_ = false;
        return ++generation_;
    }

    void resume() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        accepting_ = true;
    }

    void wait_for_drain()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        drained_.wait(lock, [this] { return pending_ == 0; });
    }

    bool accepting() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return accepting_;
    }

    uint64_t generation() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return generation_;
    }

    std::size_t pending() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_;
    }

private:
    void complete_pending() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pending_ == 0) return;
        --pending_;
        if (pending_ == 0) drained_.notify_all();
    }

    mutable std::mutex mutex_;
    std::condition_variable drained_;
    bool accepting_ = true;
    uint64_t generation_ = 1;
    std::size_t pending_ = 0;
};

} // namespace cp0::dispatch
