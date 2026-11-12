/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "cp0_pulse_client.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include <dlfcn.h>

namespace cp0::pulse {
namespace {

using pa_volume_t = std::uint32_t;

constexpr std::uint32_t kPaVolumeNorm = 0x10000U;
constexpr std::size_t kPaChannelsMax = 32;
constexpr int kPaContextReady = 4;
constexpr int kPaContextNoAutospawn = 0x0001;
constexpr int kPaOperationCancelled = 2;
constexpr auto kRequestTimeout = std::chrono::milliseconds(2000);

struct pa_mainloop;
struct pa_context;
struct pa_operation;
struct pa_mainloop_api;

// The declarations below mirror the stable public ABI of libpulse.so.0. Only
// the leading fields this file reads are declared; the server fills the full
// structures. The layout is checked at compile time below.
struct PaSampleSpec {
    int format;
    std::uint32_t rate;
    std::uint8_t channels;
};

struct PaChannelMap {
    std::uint8_t channels;
    int map[kPaChannelsMax];
};

struct PaCVolume {
    std::uint8_t channels;
    pa_volume_t values[kPaChannelsMax];
};

struct PaSinkInfo {
    const char *name;
    std::uint32_t index;
    const char *description;
    PaSampleSpec sample_spec;
    PaChannelMap channel_map;
    std::uint32_t owner_module;
    PaCVolume volume;
    int mute;
    // The remaining fields of the real pa_sink_info are not accessed.
};

struct PaServerInfo {
    const char *user_name;
    const char *host_name;
    const char *server_version;
    const char *server_name;
    PaSampleSpec sample_spec;
    const char *default_sink_name;
    // The remaining fields of the real pa_server_info are not accessed.
};

static_assert(sizeof(void *) == 8, "the libpulse ABI declarations below assume a 64-bit target");
static_assert(sizeof(PaSampleSpec) == 12, "unexpected pa_sample_spec layout");
static_assert(sizeof(PaChannelMap) == 4 + kPaChannelsMax * sizeof(int), "unexpected pa_channel_map layout");
static_assert(sizeof(PaCVolume) == 4 + kPaChannelsMax * sizeof(pa_volume_t), "unexpected pa_cvolume layout");
static_assert(offsetof(PaSinkInfo, volume) == 172, "unexpected pa_sink_info layout");
static_assert(offsetof(PaSinkInfo, mute) == 304, "unexpected pa_sink_info layout");
static_assert(offsetof(PaServerInfo, default_sink_name) == 48, "unexpected pa_server_info layout");

using SinkInfoCallback = void (*)(pa_context *, const PaSinkInfo *, int, void *);
using ServerInfoCallback = void (*)(pa_context *, const PaServerInfo *, void *);
using SuccessCallback = void (*)(pa_context *, int, void *);

struct PulseApi {
    void *handle = nullptr;
    pa_mainloop *(*mainloop_new)() = nullptr;
    pa_mainloop_api *(*mainloop_get_api)(pa_mainloop *) = nullptr;
    int (*mainloop_prepare)(pa_mainloop *, int) = nullptr;
    int (*mainloop_poll)(pa_mainloop *) = nullptr;
    int (*mainloop_dispatch)(pa_mainloop *) = nullptr;
    void (*mainloop_free)(pa_mainloop *) = nullptr;
    pa_context *(*context_new)(pa_mainloop_api *, const char *) = nullptr;
    int (*context_connect)(pa_context *, const char *, int, const void *) = nullptr;
    int (*context_get_state)(pa_context *) = nullptr;
    void (*context_disconnect)(pa_context *) = nullptr;
    void (*context_unref)(pa_context *) = nullptr;
    pa_operation *(*get_server_info)(pa_context *, ServerInfoCallback, void *) = nullptr;
    pa_operation *(*get_sink_info_by_name)(pa_context *, const char *, SinkInfoCallback, void *) = nullptr;
    pa_operation *(*set_sink_volume_by_name)(pa_context *, const char *, const PaCVolume *, SuccessCallback,
                                             void *) = nullptr;
    pa_operation *(*set_sink_mute_by_name)(pa_context *, const char *, int, SuccessCallback, void *) = nullptr;
    int (*operation_get_state)(pa_operation *) = nullptr;
    void (*operation_unref)(pa_operation *) = nullptr;

    bool complete() const
    {
        return handle && mainloop_new && mainloop_get_api && mainloop_prepare && mainloop_poll &&
               mainloop_dispatch && mainloop_free && context_new && context_connect && context_get_state &&
               context_disconnect && context_unref && get_server_info && get_sink_info_by_name &&
               set_sink_volume_by_name && set_sink_mute_by_name && operation_get_state && operation_unref;
    }
};

template <typename Function>
Function resolve_symbol(void *handle, const char *name)
{
    Function function = nullptr;
    void *symbol = dlsym(handle, name);
    static_assert(sizeof(function) == sizeof(symbol), "function pointer size mismatch");
    std::memcpy(&function, &symbol, sizeof(function));
    return function;
}

const PulseApi *pulse_api()
{
    static const PulseApi api = [] {
        PulseApi loaded;
        loaded.handle = dlopen("libpulse.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!loaded.handle) return loaded;

        loaded.mainloop_new = resolve_symbol<decltype(loaded.mainloop_new)>(loaded.handle, "pa_mainloop_new");
        loaded.mainloop_get_api =
            resolve_symbol<decltype(loaded.mainloop_get_api)>(loaded.handle, "pa_mainloop_get_api");
        loaded.mainloop_prepare =
            resolve_symbol<decltype(loaded.mainloop_prepare)>(loaded.handle, "pa_mainloop_prepare");
        loaded.mainloop_poll = resolve_symbol<decltype(loaded.mainloop_poll)>(loaded.handle, "pa_mainloop_poll");
        loaded.mainloop_dispatch =
            resolve_symbol<decltype(loaded.mainloop_dispatch)>(loaded.handle, "pa_mainloop_dispatch");
        loaded.mainloop_free = resolve_symbol<decltype(loaded.mainloop_free)>(loaded.handle, "pa_mainloop_free");
        loaded.context_new = resolve_symbol<decltype(loaded.context_new)>(loaded.handle, "pa_context_new");
        loaded.context_connect =
            resolve_symbol<decltype(loaded.context_connect)>(loaded.handle, "pa_context_connect");
        loaded.context_get_state =
            resolve_symbol<decltype(loaded.context_get_state)>(loaded.handle, "pa_context_get_state");
        loaded.context_disconnect =
            resolve_symbol<decltype(loaded.context_disconnect)>(loaded.handle, "pa_context_disconnect");
        loaded.context_unref = resolve_symbol<decltype(loaded.context_unref)>(loaded.handle, "pa_context_unref");
        loaded.get_server_info =
            resolve_symbol<decltype(loaded.get_server_info)>(loaded.handle, "pa_context_get_server_info");
        loaded.get_sink_info_by_name = resolve_symbol<decltype(loaded.get_sink_info_by_name)>(
            loaded.handle, "pa_context_get_sink_info_by_name");
        loaded.set_sink_volume_by_name = resolve_symbol<decltype(loaded.set_sink_volume_by_name)>(
            loaded.handle, "pa_context_set_sink_volume_by_name");
        loaded.set_sink_mute_by_name = resolve_symbol<decltype(loaded.set_sink_mute_by_name)>(
            loaded.handle, "pa_context_set_sink_mute_by_name");
        loaded.operation_get_state =
            resolve_symbol<decltype(loaded.operation_get_state)>(loaded.handle, "pa_operation_get_state");
        loaded.operation_unref =
            resolve_symbol<decltype(loaded.operation_unref)>(loaded.handle, "pa_operation_unref");

        if (!loaded.complete()) {
            (void)dlclose(loaded.handle);
            loaded = PulseApi{};
        }
        return loaded;
    }();
    return api.handle ? &api : nullptr;
}

struct NameState {
    bool done = false;
    bool ok = false;
    std::string name;
};

struct SinkState {
    bool done = false;
    bool ok = false;
    std::uint8_t channels = 0;
    std::uint32_t volume = 0;
    int mute = 0;
};

struct SuccessState {
    bool done = false;
    bool ok = false;
};

void server_info_cb(pa_context *, const PaServerInfo *info, void *userdata)
{
    auto *state = static_cast<NameState *>(userdata);
    if (info && info->default_sink_name && info->default_sink_name[0]) {
        state->name = info->default_sink_name;
        state->ok = true;
    }
    state->done = true;
}

void sink_info_cb(pa_context *, const PaSinkInfo *info, int eol, void *userdata)
{
    auto *state = static_cast<SinkState *>(userdata);
    if (eol < 0) {
        state->done = true;
        state->ok = false;
        return;
    }
    if (info) {
        const auto channels = std::min<std::size_t>(info->volume.channels, kPaChannelsMax);
        state->channels = static_cast<std::uint8_t>(channels);
        state->volume = info->volume.values[0];
        state->mute = info->mute;
        state->ok = state->channels > 0;
    }
    if (eol > 0) state->done = true;
}

void success_cb(pa_context *, int success, void *userdata)
{
    auto *state = static_cast<SuccessState *>(userdata);
    state->done = true;
    state->ok = success != 0;
}

class Session {
public:
    Session() = default;
    ~Session() { close(); }

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    bool open()
    {
        api_ = pulse_api();
        if (!api_) return false;

        mainloop_ = api_->mainloop_new();
        if (!mainloop_) return false;

        pa_mainloop_api *mainloop_api = api_->mainloop_get_api(mainloop_);
        if (!mainloop_api) return false;

        context_ = api_->context_new(mainloop_api, "APPLaunch");
        if (!context_) return false;

        deadline_ = std::chrono::steady_clock::now() + kRequestTimeout;
        if (api_->context_connect(context_, nullptr, kPaContextNoAutospawn, nullptr) < 0) return false;

        for (;;) {
            const int state = api_->context_get_state(context_);
            if (state == kPaContextReady) return true;
            if (state < 0 || state > kPaContextReady) return false;
            if (!pump()) return false;
        }
    }

    bool default_sink(std::string &name)
    {
        NameState state;
        if (!run(api_->get_server_info(context_, &server_info_cb, &state), state)) return false;
        name = std::move(state.name);
        return true;
    }

    bool sink(const std::string &name)
    {
        SinkState state;
        if (!run(api_->get_sink_info_by_name(context_, name.c_str(), &sink_info_cb, &state), state)) return false;
        sink_channels_ = state.channels;
        sink_volume_ = state.volume;
        sink_mute_ = state.mute;
        return true;
    }

    bool set_volume(const std::string &name, int percent)
    {
        if (sink_channels_ == 0) return false;

        PaCVolume volume{};
        volume.channels = sink_channels_;
        const pa_volume_t target = volume_scale_from_percent(percent);
        for (std::uint8_t channel = 0; channel < sink_channels_; ++channel) volume.values[channel] = target;

        SuccessState state;
        return run(api_->set_sink_volume_by_name(context_, name.c_str(), &volume, &success_cb, &state), state);
    }

    bool set_mute(const std::string &name, bool muted)
    {
        SuccessState state;
        return run(api_->set_sink_mute_by_name(context_, name.c_str(), muted ? 1 : 0, &success_cb, &state), state);
    }

    int volume_percent() const { return percent_from_volume_scale(sink_volume_); }
    bool muted() const { return sink_mute_ != 0; }

private:
    bool pump()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline_) return false;
        const auto remaining_us =
            std::chrono::duration_cast<std::chrono::microseconds>(deadline_ - now).count();
        int slice_us = static_cast<int>(std::min<std::int64_t>(remaining_us, 100000));
        if (slice_us < 1000) slice_us = 1000;

        if (api_->mainloop_prepare(mainloop_, slice_us) < 0) return false;
        if (api_->mainloop_poll(mainloop_) < 0) return false;
        if (api_->mainloop_dispatch(mainloop_) < 0) return false;
        return true;
    }

    template <typename State>
    bool run(pa_operation *operation, State &state)
    {
        if (!operation) return false;
        while (!state.done) {
            if (api_->operation_get_state(operation) == kPaOperationCancelled) break;
            if (!pump()) break;
        }
        api_->operation_unref(operation);
        return state.done && state.ok;
    }

    void close()
    {
        if (!api_) return;
        if (context_) {
            api_->context_disconnect(context_);
            api_->context_unref(context_);
            context_ = nullptr;
        }
        if (mainloop_) {
            api_->mainloop_free(mainloop_);
            mainloop_ = nullptr;
        }
    }

    const PulseApi *api_ = nullptr;
    pa_mainloop *mainloop_ = nullptr;
    pa_context *context_ = nullptr;
    std::chrono::steady_clock::time_point deadline_{};
    std::uint8_t sink_channels_ = 0;
    std::uint32_t sink_volume_ = 0;
    int sink_mute_ = 0;
};

} // namespace

int percent_from_volume_scale(std::uint32_t volume)
{
    const std::uint64_t percent =
        (static_cast<std::uint64_t>(volume) * 100 + kPaVolumeNorm / 2) / kPaVolumeNorm;
    return static_cast<int>(std::min<std::uint64_t>(percent, std::numeric_limits<int>::max()));
}

std::uint32_t volume_scale_from_percent(int percent)
{
    const int clamped = std::clamp(percent, 0, 100);
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(clamped) * kPaVolumeNorm / 100);
}

bool preload()
{
    return pulse_api() != nullptr;
}

bool default_sink_name(std::string &name)
{
    Session session;
    if (!session.open()) return false;
    return session.default_sink(name);
}

int read_default_sink_volume()
{
    Session session;
    if (!session.open()) return -1;

    std::string name;
    if (!session.default_sink(name)) return -1;
    if (!session.sink(name)) return -1;
    return session.volume_percent();
}

int write_default_sink_volume(int percent)
{
    const int clamped = std::clamp(percent, 0, 100);

    Session session;
    if (!session.open()) return -1;

    std::string name;
    if (!session.default_sink(name)) return -1;
    if (!session.sink(name)) return -1;
    return session.set_volume(name, clamped) ? clamped : -1;
}

int read_default_sink_mute()
{
    Session session;
    if (!session.open()) return -1;

    std::string name;
    if (!session.default_sink(name)) return -1;
    if (!session.sink(name)) return -1;
    return session.muted() ? 1 : 0;
}

int toggle_default_sink_mute()
{
    Session session;
    if (!session.open()) return -1;

    std::string name;
    if (!session.default_sink(name)) return -1;
    if (!session.sink(name)) return -1;

    const bool target = !session.muted();
    if (!session.set_mute(name, target)) return -1;
    return target ? 1 : 0;
}

} // namespace cp0::pulse
