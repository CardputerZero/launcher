/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <functional>
#include <memory>
#include <string>

#include "lvgl_components.hpp"
#include "settings_tree_types.hpp"

class LvSettingRtcPage3 : public LvSettingValuePage3Base {
public:
    LvSettingRtcPage3();
    LvSettingRtcPage3(lv_obj_t *parent, const NodeIter &parent_node);
    LvSettingRtcPage3(lv_obj_t *parent,
                      const NodeIter &parent_node,
                      std::function<void()> back_callback);
    ~LvSettingRtcPage3() override;

    const std::string &last_error() const noexcept;

protected:
    int initial_selection() const override;

private:
    struct Impl;
    void install_actions();
    void restore_actions() noexcept;
    void create_status_label();
    void start_refresh();
    void set_error(const char *message);
    void clear_error();
    std::unique_ptr<Impl> impl_;
};

class LvSettingRtcConfirmPage3 : public LvSettingValuePage3Base {
public:
    LvSettingRtcConfirmPage3();
    LvSettingRtcConfirmPage3(lv_obj_t *parent,
                             const NodeIter &parent_node,
                             std::function<void()> back_callback = {});
    ~LvSettingRtcConfirmPage3() override;

    const std::string &last_error() const noexcept;

protected:
    int initial_selection() const override;

private:
    struct Impl;
    void install_actions();
    void restore_actions() noexcept;
    SettingApiResult discard_and_leave();
    SettingApiResult begin_save();
    SettingApiResult begin_save(bool allow_before_factory_time);
    void show_factory_time_warning(const ActivationSink &sink);
    void close_factory_time_warning() noexcept;
    void resolve_factory_time_warning(bool confirm);
    void handle_factory_time_warning_key(lv_event_t *event);
    void handle_factory_time_warning_click(lv_event_t *event);
    void create_status_label();
    void set_error(const char *message);
    void clear_error();
    void cancel_backend_request() noexcept;
    void leave_page();
    std::unique_ptr<Impl> impl_;
};

void settings_rtc_ntp_api(int command, void *data) noexcept;

// Number of days in the month currently held by the RTC edit session.  The
// "Day" entry is declared as a static 1..31 list, so its options have to be
// rebuilt from this before the day page is constructed.
int settings_rtc_days_in_current_month() noexcept;

// Drop unsaved manual time edits held by the RTC session.  Manual edits belong
// to a single visit to Date & Time, so leaving the submenu abandons them and a
// later visit cannot write a stale timestamp.
void settings_rtc_discard_edits() noexcept;

// Why "Set Manually" must refuse activation right now, or nullptr when it may
// open.  Deliberately the same NTP status the field pages re-read on entry, so
// the gate and their own guard cannot disagree.
const ActivationBlock *settings_rtc_manual_edit_block() noexcept;

// Re-read Network Time instead of reusing the per-process cache, so the status
// icon and the gate agree with what the field pages will see.  This is a local
// D-Bus property fetch (~35ms on hardware), not a blocking wait.
void settings_rtc_refresh_ntp() noexcept;

// "YYYY-MM-DD HH:MM:SS" from the system clock, and "On"/"Off"/"Unavailable" for
// Network Time.  The Date & Time Info page re-reads both every second, so these
// must stay cheap - the time comes from std::time, not from D-Bus.
std::string settings_rtc_local_time_text();
std::string settings_rtc_ntp_status_text();

std::unique_ptr<DComponens::LvglComponensBase> settings_rtc_info_page_factory(
    lv_obj_t *parent,
    const NodeIter &parent_node,
    std::function<void()> back_callback);

std::unique_ptr<DComponens::LvglComponensBase> settings_rtc_page_factory(
    lv_obj_t *parent,
    const NodeIter &parent_node,
    std::function<void()> back_callback);

std::unique_ptr<DComponens::LvglComponensBase> settings_rtc_confirm_page_factory(
    lv_obj_t *parent,
    const NodeIter &parent_node,
    std::function<void()> back_callback);
