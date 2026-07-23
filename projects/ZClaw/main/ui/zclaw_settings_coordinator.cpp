#include "zclaw_settings_coordinator.h"

#include "zclaw_fonts.hpp"
#include "zclaw_input_dialog.h"
#include "zclaw_provider_catalog.h"
#include "zclaw_provider_manager.h"
#include "zclaw_settings_presentation.h"
#include "zclaw_theme.h"
#include "zclaw_ui_config_manager.h"
#include "zclaw_widgets.h"

namespace zclaw {

SettingsCoordinator::SettingsCoordinator(ProviderManager &providers,
                                         UiConfigManager &config,
                                         InputDialog &input, FontManager &fonts)
    : providers_(providers), config_(config), input_(input), fonts_(fonts)
{
}

SettingsCoordinator::~SettingsCoordinator()
{
    close_setup_error();
}

SettingsController &SettingsCoordinator::state()
{
    return state_;
}

const SettingsController &SettingsCoordinator::state() const
{
    return state_;
}

bool SettingsCoordinator::is_open() const
{
    return panel_.is_open();
}

void SettingsCoordinator::open(lv_obj_t *parent)
{
    if (panel_.is_open() || panel_.is_animating())
        return;
    input_.close();
    create_panel(parent);
    panel_.show();
}

void SettingsCoordinator::open_setup(lv_obj_t *parent, bool first_run_needed,
                                     bool setup_in_flight)
{
    if (panel_.is_open() || panel_.is_animating())
        return;
    input_.close();
    sync_setup_provider_selection();
    create_panel(parent);
    if (first_run_needed)
        render_setup_providers();
    else
        render_setup(setup_in_flight);
    panel_.show();
}

void SettingsCoordinator::close()
{
    close_setup_error();
    if (panel_.is_open() && !panel_.is_animating())
        panel_.close();
}

void SettingsCoordinator::render_main()
{
    state_.set_view(SettingsView::Main);
    apply(present_settings_main(config_.config()));
    state_.clamp_rows(panel_.row_count());
    panel_.update_selection(state_.selected_row());
}

void SettingsCoordinator::render_authorization()
{
    state_.reset_view(SettingsView::Authorization);
    apply(present_authorization(config_.config()));
    panel_.update_selection(state_.selected_row());
}

void SettingsCoordinator::render_setup(bool setup_in_flight)
{
    close_setup_error();
    state_.set_view(SettingsView::Setup);
    progress_.clear();
    if (setup_in_flight) {
        apply(present_setup_progress());
        progress_.show(panel_.content(), &fonts_);
        state_.set_selected_row(0);
        return;
    }
    apply(present_setup(providers_.setup_provider()));
    state_.clamp_rows(panel_.row_count());
    panel_.update_selection(state_.selected_row());
}

void SettingsCoordinator::show_setup_error(const std::string &error)
{
    close_setup_error();
    setup_error_dialog_ =
        widgets::box(lv_layer_top(), 35, 35, 250, 100, theme::kBar, 8);
    lv_obj_add_event_cb(setup_error_dialog_, setup_error_deleted,
                        LV_EVENT_DELETE, this);
    lv_obj_set_style_border_width(setup_error_dialog_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(setup_error_dialog_,
                                  lv_color_hex(theme::kPurple), LV_PART_MAIN);
    lv_obj_move_foreground(setup_error_dialog_);
    widgets::label(setup_error_dialog_, "Download failed", 12, 9, 226, 14,
                   fonts_.font_12(), theme::kText);
    widgets::label(setup_error_dialog_,
                   error.empty() ? "Network connection failed." : error,
                   12, 30, 226, 36, fonts_.font_10(), theme::kMuted);
    static constexpr const char *labels[] = {"Retry", "Exit"};
    for (int index = 0; index < 2; ++index) {
        setup_error_buttons_[index] = widgets::box(
            setup_error_dialog_, 22 + index * 106, 74, 96, 18,
            theme::kPanel, 5);
        lv_obj_set_style_border_width(setup_error_buttons_[index], 1,
                                      LV_PART_MAIN);
        widgets::label(setup_error_buttons_[index], labels[index], 0, 4,
                       96, 10, fonts_.font_10(), theme::kText,
                       LV_TEXT_ALIGN_CENTER);
    }
    update_setup_error_selection(0);
}

void SettingsCoordinator::close_setup_error()
{
    lv_obj_t *dialog = setup_error_dialog_;
    setup_error_dialog_ = nullptr;
    if (dialog)
        lv_obj_del(dialog);
    setup_error_buttons_[0] = nullptr;
    setup_error_buttons_[1] = nullptr;
}

void SettingsCoordinator::update_setup_error_selection(int selected_index)
{
    for (int index = 0; index < 2; ++index) {
        if (!setup_error_buttons_[index])
            continue;
        const bool selected = index == selected_index;
        lv_obj_set_style_bg_color(setup_error_buttons_[index],
                                  lv_color_hex(selected ? 0x252542 : theme::kPanel),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_color(setup_error_buttons_[index],
                                      lv_color_hex(selected ? theme::kText
                                                            : theme::kPanelLine),
                                      LV_PART_MAIN);
    }
}

void SettingsCoordinator::setup_error_deleted(lv_event_t *event)
{
    SettingsCoordinator *coordinator = static_cast<SettingsCoordinator *>(
        lv_event_get_user_data(event));
    if (coordinator) {
        coordinator->setup_error_dialog_ = nullptr;
        coordinator->setup_error_buttons_[0] = nullptr;
        coordinator->setup_error_buttons_[1] = nullptr;
    }
}

void SettingsCoordinator::render_setup_providers()
{
    state_.set_view(SettingsView::SetupProviders);
    const int preset_count = static_cast<int>(provider_preset_count());
    state_.normalize_setup_providers(preset_count, SettingsPanel::kMaximumRows);
    apply(present_setup_providers(state_.setup_provider_selection(),
                                  SettingsPanel::kMaximumRows));
    panel_.update_selection(state_.selected_row());
}

void SettingsCoordinator::render_providers()
{
    state_.set_view(SettingsView::Providers);
    const int total_rows = static_cast<int>(providers_.providers().size()) + 1;
    state_.normalize_providers(total_rows, SettingsPanel::kMaximumRows);
    apply(present_providers(providers_.providers(), state_.provider_selection(),
                            SettingsPanel::kMaximumRows));
    panel_.update_selection(state_.selected_row());
}

void SettingsCoordinator::render_provider_detail()
{
    state_.set_view(SettingsView::ProviderDetail);
    const int provider_index = state_.provider_detail_index();
    if (provider_index < 0 ||
        provider_index >= static_cast<int>(providers_.providers().size())) {
        render_providers();
        return;
    }
    apply(present_provider_detail(providers_.providers()[provider_index]));
    state_.clamp_rows(panel_.row_count());
    panel_.update_selection(state_.selected_row());
}

void SettingsCoordinator::move_selection(int delta)
{
    if (!is_open())
        return;
    if (state_.view() == SettingsView::SetupProviders) {
        state_.move_setup_providers(static_cast<int>(provider_preset_count()),
                                    SettingsPanel::kMaximumRows, delta);
        render_setup_providers();
        return;
    }
    if (state_.view() == SettingsView::Providers) {
        state_.move_providers(static_cast<int>(providers_.providers().size()) + 1,
                              SettingsPanel::kMaximumRows, delta);
        render_providers();
        return;
    }
    state_.move_rows(panel_.row_count(), delta);
    panel_.update_selection(state_.selected_row());
}

void SettingsCoordinator::sync_setup_provider_selection()
{
    state_.sync_setup_provider(
        static_cast<int>(provider_preset_index(providers_.setup_provider().family)),
        static_cast<int>(provider_preset_count()), SettingsPanel::kMaximumRows);
}

bool SettingsCoordinator::validate_setup(std::string *error)
{
    const SetupValidation validation = validate_setup_provider(providers_.setup_provider());
    if (validation.valid)
        return true;
    state_.set_selected_row(validation.row);
    if (error)
        *error = validation.error;
    return false;
}

int SettingsCoordinator::setup_initialize_row() const
{
    return panel_.row_count() - 1;
}

void SettingsCoordinator::update_setup_progress(const SetupProgress &progress,
                                                bool setup_in_flight)
{
    if (setup_in_flight && progress_.is_visible())
        progress_.update(progress);
}

void SettingsCoordinator::create_panel(lv_obj_t *parent)
{
    panel_.create(parent, &fonts_);
    state_.set_selected_row(0);
    render_main();
}

void SettingsCoordinator::apply(const SettingsPresentation &presentation)
{
    panel_.clear_rows();
    panel_.set_header(presentation.title, presentation.hint);
    for (const SettingsRowPresentation &row : presentation.rows)
        panel_.add_row(row.title, row.value);
}

}  // namespace zclaw
