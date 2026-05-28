#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// miniaudio forward-declares + types (header-only, declarations are lightweight)
#include "miniaudio.h"

enum class AudioState {
    Idle,
    Recording,
    RecPaused,
    Playing,
    PlayPaused,
};

class AudioEngineImpl;

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool initialize();
    void shutdown();

    // Recording
    bool startRecording(const std::string& filepath);
    void pauseRecording();
    void resumeRecording();
    void stopRecording(); // finalizes WAV file

    // Playback
    bool startPlayback(const std::string& filepath);
    void pausePlayback();
    void resumePlayback();
    void stopPlayback();

    AudioState state() const;

    // Poll from main thread to handle async events (e.g. playback finished)
    void poll();

    // Duration in seconds
    float recordingDuration() const;
    float playbackPosition() const;
    float playbackDuration() const;

    // UI update callback (called from audio thread, must be non-blocking)
    using UpdateCallback = std::function<void()>;
    void setUpdateCallback(UpdateCallback cb);

    // PIMPL - exposed for C callbacks only (AudioEngineImpl is opaque outside audio_engine.cpp)
    std::unique_ptr<AudioEngineImpl> impl_;
};
