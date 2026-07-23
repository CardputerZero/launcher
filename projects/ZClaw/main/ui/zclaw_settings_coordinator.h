#pragma once

#include "zclaw_settings_controller.h"
#include "zclaw_settings_panel.h"
#include "zclaw_setup_progress_view.h"

#include <string>

namespace zclaw {

class FontManager;
class InputDialog;
class ProviderManager;
struct SettingsPresentation;
class UiConfigManager;

class SettingsCoordinator {
public:
    SettingsCoordinator(ProviderManager &providers, UiConfigManager &config,
                        InputDialog &input, FontManager &fonts);
    ~SettingsCoordinator();

    SettingsController &state();
    const SettingsController &state() const;

    bool is_open() const;
    void open(lv_obj_t *parent);
    void open_setup(lv_obj_t *parent, bool first_run_needed,
                    bool setup_in_flight);
    void close();

    void render_main();
    void render_authorization();
    void render_setup(bool setup_in_flight);
    void show_setup_error(const std::string &error);
    void close_setup_error();
    void update_setup_error_selection(int selected_index);
    void render_setup_providers();
    void render_providers();
    void render_provider_detail();
    void move_selection(int delta);
    void sync_setup_provider_selection();

    bool validate_setup(std::string *error);
    int setup_initialize_row() const;
    void update_setup_progress(const SetupProgress &progress,
                               bool setup_in_flight);

private:
    static void setup_error_deleted(lv_event_t *event);
    void create_panel(lv_obj_t *parent);
    void apply(const SettingsPresentation &presentation);

    ProviderManager &providers_;
    UiConfigManager &config_;
    InputDialog &input_;
    FontManager &fonts_;
    SettingsController state_;
    SettingsPanel panel_;
    SetupProgressView progress_;
    lv_obj_t *setup_error_dialog_ = nullptr;
    lv_obj_t *setup_error_buttons_[2] = {};
};

}  // namespace zclaw
