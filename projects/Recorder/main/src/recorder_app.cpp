#include "recorder_app.h"
#include "wav_file.h"
#include "compat/input_keys.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

RecorderApp& RecorderApp::instance()
{
    static RecorderApp app;
    return app;
}

bool RecorderApp::init()
{
    engine_.initialize();
    scanFiles();
    return true;
}

void RecorderApp::deinit()
{
    engine_.shutdown();
}

void RecorderApp::toggleRecord()
{
    AppState s = state();
    if (s == AppState::Idle) {
        lastRecordingPath_ = generateFilename();
        if (engine_.startRecording(lastRecordingPath_)) {
            scanFiles();
        }
    } else if (s == AppState::Recording || s == AppState::RecPaused) {
        engine_.stopRecording();
        scanFiles();
    } else if (s == AppState::Playing || s == AppState::PlayPaused) {
        engine_.stopPlayback();
        lastRecordingPath_ = generateFilename();
        engine_.startRecording(lastRecordingPath_);
    }
}

void RecorderApp::pauseResumeRecord()
{
    AppState s = state();
    if (s == AppState::Recording) {
        engine_.pauseRecording();
    } else if (s == AppState::RecPaused) {
        engine_.resumeRecording();
    }
}

void RecorderApp::togglePlay()
{
    AppState s = state();
    if (s == AppState::Idle) {
        if (files_.empty()) return;
        if (currentFileIndex_ < 0 || currentFileIndex_ >= static_cast<int>(files_.size())) {
            currentFileIndex_ = static_cast<int>(files_.size()) - 1;
        }
        engine_.startPlayback(files_[currentFileIndex_].filepath);
    } else if (s == AppState::Playing) {
        engine_.pausePlayback();
    } else if (s == AppState::PlayPaused) {
        engine_.resumePlayback();
    } else if (s == AppState::Recording || s == AppState::RecPaused) {
        engine_.stopRecording();
        scanFiles();
        if (files_.empty()) return;
        if (currentFileIndex_ < 0 || currentFileIndex_ >= static_cast<int>(files_.size())) {
            currentFileIndex_ = static_cast<int>(files_.size()) - 1;
        }
        engine_.startPlayback(files_[currentFileIndex_].filepath);
    }
}

void RecorderApp::stop()
{
    AppState s = state();
    if (s == AppState::Recording || s == AppState::RecPaused) {
        engine_.stopRecording();
        scanFiles();
    } else if (s == AppState::Playing || s == AppState::PlayPaused) {
        engine_.stopPlayback();
    }
}

void RecorderApp::prevFile()
{
    if (files_.empty()) return;
    if (currentFileIndex_ > 0) currentFileIndex_--;
    else currentFileIndex_ = static_cast<int>(files_.size()) - 1;
}

void RecorderApp::nextFile()
{
    if (files_.empty()) return;
    if (currentFileIndex_ < static_cast<int>(files_.size()) - 1) currentFileIndex_++;
    else currentFileIndex_ = 0;
}

AppState RecorderApp::state() const
{
    switch (engine_.state()) {
        case AudioState::Idle:       return AppState::Idle;
        case AudioState::Recording:  return AppState::Recording;
        case AudioState::RecPaused:  return AppState::RecPaused;
        case AudioState::Playing:    return AppState::Playing;
        case AudioState::PlayPaused: return AppState::PlayPaused;
    }
    return AppState::Idle;
}

std::string RecorderApp::statusText() const
{
    switch (state()) {
        case AppState::Idle:       return "IDLE";
        case AppState::Recording:  return "RECORDING";
        case AppState::RecPaused:  return "REC PAUSED";
        case AppState::Playing:    return "PLAYING";
        case AppState::PlayPaused: return "PLAY PAUSED";
    }
    return "UNKNOWN";
}

std::string RecorderApp::timerText() const
{
    std::ostringstream oss;
    switch (state()) {
        case AppState::Recording:
        case AppState::RecPaused: {
            float d = engine_.recordingDuration();
            int mins = static_cast<int>(d) / 60;
            int secs = static_cast<int>(d) % 60;
            oss << std::setfill('0') << std::setw(2) << mins << ":"
                << std::setfill('0') << std::setw(2) << secs;
            break;
        }
        case AppState::Playing:
        case AppState::PlayPaused: {
            float pos = engine_.playbackPosition();
            float dur = engine_.playbackDuration();
            int pmin = static_cast<int>(pos) / 60;
            int psec = static_cast<int>(pos) % 60;
            int dmin = static_cast<int>(dur) / 60;
            int dsec = static_cast<int>(dur) % 60;
            oss << std::setfill('0') << std::setw(2) << pmin << ":"
                << std::setfill('0') << std::setw(2) << psec << " / "
                << std::setfill('0') << std::setw(2) << dmin << ":"
                << std::setfill('0') << std::setw(2) << dsec;
            break;
        }
        default:
            break;
    }
    return oss.str();
}

std::string RecorderApp::currentFileName() const
{
    if (state() == AppState::Recording || state() == AppState::RecPaused) {
        size_t pos = lastRecordingPath_.find_last_of('/');
        return lastRecordingPath_.substr(pos + 1);
    }
    if (state() == AppState::Playing || state() == AppState::PlayPaused) {
        if (currentFileIndex_ >= 0 && currentFileIndex_ < static_cast<int>(files_.size())) {
            return files_[currentFileIndex_].filename;
        }
    }
    if (!files_.empty() && currentFileIndex_ >= 0 && currentFileIndex_ < static_cast<int>(files_.size())) {
        return files_[currentFileIndex_].filename;
    }
    return "No recordings";
}

std::vector<RecordingInfo> RecorderApp::fileList() const
{
    return files_;
}

std::string RecorderApp::recordingsDir()
{
    const char* home = getenv("HOME");
    if (!home) return "/tmp/recordings";

    std::string musicDir = std::string(home) + "/Music";
    std::string dir = musicDir + "/Recorder";

    struct stat st;
    if (stat(musicDir.c_str(), &st) != 0) {
        mkdir(musicDir.c_str(), 0755);
    }
    if (stat(dir.c_str(), &st) != 0) {
        mkdir(dir.c_str(), 0755);
    }
    return dir;
}

std::string RecorderApp::generateFilename()
{
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&t, &tm);

    std::ostringstream oss;
    oss << recordingsDir() << "/rec_"
        << (tm.tm_year + 1900)
        << std::setfill('0') << std::setw(2) << (tm.tm_mon + 1)
        << std::setfill('0') << std::setw(2) << tm.tm_mday << "_"
        << std::setfill('0') << std::setw(2) << tm.tm_hour
        << std::setfill('0') << std::setw(2) << tm.tm_min
        << std::setfill('0') << std::setw(2) << tm.tm_sec
        << ".wav";
    return oss.str();
}

void RecorderApp::scanFiles()
{
    files_.clear();
    std::string dir = recordingsDir();
    DIR* d = opendir(dir.c_str());
    if (!d) return;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN) continue;
        size_t len = strlen(entry->d_name);
        if (len < 5 || strcmp(entry->d_name + len - 4, ".wav") != 0) continue;

        std::string path = dir + "/" + entry->d_name;
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) continue;

        uint16_t channels = 0;
        uint32_t sampleRate = 0;
        uint32_t dataSize = 0;
        if (wav_file_read_header(f, channels, sampleRate, dataSize)) {
            RecordingInfo info;
            info.filename = entry->d_name;
            info.filepath = path;
            info.duration = static_cast<float>(dataSize) / (sampleRate * channels * sizeof(int16_t));
            files_.push_back(info);
        }
        fclose(f);
    }
    closedir(d);

    std::sort(files_.begin(), files_.end(),
              [](const RecordingInfo& a, const RecordingInfo& b) { return a.filename > b.filename; });

    if (!files_.empty() && currentFileIndex_ < 0) {
        currentFileIndex_ = 0;
    }
}
