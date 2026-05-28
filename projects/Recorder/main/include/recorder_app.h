#pragma once

#include "audio_engine.h"
#include <array>
#include <atomic>
#include <future>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

enum class AppState {
    Idle,         // Home entry page
    Recording,    // Recording in progress
    RecPaused,    // Recording paused
    SaveConfirm,  // Save recording dialog
    FileList,     // Saved recordings list
    Playing,      // Playback in progress
    PlayPaused,   // Playback paused
};

struct RecordingInfo {
    std::string filename;
    std::string filepath;
    float duration;
};

struct RecorderState {
    AppState state;
    std::string statusText;
    std::string timerText;
    std::string currentFileName;
    std::string hintText;
    std::vector<RecordingInfo> fileList;
    int selectedFileIndex;
    std::string sampleRate;      // e.g. "48k"
    std::string playbackSpeed;   // e.g. "1.0X"
    float playbackPosition = 0;  // seconds
    float playbackDuration = 0;  // seconds
    std::array<float, 128> recWaveform{};
    std::array<float, 256> playWaveform{};
    bool hasPlayWaveform = false;
};

class IRecorderView {
public:
    virtual ~IRecorderView() = default;
    virtual void update(const RecorderState& state) = 0;
    virtual void setActionHandler(std::function<void(const std::string&)> handler) = 0;
};

class RecorderApp {
public:
    RecorderApp();
    ~RecorderApp();

    bool init();
    void deinit();

    void setView(IRecorderView* view);

    // Unified action entry point
    void onAction(const std::string& action);

    // Call from main loop
    void poll();

    // Build current state snapshot
    RecorderState getState() const;

private:
    void notifyView();
    void syncStateFromEngine();

    void handleToggleRecord();
    void handleTogglePlay();
    void handleTogglePause();
    void handleStop();
    void handlePrevFile();
    void handleNextFile();
    void handleDelete();

    void scanFiles();
    void asyncScanFiles();
    std::string generateFilename();
    std::string recordingsDir();
    void clampFileIndex();

    AudioEngine engine_;
    IRecorderView* view_ = nullptr;

    mutable std::mutex filesMutex_;
    std::vector<RecordingInfo> files_;
    int currentFileIndex_ = -1;
    std::string lastRecordingPath_;

    std::future<void> scanFuture_;
    std::atomic<bool> filesDirty_{false};
    AudioState lastAudioState_ = AudioState::Idle;
    uint32_t lastNotifyTime_ = 0;

    AppState appState_ = AppState::Idle;
    std::string sampleRate_ = "48k";
    std::string playbackSpeed_ = "1.0X";
};
