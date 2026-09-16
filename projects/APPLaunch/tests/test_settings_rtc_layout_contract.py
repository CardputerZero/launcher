# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT

"""Layout and behaviour contracts for Settings -> Date & Time.

These guard the fixes that made the RTC pages usable on real hardware:

* the error banner sits in the value page's empty top-left corner and wraps,
  instead of overlapping the value list;
* a nested roller page comes to rest in the right-hand column, so the
  second-level panel and the third-level page never paint over each other;
* the confirm-page title fits its title box;
* the local-time read moves its callback exactly once;
* "No" on the confirm page really discards, and switching the NTP mode is not
  blocked by unsaved manual edits;
* the "Day" option list is rebuilt from the selected month.

The checks are static (regex over the sources) so they need no toolchain and
run with the rest of the Python contracts.
"""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RTC_CPP = (ROOT / "main/ui/settings/settings_rtc_page.cpp").read_text(encoding="utf-8")
RTC_HPP = (ROOT / "main/ui/settings/settings_rtc_page.hpp").read_text(encoding="utf-8")
SUBMENU_CPP = (ROOT / "main/ui/settings/settings_submenu_page.cpp").read_text(
    encoding="utf-8"
)
SUBMENU_HPP = (ROOT / "main/ui/settings/settings_submenu_page.hpp").read_text(
    encoding="utf-8"
)
STATIC_CPP = (ROOT / "main/ui/settings/settings_static_info_page.cpp").read_text(
    encoding="utf-8"
)
PAGE_CPP = (ROOT / "main/ui/settings/settings_page.cpp").read_text(encoding="utf-8")
TREE_HPP = (ROOT / "main/ui/settings/settings_tree_types.hpp").read_text(
    encoding="utf-8"
)
COMPONENTS_HPP = (ROOT / "main/ui/settings/lvgl_components.hpp").read_text(
    encoding="utf-8"
)

# Line height used by the 10px bold status font, and the row geometry of the
# value page (LvSettingValuePage3Base::LayoutMetric).
STATUS_LINE_HEIGHT = 13


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def function_body(source: str, signature: str) -> str:
    """Return the body of a definition, with comments removed.

    `signature` must match the definition (not a forward declaration); pass the
    trailing newline and opening brace when a declaration exists too.
    """
    start = source.index(signature)
    opening_brace = source.index("{", start)
    depth = 0
    for index in range(opening_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return strip_comments(source[opening_brace + 1 : index])
    raise AssertionError(f"unterminated function: {signature}")


def normalized(source: str) -> str:
    return re.sub(r"\s+", " ", source).strip()


def metric(source: str, name: str) -> int:
    match = re.search(rf"\b{name}\s*=\s*(\d+)", source)
    assert match, f"missing layout metric: {name}"
    return int(match.group(1))


def test_status_banner_never_overlaps_the_value_column():
    status_x = metric(RTC_CPP, "StatusLabelX")
    status_w = metric(RTC_CPP, "StatusLabelW")
    status_y = metric(RTC_CPP, "StatusLabelY")
    value_left = metric(COMPONENTS_HPP, "ValueListX") + metric(COMPONENTS_HPP, "ValueBoxX")
    bar_top = metric(COMPONENTS_HPP, "BarY")

    # Value text never starts left of the value column's text box, so the
    # banner must stay clear of it.
    assert status_x + status_w < value_left, (status_x, status_w, value_left)
    # The longest message ("Disable NTP before editing time") wraps to two
    # lines; even four lines must stay above the selection bar.
    assert status_y + 4 * STATUS_LINE_HEIGHT < bar_top, (status_y, bar_top)
    # And it must not be truncated instead of wrapped.
    assert RTC_CPP.count("LV_LABEL_LONG_WRAP") >= 2


def test_nested_roller_page_rests_in_the_right_hand_column():
    assert "int panel_x() const override;" in SUBMENU_HPP

    base_body = normalized(
        function_body(COMPONENTS_HPP, "virtual int panel_x() const")
    )
    assert "return 0;" in base_body

    roller_body = normalized(
        function_body(SUBMENU_CPP, "int LvSettingRollerPage2::panel_x() const")
    )
    assert "return metric(LayoutMetric::PanelX);" in roller_body

    animate = normalized(
        function_body(
            SUBMENU_CPP, "void LvSettingRollerPage2::animate_page3_root(bool entering)"
        )
    )
    # Coming in the page still slides from off-screen right; only the resting
    # position moved.  Regressing this would remove the slide-in animation.
    assert "const int start_x = entering ? metric(LayoutMetric::PageWidth) : rest_x;" in animate
    assert "const int end_x = entering ? rest_x : metric(LayoutMetric::PageWidth);" in animate
    assert "const int rest_x = roller3_->panel_x();" in animate

    finish = normalized(
        function_body(
            SUBMENU_CPP, "void LvSettingRollerPage2::finish_page3_transition(bool entering)"
        )
    )
    assert "lv_obj_set_x(roller3_->Get(), roller3_->panel_x());" in finish


def test_confirm_page_title_fits_the_title_box():
    title_box_width = metric(COMPONENTS_HPP, "TitleBoxW")
    # Only the short label is used; the long one was clipped to "Write hardv".
    assert 'SettingEntry{"Write RTC?", settings_rtc_confirm_page_factory}' in PAGE_CPP
    assert "Write hardware RTC?" not in PAGE_CPP
    # 16px CJK text: "Write RTC?" measures ~80px, leaving headroom in the box.
    assert len("Write RTC?") * 8 <= title_box_width


def test_local_time_read_moves_its_callback_exactly_once():
    # A forward declaration of this function exists, so anchor on the definition.
    body = function_body(
        RTC_CPP, "int read_local_time_async(TimeReadCallback callback)\n{"
    )
    assert body.count("std::move(callback)") == 1
    # The old revision moved it into an unused wrapper first, so the backend
    # handler captured an empty std::function and never delivered a result.
    assert "deliver" not in body


def test_confirm_page_no_path_discards_instead_of_failing():
    body = normalized(
        function_body(
            RTC_CPP, "SettingApiResult LvSettingRtcConfirmPage3::discard_and_leave()"
        )
    )
    # activation_pending() is true for this very activation, so guarding on it
    # made "No" fail every time.
    assert "activation_pending()" not in body
    assert "impl_->request_state" in body
    assert "discard_edits();" in body
    assert "SettingApiResult::Success" in body


def test_ntp_toggle_is_not_blocked_by_unsaved_edits():
    body = normalized(
        function_body(RTC_CPP, "void settings_rtc_ntp_api(int command, void *data) noexcept")
    )
    discard_at = body.index("session.discard_edits();")
    toggle_at = body.index("begin_ntp_toggle")
    assert discard_at < toggle_at
    assert "void settings_rtc_discard_edits() noexcept;" in RTC_HPP


def test_day_options_are_rebuilt_from_the_selected_month():
    body = normalized(
        function_body(PAGE_CPP, "static std::unique_ptr<DComponens::LvglComponensBase> rtc_page3_factory")
    )
    assert 'page_node->label == "Day"' in body
    assert "settings_tree_factory_context()" in body
    assert "erase_children(page_node)" in body
    assert "settings_rtc_days_in_current_month()" in body

    days_body = normalized(
        function_body(RTC_CPP, "int settings_rtc_days_in_current_month() noexcept")
    )
    # Month-aware by construction: the model owns the leap-year rule.
    assert "field_max(settings_rtc::RtcField::DAY)" in days_body


def test_manual_entry_is_gated_while_network_time_is_on():
    """Entering Set Manually must be refused up front, not field by field."""
    assert "struct ActivationBlock" in TREE_HPP
    assert "std::function<const ActivationBlock *()> activation_gate;" in TREE_HPP

    # The gate runs before any navigation or activation is dispatched.
    handler = normalized(
        function_body(
            SUBMENU_CPP, "void LvSettingRollerPage2::handle_key_event(lv_event_t *event)"
        )
    )
    gate_at = handler.index("selected_node->activation_gate")
    assert gate_at < handler.index("LoadNextPage()")
    assert gate_at < handler.index("selected_node->Componens_api(SettingApiActivate")
    assert "show_blocked_warning(block->title, block->message)" in handler

    # Set Manually carries a gate, and the reason is chosen per NTP state.
    assert 'SettingEntry manual_entry{"Set Manually", roller_page_factory};' in PAGE_CPP
    assert (
        "manual_entry.activation_gate = [] { return settings_rtc_manual_edit_block(); };"
        in PAGE_CPP
    )

    block = normalized(
        function_body(
            RTC_CPP, "const ActivationBlock *settings_rtc_manual_edit_block() noexcept"
        )
    )
    assert "workflow.pending()" in block
    assert "state.ntp_available()" in block
    assert "state.ntp_on()" in block
    # Honest copy per case: an unknown status must not claim Network Time is on.
    assert '"Network Time is on"' in block
    assert '"Network Time status unavailable"' in block
    assert "return nullptr;" in block

    # The gate's input is refreshed once per visit, so it cannot consult a stale
    # per-process cache while the field pages read a fresh value.
    assert "settings_rtc_refresh_ntp();" in PAGE_CPP
    refresh = normalized(
        function_body(RTC_CPP, "void settings_rtc_refresh_ntp() noexcept")
    )
    # function_body() strips comments, so this reads refresh_ntp_cache(true).
    assert "refresh_ntp_cache(true)" in refresh


def test_generalised_warning_keeps_the_bluetooth_copy():
    wrapper = normalized(
        function_body(SUBMENU_CPP, "void LvSettingRollerPage2::show_power_warning()")
    )
    assert '"Bluetooth power is off"' in wrapper
    assert '"Turn on Power before continuing."' in wrapper
    # The Bluetooth entry point is unchanged.
    assert "void show_power_warning();" in SUBMENU_HPP
    assert "page->show_power_warning();" in PAGE_CPP

    # The modal itself is text-driven now, with a sane fallback.
    modal = normalized(
        function_body(
            SUBMENU_CPP,
            "void LvSettingRollerPage2::show_blocked_warning(const char *title_text, "
            "const char *message_text)",
        )
    )
    assert "title_text ? title_text" in modal
    assert "message_text ? message_text" in modal


def test_gate_precedes_every_activation_branch_and_does_not_fall_through():
    """A refused activation must not still open the page."""
    handler = normalized(
        function_body(
            SUBMENU_CPP, "void LvSettingRollerPage2::handle_key_event(lv_event_t *event)"
        )
    )
    gate = handler.index("if (selected_node->activation_gate)")
    call = handler.index(
        "if (const ActivationBlock *block = selected_node->activation_gate())"
    )
    show = handler.index("show_blocked_warning(block->title, block->message)")
    factory = handler.index("if (selected_node->page_factory)")
    api = handler.index("else if (selected_node->Componens_api)")
    page = handler.index("else if (on_selected_page)")
    assert gate < call < show < factory < api < page, (gate, call, show, factory, api, page)

    # Between the modal and the first activation branch the key must be
    # consumed, otherwise the page opens anyway.
    consumed = handler[show:factory]
    assert "lv_event_stop_processing(event);" in consumed
    assert "return;" in consumed


def test_blocked_modal_swallows_keys_and_does_not_navigate():
    """While the modal is up it owns every key, and closing it must not navigate."""
    handler = normalized(
        function_body(
            SUBMENU_CPP, "void LvSettingRollerPage2::handle_key_event(lv_event_t *event)"
        )
    )
    modal = handler.index("if (power_warning_)")
    navigation = handler.index("if (key == LV_KEY_ESC || key == LV_KEY_LEFT)")
    assert modal < navigation
    segment = handler[modal:navigation]
    assert "close_power_warning();" in segment
    assert "lv_event_stop_processing(event);" in segment
    assert "LeaveSelfPage" not in segment
    assert "LoadNextPage" not in segment


def test_gate_is_attached_before_the_entry_is_moved_into_the_tree():
    """Setting the gate after the move would store an ungated entry."""
    source = strip_comments(PAGE_CPP)
    body = normalized(
        source[
            source.index("void UISettingTreePage::create_page_detail()") : source.index(
                "void UISettingTreePage::back_home"
            )
        ]
    )
    assert (
        "manual_entry.activation_gate = [] { return settings_rtc_manual_edit_block(); };"
        in body
    )
    assert body.index("manual_entry.activation_gate") < body.index("std::move(manual_entry)")
    assert "mode_tree.append_child(date_time, std::move(manual_entry));" in body


def test_block_reasons_are_static_and_cover_all_three_states():
    """The reasons must outlive the entry (static storage) and be exhaustive."""
    block = normalized(
        function_body(
            RTC_CPP, "const ActivationBlock *settings_rtc_manual_edit_block() noexcept"
        )
    )
    assert block.count("static constexpr ActivationBlock") == 3
    assert "if (workflow.pending()) return &kInFlight;" in block
    assert "if (!state.ntp_available()) return &kUnavailable;" in block
    assert "if (state.ntp_on()) return &kNetworkTimeOn;" in block
    assert "return nullptr;" in block


def test_page2_own_offset_still_matches_page_object_base_x():
    """page2_target_x() reuses panel_x(): that identity only holds while the
    page-2 objects are based at PanelX and nothing derives from the class."""
    base = normalized(
        function_body(
            SUBMENU_CPP, "int LvSettingRollerPage2::page_object_base_x(lv_obj_t *object) const"
        )
    )
    assert (
        "if (object == selection_bg_ || object == ComponensObj) "
        "return metric(LayoutMetric::PanelX);" in base
    )
    assert "return entering ? base_x - panel_x() : base_x;" in normalized(
        function_body(
            SUBMENU_CPP,
            "int LvSettingRollerPage2::page2_target_x(lv_obj_t *object, bool entering) const",
        )
    )
    assert "public LvSettingRollerPage2" not in SUBMENU_HPP
    assert "public LvSettingRollerPage2" not in SUBMENU_CPP


def test_set_manually_does_not_offer_seconds():
    """Seconds stay in the model (a timestamp needs them) but are not editable."""
    source = strip_comments(PAGE_CPP)
    body = normalized(
        source[
            source.index("NodeIter manual = mode_tree.append_child(date_time") : source.index(
                "NodeIter write_rtc"
            )
        ]
    )
    for label in ('"Year"', '"Month"', '"Day"', '"Hour"', '"Minute"'):
        assert f"SettingEntry{{{label}, rtc_page3_factory}}" in body, label
    assert 'SettingEntry{"Second"' not in body
    assert 'SettingEntry{"Second"' not in PAGE_CPP
    # The model still holds seconds: parse/format address the field by index.
    assert "SECOND" in RTC_CPP


def test_info_page_clock_keeps_ticking():
    """A frozen clock is the one thing an Info page must not show."""
    factory = normalized(
        function_body(
            RTC_CPP,
            "std::unique_ptr<DComponens::LvglComponensBase> settings_rtc_info_page_factory(",
        )
    )
    assert "page->set_lines_provider(" in factory
    assert 'lines[0] = "Current: " + settings_rtc_local_time_text();' in factory
    assert 'lines[1] = "Network Time: " + settings_rtc_ntp_status_text();' in factory

    # A timer drives the provider, and the page owns and deletes it.
    static = normalized(STATIC_CPP)
    assert "lv_timer_create(&LvSettingStaticInfoPage3::refresh_lines_cb, 1000, this)" in static
    assert "lv_timer_delete(lines_timer_)" in static

    # The per-second read must stay cheap: std::time, never D-Bus.
    text = normalized(function_body(RTC_CPP, "std::string settings_rtc_local_time_text()"))
    assert "std::time(nullptr)" in text
    assert "localtime_r" in text


def test_field_page_keeps_edits_made_on_an_earlier_field_page():
    """Opening a second field page must not wipe the first page's edit."""
    refresh = normalized(
        function_body(RTC_CPP, "void LvSettingRtcPage3::start_refresh()")
    )
    assert "const bool keep_edits = workflow.state().dirty();" in refresh
    # Short-circuit: load_local_time() (which clears dirty_) must not run while
    # edits are pending, or setting more than one field becomes impossible.
    assert "keep_edits ||" in refresh
    assert refresh.index("keep_edits ||") < refresh.index("load_local_time(result.time.payload)")


if __name__ == "__main__":
    # Auto-discover instead of keeping a hand-written list: a list that falls
    # behind the definitions silently skips tests, which is how two of these
    # (including the activation-gate one) once ran nowhere.
    for _name, _test in sorted(globals().items()):
        if _name.startswith("test_") and callable(_test):
            _test()
