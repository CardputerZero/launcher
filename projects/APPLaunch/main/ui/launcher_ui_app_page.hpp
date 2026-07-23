/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "ui.h"
#include "keyboard_input.h"
#include "input_keys.h"
#include "ui_app_page.hpp"

// The launcher home screen now uses the same page shell as standalone apps.
// This compatibility type remains because UILaunchPage also owns carousel logic.
class home_base : public AppPage
{
public:
    home_base()
    {
        set_page_title("ZERO");
    }

    lv_obj_t *content_container() const { return ui_APP_Container; }
    lv_obj_t *top_container() const { return root_screen_; }
    void use_bold_home_title_font() {}
};
