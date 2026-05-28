#include "ui_recorder.h"
#include "compat/input_keys.h"

LV_FONT_DECLARE(lv_font_montserrat_16)
LV_FONT_DECLARE(lv_font_montserrat_26)

// Layout constants
constexpr int kScreenW = 320;
constexpr int kScreenH = 170;
constexpr int kStatusH = 30;
constexpr int kBottomH = 30;
constexpr int kContentH = kScreenH - kStatusH - kBottomH; // 110
constexpr int kBtnW = kScreenW / 5; // 64

// Colors (light theme matching reference image)
const lv_color_t kColorBg = lv_color_hex(0xE8E8EC);
const lv_color_t kColorStatusBar = lv_color_hex(0x555555);
const lv_color_t kColorStatusText = lv_color_hex(0xFFFFFF);
const lv_color_t kColorText = lv_color_hex(0x222222);
const lv_color_t kColorTextGray = lv_color_hex(0x888888);
const lv_color_t kColorBottomBar = lv_color_hex(0xD8D8E0);
const lv_color_t kColorWaveBg = lv_color_hex(0xC8C8D0);
const lv_color_t kColorHighlight = lv_color_hex(0x0066CC);

void UiRecorder::init(lv_obj_t* parent)
{
    parent_ = parent;
    buildUi(parent);
}

void UiRecorder::buildUi(lv_obj_t* parent)
{
    lv_obj_set_size(parent, kScreenW, kScreenH);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, kColorBg, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    createStatusBar(parent);
    createBottomBar(parent);

    pages_[static_cast<size_t>(UiPage::Home)] = createPageContainer(parent);
    pages_[static_cast<size_t>(UiPage::FileList)] = createPageContainer(parent);
    pages_[static_cast<size_t>(UiPage::Recording)] = createPageContainer(parent);
    pages_[static_cast<size_t>(UiPage::RecPaused)] = createPageContainer(parent);
    pages_[static_cast<size_t>(UiPage::SaveConfirm)] = createPageContainer(parent);
    pages_[static_cast<size_t>(UiPage::Playback)] = createPageContainer(parent);

    createPageHome(pages_[static_cast<size_t>(UiPage::Home)]);
    createPageFileList(pages_[static_cast<size_t>(UiPage::FileList)]);
    createPageRecording(pages_[static_cast<size_t>(UiPage::Recording)]);
    createPageRecPaused(pages_[static_cast<size_t>(UiPage::RecPaused)]);
    createPageSaveConfirm(pages_[static_cast<size_t>(UiPage::SaveConfirm)]);
    createPagePlayback(pages_[static_cast<size_t>(UiPage::Playback)]);

    switchPage(UiPage::Home);
}

void UiRecorder::createStatusBar(lv_obj_t* parent)
{
    statusBar_ = lv_obj_create(parent);
    lv_obj_set_size(statusBar_, kScreenW, kStatusH);
    lv_obj_set_pos(statusBar_, 0, 0);
    lv_obj_clear_flag(statusBar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(statusBar_, kColorStatusBar, 0);
    lv_obj_set_style_bg_opa(statusBar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(statusBar_, 0, 0);
    lv_obj_set_style_pad_all(statusBar_, 0, 0);
    lv_obj_set_style_radius(statusBar_, 0, 0);

    lblStatusText_ = lv_label_create(statusBar_);
    lv_obj_set_pos(lblStatusText_, 8, 6);
    lv_obj_set_style_text_font(lblStatusText_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblStatusText_, kColorStatusText, 0);
    lv_label_set_text(lblStatusText_, "Recorder");
}

void UiRecorder::createBottomBar(lv_obj_t* parent)
{
    bottomBar_ = lv_obj_create(parent);
    lv_obj_set_size(bottomBar_, kScreenW, kBottomH);
    lv_obj_set_pos(bottomBar_, 0, kScreenH - kBottomH);
    lv_obj_clear_flag(bottomBar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bottomBar_, kColorBottomBar, 0);
    lv_obj_set_style_bg_opa(bottomBar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bottomBar_, 0, 0);
    lv_obj_set_style_pad_all(bottomBar_, 0, 0);
    lv_obj_set_style_radius(bottomBar_, 0, 0);

    for (int i = 0; i < 5; i++) {
        lblBottomBtns_[i] = lv_label_create(bottomBar_);
        lv_obj_set_pos(lblBottomBtns_[i], i * kBtnW, 0);
        lv_obj_set_size(lblBottomBtns_[i], kBtnW, kBottomH);
        lv_obj_set_style_text_font(lblBottomBtns_[i], &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lblBottomBtns_[i], kColorText, 0);
        lv_obj_set_style_text_align(lblBottomBtns_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(lblBottomBtns_[i], "--");
        lv_obj_set_style_pad_top(lblBottomBtns_[i], 8, 0);
    }
}

lv_obj_t* UiRecorder::createPageContainer(lv_obj_t* parent)
{
    lv_obj_t* page = lv_obj_create(parent);
    lv_obj_set_size(page, kScreenW, kContentH);
    lv_obj_set_pos(page, 0, kStatusH);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(page, kColorBg, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    return page;
}

// ---------- Home Page ----------
void UiRecorder::createPageHome(lv_obj_t* page)
{
    lblHomeReady_ = lv_label_create(page);
    lv_obj_set_style_text_font(lblHomeReady_, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(lblHomeReady_, kColorText, 0);
    lv_label_set_text(lblHomeReady_, "Ready");
    lv_obj_center(lblHomeReady_);
}

// ---------- File List Page ----------
void UiRecorder::createPageFileList(lv_obj_t* page)
{
    for (int i = 0; i < 5; i++) {
        lblFileListItems_[i] = lv_label_create(page);
        lv_obj_set_pos(lblFileListItems_[i], 10, 5 + i * 20);
        lv_obj_set_width(lblFileListItems_[i], 300);
        lv_obj_set_style_text_font(lblFileListItems_[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lblFileListItems_[i], kColorText, 0);
        lv_label_set_text(lblFileListItems_[i], "");
        lv_obj_add_flag(lblFileListItems_[i], LV_OBJ_FLAG_HIDDEN);
    }

    lblFileListEmpty_ = lv_label_create(page);
    lv_obj_set_style_text_font(lblFileListEmpty_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblFileListEmpty_, kColorTextGray, 0);
    lv_label_set_text(lblFileListEmpty_, "Empty");
    lv_obj_center(lblFileListEmpty_);
}

// ---------- Recording Page ----------
void UiRecorder::createPageRecording(lv_obj_t* page)
{
    lblRecFilename_ = lv_label_create(page);
    lv_obj_set_pos(lblRecFilename_, 10, 5);
    lv_obj_set_width(lblRecFilename_, 300);
    lv_obj_set_style_text_font(lblRecFilename_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblRecFilename_, kColorText, 0);
    lv_label_set_long_mode(lblRecFilename_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(lblRecFilename_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblRecFilename_, "");

    recWaveContainer_ = lv_obj_create(page);
    lv_obj_set_size(recWaveContainer_, 280, 50);
    lv_obj_set_pos(recWaveContainer_, 20, 30);
    lv_obj_set_style_bg_color(recWaveContainer_, kColorWaveBg, 0);
    lv_obj_set_style_bg_opa(recWaveContainer_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(recWaveContainer_, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_border_width(recWaveContainer_, 1, 0);
    lv_obj_clear_flag(recWaveContainer_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(recWaveContainer_, 4, 0);

    lblRecTimer_ = lv_label_create(page);
    lv_obj_set_pos(lblRecTimer_, 0, 88);
    lv_obj_set_width(lblRecTimer_, kScreenW);
    lv_obj_set_style_text_font(lblRecTimer_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblRecTimer_, kColorText, 0);
    lv_obj_set_style_text_align(lblRecTimer_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblRecTimer_, "00:00");
}

// ---------- Rec Paused Page ----------
void UiRecorder::createPageRecPaused(lv_obj_t* page)
{
    lblPauseFilename_ = lv_label_create(page);
    lv_obj_set_pos(lblPauseFilename_, 10, 5);
    lv_obj_set_width(lblPauseFilename_, 300);
    lv_obj_set_style_text_font(lblPauseFilename_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblPauseFilename_, kColorText, 0);
    lv_label_set_long_mode(lblPauseFilename_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(lblPauseFilename_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblPauseFilename_, "");

    // Horizontal pause line in the center
    pauseLine_ = lv_line_create(page);
    static lv_point_precise_t line_points[2];
    line_points[0].x = 20;
    line_points[0].y = 55;
    line_points[1].x = 300;
    line_points[1].y = 55;
    lv_line_set_points(pauseLine_, line_points, 2);
    lv_obj_set_style_line_width(pauseLine_, 2, 0);
    lv_obj_set_style_line_color(pauseLine_, kColorText, 0);

    lblPauseTimer_ = lv_label_create(page);
    lv_obj_set_pos(lblPauseTimer_, 0, 88);
    lv_obj_set_width(lblPauseTimer_, kScreenW);
    lv_obj_set_style_text_font(lblPauseTimer_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblPauseTimer_, kColorText, 0);
    lv_obj_set_style_text_align(lblPauseTimer_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblPauseTimer_, "00:00");
}

// ---------- Save Confirm Page ----------
void UiRecorder::createPageSaveConfirm(lv_obj_t* page)
{
    lblSaveFilename_ = lv_label_create(page);
    lv_obj_set_pos(lblSaveFilename_, 10, 40);
    lv_obj_set_width(lblSaveFilename_, 300);
    lv_obj_set_style_text_font(lblSaveFilename_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblSaveFilename_, kColorText, 0);
    lv_obj_set_style_text_align(lblSaveFilename_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lblSaveFilename_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(lblSaveFilename_, "");
}

// ---------- Playback Page ----------
void UiRecorder::createPagePlayback(lv_obj_t* page)
{
    lblPlayFilename_ = lv_label_create(page);
    lv_obj_set_pos(lblPlayFilename_, 10, 5);
    lv_obj_set_width(lblPlayFilename_, 300);
    lv_obj_set_style_text_font(lblPlayFilename_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblPlayFilename_, kColorText, 0);
    lv_label_set_long_mode(lblPlayFilename_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(lblPlayFilename_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblPlayFilename_, "");

    playWaveContainer_ = lv_obj_create(page);
    lv_obj_set_size(playWaveContainer_, 280, 50);
    lv_obj_set_pos(playWaveContainer_, 20, 30);
    lv_obj_set_style_bg_color(playWaveContainer_, kColorWaveBg, 0);
    lv_obj_set_style_bg_opa(playWaveContainer_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(playWaveContainer_, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_border_width(playWaveContainer_, 1, 0);
    lv_obj_clear_flag(playWaveContainer_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(playWaveContainer_, 4, 0);

    lblPlayTimer_ = lv_label_create(page);
    lv_obj_set_pos(lblPlayTimer_, 0, 88);
    lv_obj_set_width(lblPlayTimer_, kScreenW);
    lv_obj_set_style_text_font(lblPlayTimer_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblPlayTimer_, kColorText, 0);
    lv_obj_set_style_text_align(lblPlayTimer_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblPlayTimer_, "00:00 / 00:00");
}

// ---------- Page switching ----------
void UiRecorder::switchPage(UiPage page)
{
    for (size_t i = 0; i < pages_.size(); i++) {
        if (i == static_cast<size_t>(page)) {
            lv_obj_clear_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(pages_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    currentPage_ = page;
    updateBottomLabels(page, isPlaybackPaused_);
}

void UiRecorder::updateBottomLabels(UiPage page, bool isPaused)
{
    struct LabelSet {
        const char* b0;
        const char* b1;
        const char* b2;
        const char* b3;
        const char* b4;
    };

    LabelSet labels = {"--", "--", "--", "--", "--"};
    switch (page) {
        case UiPage::Home:
            labels = {"List", "48k", "Rec", "--", "Exit"};
            break;
        case UiPage::FileList:
            labels = {"Up", "Down", "Play", "Rename", "Exit"};
            break;
        case UiPage::Recording:
            labels = {"--", "--", "Pause", "Done", "Exit"};
            break;
        case UiPage::RecPaused:
            labels = {"--", "--", "Continue", "Done", "Exit"};
            break;
        case UiPage::SaveConfirm:
            labels = {"--", "--", "--", "Save", "Exit"};
            break;
        case UiPage::Playback:
            labels = {lastState_.playbackSpeed.c_str(), "-10s", isPaused ? "Play" : "Pause", "+10s", "Exit"};
            break;
    }

    lv_label_set_text(lblBottomBtns_[0], labels.b0);
    lv_label_set_text(lblBottomBtns_[1], labels.b1);
    lv_label_set_text(lblBottomBtns_[2], labels.b2);
    lv_label_set_text(lblBottomBtns_[3], labels.b3);
    lv_label_set_text(lblBottomBtns_[4], labels.b4);
}

// ---------- Content update ----------
void UiRecorder::updatePageContent(const RecorderState& state)
{
    lastState_ = state;

    // Status bar
    lv_label_set_text(lblStatusText_, state.statusText.c_str());

    // FileList
    if (currentPage_ == UiPage::FileList) {
        bool hasFiles = !state.fileList.empty();
        if (hasFiles) {
            lv_obj_add_flag(lblFileListEmpty_, LV_OBJ_FLAG_HIDDEN);

            int sel = state.selectedFileIndex;
            if (sel < 0) sel = 0;
            if (sel >= static_cast<int>(state.fileList.size())) sel = static_cast<int>(state.fileList.size()) - 1;

            if (sel < fileListOffset_) fileListOffset_ = sel;
            if (sel >= fileListOffset_ + 5) fileListOffset_ = sel - 4;
            if (fileListOffset_ < 0) fileListOffset_ = 0;
            int maxOffset = static_cast<int>(state.fileList.size()) - 1;
            if (fileListOffset_ > maxOffset) fileListOffset_ = maxOffset;

            for (int i = 0; i < 5; i++) {
                int idx = fileListOffset_ + i;
                if (idx < static_cast<int>(state.fileList.size())) {
                    lv_label_set_text(lblFileListItems_[i], state.fileList[idx].filename.c_str());
                    lv_obj_clear_flag(lblFileListItems_[i], LV_OBJ_FLAG_HIDDEN);
                    if (idx == sel) {
                        lv_obj_set_style_text_color(lblFileListItems_[i], kColorHighlight, 0);
                    } else {
                        lv_obj_set_style_text_color(lblFileListItems_[i], kColorText, 0);
                    }
                } else {
                    lv_label_set_text(lblFileListItems_[i], "");
                    lv_obj_add_flag(lblFileListItems_[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        } else {
            fileListOffset_ = 0;
            lv_obj_clear_flag(lblFileListEmpty_, LV_OBJ_FLAG_HIDDEN);
            for (int i = 0; i < 5; i++) {
                lv_obj_add_flag(lblFileListItems_[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    // Recording / RecPaused / SaveConfirm filename & timer
    const char* fname = state.currentFileName.empty() ? "" : state.currentFileName.c_str();
    lv_label_set_text(lblRecFilename_, fname);
    lv_label_set_text(lblPauseFilename_, fname);
    lv_label_set_text(lblSaveFilename_, fname);
    lv_label_set_text(lblRecTimer_, state.timerText.c_str());
    lv_label_set_text(lblPauseTimer_, state.timerText.c_str());

    // Playback
    lv_label_set_text(lblPlayFilename_, fname);
    lv_label_set_text(lblPlayTimer_, state.timerText.c_str());

    // Update bottom labels in case sample rate or speed changed
    updateBottomLabels(currentPage_, isPlaybackPaused_);
}

void UiRecorder::update(const RecorderState& state)
{
    switch (state.state) {
        case AppState::Idle:        currentPage_ = UiPage::Home; break;
        case AppState::Recording:   currentPage_ = UiPage::Recording; break;
        case AppState::RecPaused:   currentPage_ = UiPage::RecPaused; break;
        case AppState::SaveConfirm: currentPage_ = UiPage::SaveConfirm; break;
        case AppState::FileList:    currentPage_ = UiPage::FileList; break;
        case AppState::Playing:     currentPage_ = UiPage::Playback; isPlaybackPaused_ = false; break;
        case AppState::PlayPaused:  currentPage_ = UiPage::Playback; isPlaybackPaused_ = true; break;
    }

    updatePageContent(state);
    switchPage(currentPage_);
}

void UiRecorder::setActionHandler(std::function<void(const std::string&)> handler)
{
    actionHandler_ = handler;
}

// ---------- Key handling ----------
void UiRecorder::onKeyPressed(uint32_t key_code)
{
    uint32_t now = lv_tick_get();
    if (key_code == lastKeyCode_ && now - lastKeyTime_ < 200) return;
    lastKeyCode_ = key_code;
    lastKeyTime_ = now;

    int btnIndex = -1;
    switch (key_code) {
        case KEY_F4: btnIndex = 0; break;
        case KEY_F5: btnIndex = 1; break;
        case KEY_F6: btnIndex = 2; break;
        case KEY_F7: btnIndex = 3; break;
        case KEY_F8: btnIndex = 4; break;
        case KEY_ENTER:
        case KEY_KPENTER:
            if (currentPage_ == UiPage::Home) btnIndex = 2;          // Rec
            else if (currentPage_ == UiPage::Recording) btnIndex = 3; // Done
            else if (currentPage_ == UiPage::RecPaused) btnIndex = 3; // Done
            else if (currentPage_ == UiPage::FileList) btnIndex = 2; // Play
            else if (currentPage_ == UiPage::Playback) btnIndex = 2; // Pause/Play
            else if (currentPage_ == UiPage::SaveConfirm) btnIndex = 3; // Save
            break;
        case KEY_ESC:
            if (currentPage_ == UiPage::Home) {
                if (actionHandler_) actionHandler_("quit");
                return;
            }
            btnIndex = 4; // Exit
            break;
        case KEY_LEFT:
            if (currentPage_ == UiPage::FileList) { btnIndex = 0; break; }
            return;
        case KEY_RIGHT:
            if (currentPage_ == UiPage::FileList) { btnIndex = 1; break; }
            return;
        case KEY_P:
            if (currentPage_ == UiPage::Recording || currentPage_ == UiPage::RecPaused) btnIndex = 2;
            else if (currentPage_ == UiPage::Playback) btnIndex = 2;
            break;
        case KEY_SPACE:
            if (currentPage_ == UiPage::Playback) btnIndex = 2;
            break;
        case KEY_DELETE:
            if (currentPage_ == UiPage::FileList && actionHandler_) {
                actionHandler_("delete");
            }
            return;
        default:
            return;
    }

    if (btnIndex < 0 || btnIndex > 4) return;
    handleActionForPage(currentPage_, btnIndex);
}

void UiRecorder::handleActionForPage(UiPage page, int btn)
{
    if (!actionHandler_) return;

    struct ActionMap {
        const char* a0;
        const char* a1;
        const char* a2;
        const char* a3;
        const char* a4;
    };

    ActionMap map = {"", "", "", "", ""};
    switch (page) {
        case UiPage::Home:
            map = {"list", "cycle_rate", "toggle_record", "", "quit"};
            break;
        case UiPage::FileList:
            map = {"prev_file", "next_file", "toggle_play", "rename", "exit"};
            break;
        case UiPage::Recording:
            map = {"", "", "toggle_pause", "done", "exit"};
            break;
        case UiPage::RecPaused:
            map = {"", "", "toggle_pause", "done", "exit"};
            break;
        case UiPage::SaveConfirm:
            map = {"", "", "", "save", "exit"};
            break;
        case UiPage::Playback:
            map = {"speed", "seek_back", "toggle_pause", "seek_fwd", "exit"};
            break;
    }

    const char* action = nullptr;
    switch (btn) {
        case 0: action = map.a0; break;
        case 1: action = map.a1; break;
        case 2: action = map.a2; break;
        case 3: action = map.a3; break;
        case 4: action = map.a4; break;
    }

    if (action && action[0] != '\0') {
        actionHandler_(action);
    }
}
