#pragma once

#include "zclaw_provider_form_model.h"
#include "zclaw_selection_model.h"
#include "zclaw_settings_navigation_model.h"

namespace zclaw {

class SettingsController {
public:
    SettingsView view() const;
    int selected_row() const;
    void set_view(SettingsView view);
    void reset_view(SettingsView view, int selected_row = 0);
    void set_selected_row(int selected_row);
    void clamp_rows(int row_count);
    void move_rows(int row_count, int delta);

    const PagedSelection &provider_selection() const;
    void reset_providers();
    void set_provider_selected(int selected_index, int item_count, int visible_rows);
    void normalize_providers(int item_count, int visible_rows);
    void move_providers(int item_count, int visible_rows, int delta);

    const PagedSelection &setup_provider_selection() const;
    void sync_setup_provider(int selected_index, int item_count, int visible_rows);
    void normalize_setup_providers(int item_count, int visible_rows);
    void move_setup_providers(int item_count, int visible_rows, int delta);

    int provider_detail_index() const;
    void open_provider_detail(int provider_index);
    void close_provider_detail();

    ProviderEditField provider_edit_field() const;
    void set_provider_edit_field(ProviderEditField field);
    SetupEditField setup_edit_field() const;
    void set_setup_edit_field(SetupEditField field);

    SettingsActivation activation(int setup_initialize_row, int provider_count,
                                  bool setup_in_flight) const;
    SettingsBackAction back_action(bool setup_in_flight, bool first_run_needed) const;

private:
    SettingsView view_ = SettingsView::Main;
    int selected_row_ = 0;
    PagedSelection providers_;
    PagedSelection setup_providers_;
    int provider_detail_index_ = -1;
    ProviderEditField provider_edit_field_ = ProviderEditField::None;
    SetupEditField setup_edit_field_ = SetupEditField::None;
};

}  // namespace zclaw
