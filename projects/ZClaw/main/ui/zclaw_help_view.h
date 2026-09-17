/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl/lvgl.h"

#include <cstdint>

namespace zclaw {

class FontManager;

class HelpView {
public:
    HelpView() = default;
    ~HelpView();

    HelpView(const HelpView &) = delete;
    HelpView &operator=(const HelpView &) = delete;

    void create(lv_obj_t *parent, const FontManager *fonts);
    void show();
    void hide();
    bool visible() const;
    void scroll(int32_t direction);

private:
    static void backdrop_clicked(lv_event_t *event);
    static void backdrop_deleted(lv_event_t *event);
    void release();

    const FontManager *fonts_ = nullptr;
    lv_obj_t *backdrop_ = nullptr;
    lv_obj_t *content_ = nullptr;
};

}  // namespace zclaw
