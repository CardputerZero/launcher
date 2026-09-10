/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace zclaw {

class ChatWorkflow;
class InputDialog;
class SettingsWorkflow;

class InputWorkflow {
public:
    InputWorkflow(InputDialog &input, SettingsWorkflow &settings,
                  ChatWorkflow &chat);

    void submit_current();

private:
    InputDialog &input_;
    SettingsWorkflow &settings_;
    ChatWorkflow &chat_;
};

}  // namespace zclaw
