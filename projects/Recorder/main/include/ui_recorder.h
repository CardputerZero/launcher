#pragma once

#include "recorder_app.h"
#include "lvgl/lvgl.h"
#include <array>
#include <string>

enum class UiPage {
    Home,
    FileList,
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
    void onKeyPressed(uint32_t key_code, bool isRepeat = false);
    void onCharTyped(uint32_t codepoint);

private:
    void buildUi(lv_obj_t* parent);
    void createStatusBar(lv_obj_t* parent);
    void createBottomBar(lv_obj_t* parent);

    lv_obj_t* createPageContainer(lv_obj_t* parent);
    void createPageHome(lv_obj_t* page);
    void createPageFileList(lv_obj_t* page);
    void createPageSaveConfirm(lv_obj_t* page);
    void createPagePlayback(lv_obj_t* page);

    void switchPage(UiPage page);
    void updatePageContent(const RecorderState& state);
    void updateBottomLabels(UiPage page, bool isPaused);
    void handleActionForPage(UiPage page, int btnIndex);

    lv_obj_t* parent_ = nullptr;
    UiPage currentPage_ = UiPage::Home;

    // Status bar
    lv_obj_t* lblStatusText_ = nullptr;

    // Bottom bar
    std::array<lv_obj_t*, 5> lblBottomBtns_{};
    std::array<lv_obj_t*, 5> lblBottomIndicators_{};

    // Pages
    std::array<lv_obj_t*, 4> pages_{};

    // FileList
    lv_obj_t* lblFileListEmpty_ = nullptr;
    std::array<lv_obj_t*, 5> lblFileListItems_{};
    int fileListOffset_ = 0;

    // Home (shared with recording states)
    lv_obj_t* lblRecFilename_ = nullptr;
    lv_obj_t* recWaveContainer_ = nullptr;
    lv_obj_t* recWaveLine_ = nullptr;
    std::array<lv_point_precise_t, 128> recWavePoints_{};
    lv_obj_t* lblRecTimer_ = nullptr;

    // SaveConfirm
    lv_obj_t* taEdit_ = nullptr;
    std::string editOriginalName_;

    // Playback
    lv_obj_t* lblPlayFilename_ = nullptr;
    lv_obj_t* lblPlayDuration_ = nullptr;
    static constexpr int kPlayBarCount = 40;
    std::array<lv_obj_t*, kPlayBarCount> playWaveBars_{};
    lv_obj_t* playBaseLine_ = nullptr;
    lv_obj_t* playProgressArrow_ = nullptr;
    lv_obj_t* playProgressTime_ = nullptr;

    // Legacy (kept but hidden)
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
    uint32_t backspaceRepeatCount_ = 0;
    std::function<void(const std::string&)> actionHandler_;
};
