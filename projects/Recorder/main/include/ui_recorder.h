#pragma once

#include "recorder_app.h"
#include "lvgl/lvgl.h"
#include <array>
#include <string>

enum class UiPage {
    Home,
    FileList,
    Recording,
    RecPaused,
    SaveConfirm,
    Playback,
};

class UiRecorder : public IRecorderView {
public:
    UiRecorder() = default;

    void init(lv_obj_t* parent);
    void update(const RecorderState& state) override;
    void setActionHandler(std::function<void(const std::string&)> handler) override;

    // Keyboard event from input system
    void onKeyPressed(uint32_t key_code);

private:
    void buildUi(lv_obj_t* parent);
    void createStatusBar(lv_obj_t* parent);
    void createBottomBar(lv_obj_t* parent);

    lv_obj_t* createPageContainer(lv_obj_t* parent);
    void createPageHome(lv_obj_t* page);
    void createPageFileList(lv_obj_t* page);
    void createPageRecording(lv_obj_t* page);
    void createPageRecPaused(lv_obj_t* page);
    void createPageSaveConfirm(lv_obj_t* page);
    void createPagePlayback(lv_obj_t* page);

    void switchPage(UiPage page);
    void updatePageContent(const RecorderState& state);
    void updateBottomLabels(UiPage page, bool isPaused);
    void handleActionForPage(UiPage page, int btnIndex);

    lv_obj_t* parent_ = nullptr;
    UiPage currentPage_ = UiPage::Home;

    // Status bar
    lv_obj_t* statusBar_ = nullptr;
    lv_obj_t* lblStatusText_ = nullptr;

    // Bottom bar
    lv_obj_t* bottomBar_ = nullptr;
    std::array<lv_obj_t*, 5> lblBottomBtns_{};

    // Pages
    std::array<lv_obj_t*, 6> pages_{};

    // Home
    lv_obj_t* lblHomeReady_ = nullptr;

    // FileList
    lv_obj_t* lblFileListEmpty_ = nullptr;
    std::array<lv_obj_t*, 5> lblFileListItems_{};
    int fileListOffset_ = 0;

    // Recording
    lv_obj_t* lblRecFilename_ = nullptr;
    lv_obj_t* recWaveContainer_ = nullptr;
    lv_obj_t* recWaveLine_ = nullptr;
    std::array<lv_point_precise_t, 128> recWavePoints_{};
    lv_obj_t* lblRecTimer_ = nullptr;

    // RecPaused
    lv_obj_t* lblPauseFilename_ = nullptr;
    lv_obj_t* pauseLine_ = nullptr;
    lv_obj_t* lblPauseTimer_ = nullptr;

    // SaveConfirm
    lv_obj_t* lblSaveFilename_ = nullptr;

    // Playback
    lv_obj_t* lblPlayFilename_ = nullptr;
    lv_obj_t* playWaveContainer_ = nullptr;
    lv_obj_t* playWaveLine_ = nullptr;
    lv_obj_t* playProgressLine_ = nullptr;
    std::array<lv_point_precise_t, 256> playWavePoints_{};
    lv_point_precise_t playProgressPoints_[2]{};
    lv_obj_t* lblPlayTimer_ = nullptr;

    // State cache
    RecorderState lastState_{};
    bool isPlaybackPaused_ = false;

    uint32_t lastKeyTime_ = 0;
    uint32_t lastKeyCode_ = 0;
    std::function<void(const std::string&)> actionHandler_;
};
