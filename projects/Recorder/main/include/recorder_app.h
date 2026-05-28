#pragma once

#include "audio_engine.h"
#include <atomic>
#include <future>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

enum class AppState {
    Idle,
    Recording,
    RecPaused,
    Playing,
    PlayPaused,
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
    void handleToggleRecord();
    void handleTogglePlay();
    void handleTogglePause();
    void handleStop();
    void handlePrevFile();
    void handleNextFile();

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
};
