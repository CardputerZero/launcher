#include "recorder_app.h"
#include "wav_file.h"
#include "compat/input_keys.h"
#include "lvgl/lvgl.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

RecorderApp::RecorderApp() = default;
RecorderApp::~RecorderApp() = default;

bool RecorderApp::init()
{
    engine_.initialize();
    scanFiles();
    lastAudioState_ = engine_.state();
    return true;
}

void RecorderApp::deinit()
{
    if (scanFuture_.valid()) {
        scanFuture_.wait();
    }
    engine_.shutdown();
}

void RecorderApp::setView(IRecorderView* view)
{
    view_ = view;
    notifyView();
}

void RecorderApp::onAction(const std::string& action)
{
    if (action == "toggle_record") {
        handleToggleRecord();
    } else if (action == "toggle_play") {
        handleTogglePlay();
    } else if (action == "toggle_pause") {
        handleTogglePause();
    } else if (action == "stop") {
        handleStop();
    } else if (action == "prev_file") {
        handlePrevFile();
    } else if (action == "next_file") {
        handleNextFile();
    }
    notifyView();
}

void RecorderApp::poll()
{
    engine_.poll();

    uint32_t now = lv_tick_get();
    bool needNotify = false;

    if (now - lastNotifyTime_ >= 200) {
        lastNotifyTime_ = now;
        needNotify = true;
    }

    if (filesDirty_.exchange(false)) {
        needNotify = true;
    }

    AudioState current = engine_.state();
    if (current != lastAudioState_) {
        lastAudioState_ = current;
        needNotify = true;
    }

    if (needNotify) {
        notifyView();
    }
}

RecorderState RecorderApp::getState() const
{
    RecorderState rs;
    rs.state = [this]() {
        switch (engine_.state()) {
            case AudioState::Idle:       return AppState::Idle;
            case AudioState::Recording:  return AppState::Recording;
            case AudioState::RecPaused:  return AppState::RecPaused;
            case AudioState::Playing:    return AppState::Playing;
            case AudioState::PlayPaused: return AppState::PlayPaused;
        }
        return AppState::Idle;
    }();

    switch (rs.state) {
        case AppState::Idle:       rs.statusText = "IDLE"; break;
        case AppState::Recording:  rs.statusText = "RECORDING"; break;
        case AppState::RecPaused:  rs.statusText = "REC PAUSED"; break;
        case AppState::Playing:    rs.statusText = "PLAYING"; break;
        case AppState::PlayPaused: rs.statusText = "PLAY PAUSED"; break;
    }

    std::ostringstream oss;
    switch (rs.state) {
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
    rs.timerText = oss.str();

    {
        std::lock_guard<std::mutex> lock(filesMutex_);
        rs.fileList = files_;
        rs.selectedFileIndex = currentFileIndex_;

        if (rs.state == AppState::Recording || rs.state == AppState::RecPaused) {
            size_t pos = lastRecordingPath_.find_last_of('/');
            rs.currentFileName = lastRecordingPath_.substr(pos + 1);
        } else if (!files_.empty() && currentFileIndex_ >= 0 && currentFileIndex_ < static_cast<int>(files_.size())) {
            rs.currentFileName = files_[currentFileIndex_].filename;
        } else {
            rs.currentFileName = "No recordings";
        }
    }

    switch (rs.state) {
        case AppState::Idle:       rs.hintText = "[Enter]Rec [Space]Play [<- ->]File [Esc]Quit"; break;
        case AppState::Recording:  rs.hintText = "[P]Pause [Enter]Stop"; break;
        case AppState::RecPaused:  rs.hintText = "[P]Resume [Enter]Stop"; break;
        case AppState::Playing:    rs.hintText = "[Space]Pause [S]Stop [<- ->]File"; break;
        case AppState::PlayPaused: rs.hintText = "[Space]Resume [S]Stop"; break;
    }

    return rs;
}

void RecorderApp::notifyView()
{
    if (view_) {
        view_->update(getState());
    }
}

void RecorderApp::handleToggleRecord()
{
    switch (engine_.state()) {
        case AudioState::Idle:
            lastRecordingPath_ = generateFilename();
            engine_.startRecording(lastRecordingPath_);
            asyncScanFiles();
            break;
        case AudioState::Recording:
        case AudioState::RecPaused:
            engine_.stopRecording();
            asyncScanFiles();
            break;
        case AudioState::Playing:
        case AudioState::PlayPaused:
            engine_.stopPlayback();
            lastRecordingPath_ = generateFilename();
            engine_.startRecording(lastRecordingPath_);
            break;
    }
}

void RecorderApp::handleTogglePlay()
{
    switch (engine_.state()) {
        case AudioState::Idle:
            if (files_.empty()) return;
            clampFileIndex();
            engine_.startPlayback(files_[currentFileIndex_].filepath);
            break;
        case AudioState::Playing:
            engine_.pausePlayback();
            break;
        case AudioState::PlayPaused:
            engine_.resumePlayback();
            break;
        case AudioState::Recording:
        case AudioState::RecPaused:
            engine_.stopRecording();
            asyncScanFiles();
            if (files_.empty()) return;
            clampFileIndex();
            engine_.startPlayback(files_[currentFileIndex_].filepath);
            break;
    }
}

void RecorderApp::handleTogglePause()
{
    switch (engine_.state()) {
        case AudioState::Recording:
            engine_.pauseRecording();
            break;
        case AudioState::RecPaused:
            engine_.resumeRecording();
            break;
        case AudioState::Playing:
            engine_.pausePlayback();
            break;
        case AudioState::PlayPaused:
            engine_.resumePlayback();
            break;
        default:
            break;
    }
}

void RecorderApp::handleStop()
{
    switch (engine_.state()) {
        case AudioState::Recording:
        case AudioState::RecPaused:
            engine_.stopRecording();
            asyncScanFiles();
            break;
        case AudioState::Playing:
        case AudioState::PlayPaused:
            engine_.stopPlayback();
            break;
        default:
            break;
    }
}

void RecorderApp::handlePrevFile()
{
    std::lock_guard<std::mutex> lock(filesMutex_);
    if (files_.empty()) return;
    if (currentFileIndex_ > 0) currentFileIndex_--;
    else currentFileIndex_ = static_cast<int>(files_.size()) - 1;
}

void RecorderApp::handleNextFile()
{
    std::lock_guard<std::mutex> lock(filesMutex_);
    if (files_.empty()) return;
    if (currentFileIndex_ < static_cast<int>(files_.size()) - 1) currentFileIndex_++;
    else currentFileIndex_ = 0;
}

void RecorderApp::clampFileIndex()
{
    std::lock_guard<std::mutex> lock(filesMutex_);
    if (currentFileIndex_ < 0 || currentFileIndex_ >= static_cast<int>(files_.size())) {
        currentFileIndex_ = files_.empty() ? -1 : static_cast<int>(files_.size()) - 1;
    }
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
    std::vector<RecordingInfo> result;
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
            result.push_back(info);
        }
        fclose(f);
    }
    closedir(d);

    std::sort(result.begin(), result.end(),
              [](const RecordingInfo& a, const RecordingInfo& b) { return a.filename > b.filename; });

    {
        std::lock_guard<std::mutex> lock(filesMutex_);
        files_ = std::move(result);
        if (!files_.empty() && currentFileIndex_ < 0) {
            currentFileIndex_ = 0;
        }
    }
}

void RecorderApp::asyncScanFiles()
{
    if (scanFuture_.valid()) {
        scanFuture_.wait();
    }
    scanFuture_ = std::async(std::launch::async, [this]() {
        scanFiles();
        filesDirty_.store(true);
    });
}
