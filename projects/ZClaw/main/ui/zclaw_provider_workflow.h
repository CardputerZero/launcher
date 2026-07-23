#pragma once

#include <string>

namespace zclaw {

class ChatView;
class FontManager;
class InputDialog;
class ProviderManager;
class SettingsCoordinator;

class ProviderWorkflow {
public:
    ProviderWorkflow(ProviderManager &providers, SettingsCoordinator &settings,
                     InputDialog &input, FontManager &fonts, ChatView &chat);

    void add();
    void open_detail(int provider_index);
    void edit_selected_field();
    void apply_edit(const std::string &value);
    void delete_current();

private:
    void report_save_error(const std::string &error);

    ProviderManager &providers_;
    SettingsCoordinator &settings_;
    InputDialog &input_;
    FontManager &fonts_;
    ChatView &chat_;
};

}  // namespace zclaw
