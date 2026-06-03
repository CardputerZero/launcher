#include "ui_recorder.h"
#include "app_font.h"
#include "compat/input_keys.h"

LV_FONT_DECLARE(lv_font_montserrat_16)
LV_FONT_DECLARE(lv_font_montserrat_26)

// Layout constants
constexpr int kScreenW = 320;
constexpr int kScreenH = 170;
constexpr int kStatusH = 30;
constexpr int kBottomH = 25;
constexpr int kContentH = kScreenH - kStatusH - kBottomH; // 110
constexpr int kBtnW = kScreenW / 5; // 64

// Colors (dark theme)
const lv_color_t kColorBg = lv_color_hex(0x000000);
const lv_color_t kColorStatusBar = lv_color_hex(0x333333);
const lv_color_t kColorStatusText = lv_color_hex(0xFFFFFF);
const lv_color_t kColorText = lv_color_hex(0xFFFFFF);
const lv_color_t kColorTextGray = lv_color_hex(0xAAAAAA);
const lv_color_t kColorBottomBar = lv_color_hex(0x111111);
const lv_color_t kColorWaveBg = lv_color_hex(0x222222);
const lv_color_t kColorHighlight = lv_color_hex(0x00AAFF);
const lv_color_t kColorWaveOrange = lv_color_hex(0xFF8800);
const lv_color_t kColorIconStop       = lv_color_hex(0xFF0000);
const lv_color_t kColorIconRecord     = lv_color_hex(0xFF3399);
const lv_color_t kColorIconSampleRate = lv_color_hex(0xFFCC00);
const lv_color_t kColorIconList       = lv_color_hex(0x33CC33);
const lv_color_t kColorIconPlay       = lv_color_hex(0x1F9DFF);
const lv_color_t kColorIconPause      = lv_color_hex(0xCC6633);
const lv_color_t kColorIconFast       = lv_color_hex(0xCC3366);

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

    pages_[static_cast<size_t>(UiPage::Home)] = createPageContainer(parent);
    pages_[static_cast<size_t>(UiPage::FileList)] = createPageContainer(parent);
    pages_[static_cast<size_t>(UiPage::SaveConfirm)] = createPageContainer(parent);
    pages_[static_cast<size_t>(UiPage::Playback)] = createPageContainer(parent);

    createPageHome(pages_[static_cast<size_t>(UiPage::Home)]);
    createPageFileList(pages_[static_cast<size_t>(UiPage::FileList)]);
    createPageSaveConfirm(pages_[static_cast<size_t>(UiPage::SaveConfirm)]);
    createPagePlayback(pages_[static_cast<size_t>(UiPage::Playback)]);

    createStatusBar(parent);
    createBottomBar(parent);

    switchPage(UiPage::Home);
}

void UiRecorder::createStatusBar(lv_obj_t* parent)
{
    lblStatusText_ = lv_label_create(parent);
    lv_obj_set_pos(lblStatusText_, 8, 6);
    lv_obj_set_style_text_font(lblStatusText_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblStatusText_, kColorStatusText, 0);
    lv_label_set_text(lblStatusText_, "Recorder");
}

void UiRecorder::createBottomBar(lv_obj_t* parent)
{
    for (int i = 0; i < 5; i++) {
        lblBottomBtns_[i] = lv_label_create(parent);
        lv_obj_set_pos(lblBottomBtns_[i], i * kBtnW, kScreenH - kBottomH -4);
        lv_obj_set_size(lblBottomBtns_[i], kBtnW, kBottomH);
        lv_obj_set_style_text_font(lblBottomBtns_[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lblBottomBtns_[i], kColorText, 0);
        lv_obj_set_style_text_align(lblBottomBtns_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(lblBottomBtns_[i], "--");
        lv_obj_set_style_pad_top(lblBottomBtns_[i], 0, 0);
        lv_obj_add_flag(lblBottomBtns_[i], LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    }

    for (int i = 0; i < 5; i++) {
        lblBottomIndicators_[i] = lv_label_create(parent);
        lv_obj_set_pos(lblBottomIndicators_[i], i * kBtnW, kScreenH - 12);
        lv_obj_set_size(lblBottomIndicators_[i], kBtnW, 12);
        lv_obj_set_style_text_font(lblBottomIndicators_[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lblBottomIndicators_[i], kColorText, 0);
        lv_obj_set_style_text_align(lblBottomIndicators_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(lblBottomIndicators_[i], "|");
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

// ---------- Home Page (merged with Recording/RecPaused) ----------
void UiRecorder::createPageHome(lv_obj_t* page)
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
    lv_obj_add_flag(recWaveContainer_, LV_OBJ_FLAG_HIDDEN);

    recWaveLine_ = lv_line_create(page);
    lv_obj_set_size(recWaveLine_, 280, 50);
    lv_obj_set_pos(recWaveLine_, 20, 30);
    lv_obj_set_style_line_color(recWaveLine_, kColorWaveOrange, 0);
    lv_obj_set_style_line_width(recWaveLine_, 2, 0);
    lv_line_set_y_invert(recWaveLine_, true);
    lv_line_set_points_mutable(recWaveLine_, recWavePoints_.data(), recWavePoints_.size());

    lblRecTimer_ = lv_label_create(page);
    lv_obj_set_pos(lblRecTimer_, 0, 88);
    lv_obj_set_width(lblRecTimer_, kScreenW);
    lv_obj_set_style_text_font(lblRecTimer_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblRecTimer_, kColorText, 0);
    lv_obj_set_style_text_align(lblRecTimer_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblRecTimer_, "");
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

// ---------- Save Confirm Page ----------
void UiRecorder::createPageSaveConfirm(lv_obj_t* page)
{
    taEdit_ = lv_textarea_create(page);
    lv_obj_set_pos(taEdit_, 10, 35);
    lv_obj_set_size(taEdit_, 300, 30);
    lv_obj_set_style_text_font(taEdit_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(taEdit_, kColorText, 0);
    lv_obj_set_style_bg_opa(taEdit_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(taEdit_, 0, 0);
    lv_obj_set_style_pad_all(taEdit_, 0, 0);
    lv_textarea_set_one_line(taEdit_, true);
    lv_textarea_set_text(taEdit_, "");
    lv_textarea_set_text_selection(taEdit_, true);

    // Cursor styling
    lv_obj_set_style_border_width(taEdit_, 2, LV_PART_CURSOR);
    lv_obj_set_style_border_color(taEdit_, kColorHighlight, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(taEdit_, LV_OPA_50, LV_PART_CURSOR);
    lv_obj_set_style_bg_color(taEdit_, kColorHighlight, LV_PART_CURSOR);
}

// ---------- Playback Page ----------
void UiRecorder::createPagePlayback(lv_obj_t* page)
{
    // Filename: top-right, scrolling
    lblPlayFilename_ = lv_label_create(page);
    lv_obj_set_pos(lblPlayFilename_, 10, 5);
    lv_obj_set_width(lblPlayFilename_, 300);
    lv_obj_set_style_text_font(lblPlayFilename_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblPlayFilename_, kColorText, 0);
    lv_label_set_long_mode(lblPlayFilename_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(lblPlayFilename_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lblPlayFilename_, "");

    // Duration: below filename, right aligned
    lblPlayDuration_ = lv_label_create(page);
    lv_obj_set_pos(lblPlayDuration_, 10, 22);
    lv_obj_set_width(lblPlayDuration_, 300);
    lv_obj_set_style_text_font(lblPlayDuration_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblPlayDuration_, kColorTextGray, 0);
    lv_obj_set_style_text_align(lblPlayDuration_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(lblPlayDuration_, "");

    // Wave bars: 40 gray bars
    constexpr int barW = 5;
    constexpr int barGap = 2;
    constexpr int startX = 20;
    constexpr int baseY = 80;
    for (int i = 0; i < kPlayBarCount; i++) {
        playWaveBars_[i] = lv_obj_create(page);
        lv_obj_set_size(playWaveBars_[i], barW, 1);
        lv_obj_set_pos(playWaveBars_[i], startX + i * (barW + barGap), baseY - 1);
        lv_obj_set_style_bg_color(playWaveBars_[i], lv_color_hex(0x888888), 0);
        lv_obj_set_style_bg_opa(playWaveBars_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(playWaveBars_[i], 0, 0);
        lv_obj_set_style_radius(playWaveBars_[i], 0, 0);
        lv_obj_clear_flag(playWaveBars_[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    // Blue base line
    playBaseLine_ = lv_line_create(page);
    static lv_point_precise_t baseLinePts[2];
    baseLinePts[0].x = startX;
    baseLinePts[0].y = baseY;
    baseLinePts[1].x = startX + kPlayBarCount * (barW + barGap) - barGap;
    baseLinePts[1].y = baseY;
    lv_line_set_points(playBaseLine_, baseLinePts, 2);
    lv_obj_set_style_line_color(playBaseLine_, lv_color_hex(0x0088FF), 0);
    lv_obj_set_style_line_width(playBaseLine_, 2, 0);

    // Red arrow (below blue line)
    playProgressArrow_ = lv_label_create(page);
    lv_obj_set_size(playProgressArrow_, 20, 12);
    lv_obj_set_style_text_font(playProgressArrow_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(playProgressArrow_, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_text_align(playProgressArrow_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(playProgressArrow_, "▲");

    // Time below arrow
    playProgressTime_ = lv_label_create(page);
    lv_obj_set_size(playProgressTime_, 50, 14);
    lv_obj_set_style_text_font(playProgressTime_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(playProgressTime_, kColorText, 0);
    lv_obj_set_style_text_align(playProgressTime_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(playProgressTime_, "00:00");

    // Legacy container (hidden)
    playWaveContainer_ = lv_obj_create(page);
    lv_obj_set_size(playWaveContainer_, 280, 50);
    lv_obj_set_pos(playWaveContainer_, 20, 30);
    lv_obj_set_style_bg_opa(playWaveContainer_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(playWaveContainer_, 0, 0);
    lv_obj_clear_flag(playWaveContainer_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(playWaveContainer_, 0, 0);
    lv_obj_add_flag(playWaveContainer_, LV_OBJ_FLAG_HIDDEN);

    playWaveLine_ = lv_line_create(playWaveContainer_);
    lv_obj_set_size(playWaveLine_, 280, 50);
    lv_obj_set_pos(playWaveLine_, 0, 0);
    lv_obj_set_style_line_color(playWaveLine_, kColorHighlight, 0);
    lv_obj_set_style_line_width(playWaveLine_, 2, 0);
    lv_line_set_y_invert(playWaveLine_, true);
    lv_line_set_points_mutable(playWaveLine_, playWavePoints_.data(), playWavePoints_.size());

    playProgressLine_ = lv_line_create(playWaveContainer_);
    lv_obj_set_size(playProgressLine_, 280, 50);
    lv_obj_set_pos(playProgressLine_, 0, 0);
    lv_obj_set_style_line_color(playProgressLine_, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_line_width(playProgressLine_, 2, 0);
    lv_line_set_y_invert(playProgressLine_, true);
    lv_line_set_points_mutable(playProgressLine_, playProgressPoints_, 2);

    lblPlayTimer_ = lv_label_create(page);
    lv_obj_add_flag(lblPlayTimer_, LV_OBJ_FLAG_HIDDEN);
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
    auto setBtn = [this](int idx, const char* text, bool isIcon, lv_color_t color = kColorText, lv_opa_t opa = LV_OPA_COVER) {
        if (isIcon) {
            lv_obj_set_style_text_font(lblBottomBtns_[idx], ui::font::AppFont::svgfont(16), 0);
        } else {
            lv_obj_set_style_text_font(lblBottomBtns_[idx], &lv_font_montserrat_12, 0);
        }
        lv_obj_set_style_text_color(lblBottomBtns_[idx], color, 0);
        lv_obj_set_style_text_opa(lblBottomBtns_[idx], opa, 0);
        lv_label_set_text(lblBottomBtns_[idx], text);
    };

    switch (page) {
        case UiPage::Home:
            if (lastState_.state == AppState::Idle) {
                setBtn(0, ui::font::ICON_EXIT, true);
                setBtn(1, ui::font::ICON_STOP, true, kColorIconStop, LV_OPA_50);
                setBtn(2, ui::font::ICON_RECORD, true, kColorIconRecord);
                setBtn(3, lastState_.sampleRate.c_str(), false, kColorIconSampleRate);
                setBtn(4, ui::font::ICON_LIST, true, kColorIconList);
            } else if (lastState_.state == AppState::Recording) {
                setBtn(0, ui::font::ICON_EXIT, true);
                setBtn(1, ui::font::ICON_STOP, true, kColorIconStop);
                setBtn(2, ui::font::ICON_PAUSE, true, kColorIconPause);
                setBtn(3, lastState_.sampleRate.c_str(), false, kColorIconSampleRate, LV_OPA_50);
                setBtn(4, ui::font::ICON_LIST, true, kColorIconList, LV_OPA_50);
            } else if (lastState_.state == AppState::RecPaused) {
                setBtn(0, ui::font::ICON_EXIT, true);
                setBtn(1, ui::font::ICON_STOP, true, kColorIconStop);
                setBtn(2, ui::font::ICON_PLAY, true, kColorIconPlay);
                setBtn(3, lastState_.sampleRate.c_str(), false, kColorIconSampleRate, LV_OPA_50);
                setBtn(4, ui::font::ICON_LIST, true, kColorIconList, LV_OPA_50);
            }
            break;
        case UiPage::FileList:
            setBtn(0, "Up", false);
            setBtn(1, "Down", false);
            setBtn(2, ui::font::ICON_PLAY, true, kColorIconPlay);
            setBtn(3, "Rename", false);
            setBtn(4, ui::font::ICON_EXIT, true);
            break;
        case UiPage::SaveConfirm:
            setBtn(0, ui::font::ICON_FAST_REWIND, true, kColorIconFast);
            setBtn(1, ui::font::ICON_FAST_FORWARD, true, kColorIconFast);
            setBtn(2, "Undo", false);
            setBtn(3, "Save", false);
            setBtn(4, ui::font::ICON_EXIT, true);
            break;
        case UiPage::Playback:
            setBtn(0, ui::font::ICON_EXIT, true);
            setBtn(1, lastState_.playbackSpeed.c_str(), false);
            setBtn(2, isPaused ? ui::font::ICON_PLAY : ui::font::ICON_PAUSE, true, isPaused ? kColorIconPlay : kColorIconPause);
            setBtn(3, ui::font::ICON_FAST_REWIND, true, kColorIconFast);
            setBtn(4, ui::font::ICON_FAST_FORWARD, true, kColorIconFast);
            break;
    }
}

// ---------- Content update ----------
void UiRecorder::updatePageContent(const RecorderState& state)
{
    // Detect page transitions before updating lastState_
    bool enteringSaveConfirm = (lastState_.state != AppState::SaveConfirm && state.state == AppState::SaveConfirm);
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

    // Home page content (Idle / Recording / RecPaused)
    if (currentPage_ == UiPage::Home) {
        if (state.state == AppState::Idle) {
            lv_label_set_text(lblRecFilename_, "");
            lv_label_set_text(lblRecTimer_, "");
        } else {
            const char* fname = state.currentFileName.empty() ? "" : state.currentFileName.c_str();
            lv_label_set_text(lblRecFilename_, fname);
            lv_label_set_text(lblRecTimer_, state.timerText.c_str());
        }

        constexpr int count = 128;
        float stepX = 280.0f / (count - 1);
        for (int i = 0; i < count; i++) {
            recWavePoints_[i].x = i * stepX;
            if (state.state == AppState::Idle || state.state == AppState::RecPaused) {
                recWavePoints_[i].y = 25.0f; // flat line at center
            } else {
                float v = state.recWaveform[i];
                if (v < -1.0f) v = -1.0f;
                if (v > 1.0f) v = 1.0f;
                recWavePoints_[i].y = 25.0f + v * 25.0f;
            }
        }
        lv_obj_invalidate(recWaveLine_);
    }

    if (enteringSaveConfirm && taEdit_) {
        std::string nameOnly = state.currentFileName;
        size_t dot = nameOnly.find_last_of('.');
        if (dot != std::string::npos) nameOnly = nameOnly.substr(0, dot);
        lv_textarea_set_text(taEdit_, nameOnly.c_str());
        lv_textarea_set_cursor_pos(taEdit_, LV_TEXTAREA_CURSOR_LAST);
        lv_obj_t* label = lv_textarea_get_label(taEdit_);
        if (label) {
            lv_label_set_text_selection_start(label, 0);
            lv_label_set_text_selection_end(label, nameOnly.length());
        }
        editOriginalName_ = nameOnly;
    }

    // Playback
    const char* fname = state.currentFileName.empty() ? "" : state.currentFileName.c_str();
    lv_label_set_text(lblPlayFilename_, fname);
    {
        float dur = state.playbackDuration;
        int dmin = static_cast<int>(dur) / 60;
        int dsec = static_cast<int>(dur) % 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", dmin, dsec);
        lv_label_set_text(lblPlayDuration_, buf);
    }

    // Update playback waveform and progress
    if (currentPage_ == UiPage::Playback) {
        // Gray bars
        if (state.hasPlayWaveform) {
            constexpr int sampleCount = 256;
            constexpr int baseY = 80;
            constexpr int maxBarH = 40;
            for (int i = 0; i < kPlayBarCount; i++) {
                int startIdx = i * sampleCount / kPlayBarCount;
                int endIdx = (i + 1) * sampleCount / kPlayBarCount;
                float maxVal = 0.0f;
                for (int j = startIdx; j < endIdx && j < sampleCount; j++) {
                    float v = state.playWaveform[j];
                    if (v < 0.0f) v = 0.0f;
                    if (v > 1.0f) v = 1.0f;
                    if (v > maxVal) maxVal = v;
                }
                int h = static_cast<int>(maxVal * maxBarH);
                if (h < 1) h = 1;
                lv_obj_set_size(playWaveBars_[i], 5, h);
                lv_obj_set_pos(playWaveBars_[i], 20 + i * 7, baseY - h);
            }
        }

        // Arrow and time position
        float dur = state.playbackDuration;
        float pos = state.playbackPosition;
        int arrowX = 20;
        if (dur > 0.0f) {
            float ratio = pos / dur;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            constexpr int totalW = kPlayBarCount * 7 - 2; // 278
            arrowX = 20 + static_cast<int>(ratio * totalW);
        }
        lv_obj_set_pos(playProgressArrow_, arrowX - 10, 82);
        lv_obj_set_pos(playProgressTime_, arrowX - 25, 95);

        int pmin = static_cast<int>(pos) / 60;
        int psec = static_cast<int>(pos) % 60;
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", pmin, psec);
        lv_label_set_text(playProgressTime_, timeBuf);
    }

    // Update bottom labels in case sample rate or speed changed
    updateBottomLabels(currentPage_, isPlaybackPaused_);
}

void UiRecorder::update(const RecorderState& state)
{
    switch (state.state) {
        case AppState::Idle:        currentPage_ = UiPage::Home; break;
        case AppState::Recording:   currentPage_ = UiPage::Home; break;
        case AppState::RecPaused:   currentPage_ = UiPage::Home; break;
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
void UiRecorder::onKeyPressed(uint32_t key_code, bool isRepeat)
{
    uint32_t now = lv_tick_get();
    if (!isRepeat && key_code == lastKeyCode_ && now - lastKeyTime_ < 200) return;
    lastKeyCode_ = key_code;
    lastKeyTime_ = now;

    // Direct textarea editing in SaveConfirm
    if (currentPage_ == UiPage::SaveConfirm) {
        switch (key_code) {
            case KEY_F4:
                if (taEdit_) {
                    lv_textarea_cursor_left(taEdit_);
                    lv_textarea_clear_selection(taEdit_);
                }
                return;
            case KEY_F5:
                if (taEdit_) {
                    lv_textarea_cursor_right(taEdit_);
                    lv_textarea_clear_selection(taEdit_);
                }
                return;
            case KEY_F6:
                if (taEdit_) {
                    lv_textarea_set_text(taEdit_, editOriginalName_.c_str());
                    lv_obj_t* label = lv_textarea_get_label(taEdit_);
                    if (label) {
                        lv_label_set_text_selection_start(label, 0);
                        lv_label_set_text_selection_end(label, editOriginalName_.length());
                    }
                }
                return;
            case KEY_BACKSPACE:
            case KEY_DELETE:
                if (taEdit_) {
                    if (isRepeat) {
                        backspaceRepeatCount_++;
                        if (backspaceRepeatCount_ >= 15) {
                            lv_textarea_set_text(taEdit_, "");
                            backspaceRepeatCount_ = 0;
                        } else {
                            lv_textarea_delete_char(taEdit_);
                        }
                    } else {
                        backspaceRepeatCount_ = 1;
                        lv_textarea_delete_char(taEdit_);
                    }
                }
                return;
            default:
                break;
        }
    }

    int btnIndex = -1;
    switch (key_code) {
        case KEY_F4: btnIndex = 0; break;
        case KEY_F5: btnIndex = 1; break;
        case KEY_F6: btnIndex = 2; break;
        case KEY_F7: btnIndex = 3; break;
        case KEY_F8: btnIndex = 4; break;
        case KEY_ENTER:
        case KEY_KPENTER:
            if (currentPage_ == UiPage::Home) btnIndex = 2;
            else if (currentPage_ == UiPage::FileList) btnIndex = 2;
            else if (currentPage_ == UiPage::Playback) btnIndex = 2;
            else if (currentPage_ == UiPage::SaveConfirm) btnIndex = 3;
            break;
        case KEY_ESC:
            if (currentPage_ == UiPage::Home && lastState_.state == AppState::Idle) {
                if (actionHandler_) actionHandler_("quit");
                return;
            } else if (currentPage_ == UiPage::Home) {
                btnIndex = 0; // Exit
            } else if (currentPage_ == UiPage::Playback) {
                btnIndex = 0; // Exit
            } else {
                btnIndex = 4; // Exit for FileList / SaveConfirm
            }
            break;
        case KEY_LEFT:
            if (currentPage_ == UiPage::FileList) { btnIndex = 0; break; }
            return;
        case KEY_RIGHT:
            if (currentPage_ == UiPage::FileList) { btnIndex = 1; break; }
            return;
        case KEY_P:
            if (currentPage_ == UiPage::Home && (lastState_.state == AppState::Recording || lastState_.state == AppState::RecPaused)) btnIndex = 2;
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

void UiRecorder::onCharTyped(uint32_t codepoint)
{
    if (currentPage_ != UiPage::SaveConfirm || !taEdit_) return;
    if (codepoint >= 32 && codepoint < 127) {
        lv_textarea_add_char(taEdit_, codepoint);
    }
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
            if (lastState_.state == AppState::Idle) {
                map = {"quit", "", "toggle_record", "cycle_rate", "list"};
            } else {
                map = {"exit", "done", "toggle_pause", "cycle_rate", "list"};
            }
            break;
        case UiPage::FileList:
            map = {"prev_file", "next_file", "toggle_play", "rename", "exit"};
            break;
        case UiPage::SaveConfirm:
            map = {"left", "right", "undo", "save", "exit"};
            break;
        case UiPage::Playback:
            map = {"exit", "speed", "toggle_pause", "seek_back", "seek_fwd"};
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
        if (page == UiPage::SaveConfirm && btn == 3) {
            // Save: get edited name from textarea
            if (taEdit_) {
                const char* text = lv_textarea_get_text(taEdit_);
                std::string name = text ? text : "";
                actionHandler_(std::string("save:") + name);
            }
        } else {
            actionHandler_(action);
        }
    }
}
