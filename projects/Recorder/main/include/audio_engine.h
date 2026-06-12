#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

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

    bool startRecording(const std::string& filepath);
    void pauseRecording();
    void resumeRecording();
    void stopRecording();

    bool startPlayback(const std::string& filepath);
    void pausePlayback();
    void resumePlayback();
    void stopPlayback();

    AudioState state() const;
    void poll();

    float recordingDuration() const;
    float playbackPosition() const;
    float playbackDuration() const;

    // Extended APIs
    void seekPlayback(float seconds);
    void setPlaybackSpeed(float speed);
    void getRecordingWaveform(float* out, int count) const;
    void getPlaybackWaveform(float* out, int count) const;
    bool hasPlaybackWaveform() const;

    using UpdateCallback = std::function<void()>;
    void setUpdateCallback(UpdateCallback cb);

    // Internal callbacks invoked from miniaudio's audio thread
    void onRecordingData(const void* input, ma_uint32 frameCount);
    void onPlaybackData(void* output, ma_uint32 frameCount);

private:
    std::unique_ptr<AudioEngineImpl> impl_;
};
