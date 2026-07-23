#include "zclaw_settings_controller.h"

namespace zclaw {

SettingsView SettingsController::view() const
{
    return view_;
}

int SettingsController::selected_row() const
{
    return selected_row_;
}

void SettingsController::set_view(SettingsView view)
{
    view_ = view;
}

void SettingsController::reset_view(SettingsView view, int selected_row)
{
    view_ = view;
    selected_row_ = selected_row;
}

void SettingsController::set_selected_row(int selected_row)
{
    selected_row_ = selected_row;
}

void SettingsController::clamp_rows(int row_count)
{
    selected_row_ = move_row_selection(selected_row_, row_count, 0);
}

void SettingsController::move_rows(int row_count, int delta)
{
    selected_row_ = move_row_selection(selected_row_, row_count, delta);
}

const PagedSelection &SettingsController::provider_selection() const
{
    return providers_;
}

void SettingsController::reset_providers()
{
    providers_ = {};
    selected_row_ = 0;
}

void SettingsController::set_provider_selected(int selected_index, int item_count,
                                               int visible_rows)
{
    providers_ = move_paged_selection(selected_index, providers_.scroll_offset,
                                      item_count, visible_rows, 0);
    selected_row_ = providers_.visible_row;
}

void SettingsController::normalize_providers(int item_count, int visible_rows)
{
    providers_ = move_paged_selection(providers_.selected_index, providers_.scroll_offset,
                                      item_count, visible_rows, 0);
    selected_row_ = providers_.visible_row;
}

void SettingsController::move_providers(int item_count, int visible_rows, int delta)
{
    providers_ = move_paged_selection(providers_.selected_index, providers_.scroll_offset,
                                      item_count, visible_rows, delta);
    selected_row_ = providers_.visible_row;
}

const PagedSelection &SettingsController::setup_provider_selection() const
{
    return setup_providers_;
}

void SettingsController::sync_setup_provider(int selected_index, int item_count,
                                             int visible_rows)
{
    setup_providers_ = move_paged_selection(selected_index, 0, item_count, visible_rows, 0);
}

void SettingsController::normalize_setup_providers(int item_count, int visible_rows)
{
    setup_providers_ = move_paged_selection(setup_providers_.selected_index,
                                            setup_providers_.scroll_offset,
                                            item_count, visible_rows, 0);
    selected_row_ = setup_providers_.visible_row;
}

void SettingsController::move_setup_providers(int item_count, int visible_rows, int delta)
{
    setup_providers_ = move_paged_selection(setup_providers_.selected_index,
                                            setup_providers_.scroll_offset,
                                            item_count, visible_rows, delta);
    selected_row_ = setup_providers_.visible_row;
}

int SettingsController::provider_detail_index() const
{
    return provider_detail_index_;
}

void SettingsController::open_provider_detail(int provider_index)
{
    provider_detail_index_ = provider_index;
    selected_row_ = 0;
}

void SettingsController::close_provider_detail()
{
    provider_detail_index_ = -1;
}

ProviderEditField SettingsController::provider_edit_field() const
{
    return provider_edit_field_;
}

void SettingsController::set_provider_edit_field(ProviderEditField field)
{
    provider_edit_field_ = field;
}

SetupEditField SettingsController::setup_edit_field() const
{
    return setup_edit_field_;
}

void SettingsController::set_setup_edit_field(SetupEditField field)
{
    setup_edit_field_ = field;
}

SettingsActivation SettingsController::activation(int setup_initialize_row,
                                                  int provider_count,
                                                  bool setup_in_flight) const
{
    return settings_activation({view_, selected_row_, setup_initialize_row,
                                providers_.scroll_offset, provider_count,
                                setup_in_flight});
}

SettingsBackAction SettingsController::back_action(bool setup_in_flight,
                                                   bool first_run_needed) const
{
    return settings_back_action(view_, setup_in_flight, first_run_needed);
}

}  // namespace zclaw
