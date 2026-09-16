/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "settings_about_help_model.hpp"
#include "settings_tree_types.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lvgl_components.hpp"

class LvSettingStaticInfoPage3 : public DComponens::LvglComponensBase {
public:
    enum class LayoutMetric : int {
        ScreenW = 320,
        ScreenH = 150,
        ContentX = 8,
        ContentW = 304,
        TitleY = 6,
        LinesY = 30,
        LinesBottomY = 132,
        LineGap = 3,
        FooterY = 133,
    };

    static constexpr int metric(LayoutMetric value)
    {
        return static_cast<int>(value);
    }

    LvSettingStaticInfoPage3();

    LvSettingStaticInfoPage3(lv_obj_t *parent,
                             const NodeIter &page_node,
                             std::function<void()> back_callback,
                             settings_t12b::about_help::Content content);

    ~LvSettingStaticInfoPage3() override;

    void AnimateNextIn(std::function<void()> callback) override;
    void AnimateNextOut(std::function<void()> callback) override;
    void LoadNextPage() override;
    void LeaveNextPage() override;
    void create_ui(lv_obj_t *parent) override;

    // Optional: re-generate the body lines while the page is on screen.  The
    // callback fills `lines` and returns true to repaint; a false return or a
    // different line count leaves the labels untouched.  Date & Time uses this
    // so its clock keeps ticking instead of freezing at construction time.
    void set_lines_provider(std::function<bool(std::vector<std::string> &)> provider);

private:
    static void refresh_lines_cb(lv_timer_t *timer);

    lv_obj_t *add_label(const std::string &text,
                        int x,
                        int y,
                        uint32_t color,
                        const lv_font_t *font,
                        bool wrap);

    void handle_key_event(lv_event_t *event);

    NodeIter page_node_;
    settings_t12b::about_help::Content content_;
    std::function<bool(std::vector<std::string> &)> lines_provider_;
    std::vector<lv_obj_t *> line_labels_;
    lv_timer_t *lines_timer_ = nullptr;
};

std::unique_ptr<DComponens::LvglComponensBase> settings_t12b_about_page_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> on_back);

std::unique_ptr<DComponens::LvglComponensBase> settings_storage_page_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> on_back);

std::unique_ptr<DComponens::LvglComponensBase> settings_credit_page_factory(
    lv_obj_t *parent,
    const NodeIter &page_node,
    std::function<void()> on_back);
