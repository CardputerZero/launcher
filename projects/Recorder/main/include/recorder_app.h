#pragma once

#include "audio_engine.h"
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

class RecorderApp {
public:
    static RecorderApp& instance();

    bool init();
    void deinit();

    // Actions
    void toggleRecord();      // Enter: start/stop recording
    void pauseResumeRecord(); // P: pause/resume recording
    void togglePlay();        // Space: play/pause playback
    void stop();              // S: stop current operation
    void prevFile();
    void nextFile();

    // State queries
    AppState state() const;
    std::string statusText() const;
    std::string timerText() const;
    std::string currentFileName() const;
    std::vector<RecordingInfo> fileList() const;

    // Audio engine access for UI callbacks
    AudioEngine& engine() { return engine_; }

    // Scan recordings directory
    void scanFiles();

private:
    RecorderApp() = default;
    ~RecorderApp() = default;
    RecorderApp(const RecorderApp&) = delete;
    RecorderApp& operator=(const RecorderApp&) = delete;

    std::string generateFilename();
    std::string recordingsDir();

    AudioEngine engine_;
    std::vector<RecordingInfo> files_;
    int currentFileIndex_ = -1;
    std::string lastRecordingPath_;
};
