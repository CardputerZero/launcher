#define APP_PAGE_IMPLEMENTATION_UNIT
#include "../ui_app_setup.hpp"
#include "setup_page_access.hpp"

#include <memory>

namespace setting {

Developer::Developer() = default;

Developer::~Developer()
{
    shutdown();
}

void Developer::shutdown()
{
    const uint64_t request_id = async_operation_.shutdown();
    if (request_id) cp0_signal_sudo_cancel(request_id, nullptr);
    stop_usb_guide_anims();
    if (pair_input_label_)
        lv_obj_remove_event_cb_with_user_data(
            pair_input_label_, view_object_delete_cb, this);
    if (pair_hint_label_)
        lv_obj_remove_event_cb_with_user_data(
            pair_hint_label_, view_object_delete_cb, this);
    pair_input_label_ = nullptr;
    pair_hint_label_ = nullptr;
    close_status();
}

void Developer::append(UISetupPage &p, std::vector<MenuItem> &menu)
{
    UISetupPage *page = &p;
    Developer *controller = this;
    MenuItem m;
    m.label = "Developer";
    bool adb_en = SetupPageAccess(p).config_get_int("adb_debug", 0) != 0;
    m.sub_items = {
        {"ADB", true, adb_en, [controller, page]() { controller->toggle_adb(*page); }},
        {"Pair host key", false, false,
         [controller, page]() { controller->enter_pair_view(*page); }},
        {"Authorizations: 0", false, false, nullptr},
        {"Manage authorized hosts", false, false,
         [controller, page]() { controller->enter_revoke_view(*page); }},
        {"Clear authorizations", false, false, [controller, page]() {
            SetupPageAccess(*page).confirm("Clear ADB authorizations?", [controller, page]() {
                controller->clear_authorizations(*page);
            });
        }},
    };
    m.on_enter = [controller, page]() { controller->refresh_adb_status(*page); };
    menu.push_back(m);
}

void Developer::toggle_adb(UISetupPage &page)
{
    SetupPageAccess access(page);
    MenuItem *menu = access.find_menu("Developer");
    if (!menu || menu->sub_items.size() < 3) return;
    if (async_operation_.active()) {
        menu->sub_items[0].toggle_state = !menu->sub_items[0].toggle_state;
        return;
    }
    bool want_on = menu->sub_items[0].toggle_state;
    const bool previous = !want_on;
    if (want_on) {
        refresh_adb_status(page);
        if (menu->sub_items[2].label == "Authorizations: 0") {
            menu->sub_items[0].toggle_state = false;
            enter_pair_view(page, true);
            return;
        }
    }
    const auto operation = async_operation_.begin();
    if (!operation)
        return;
    show_status(want_on ? "Enabling ADB" : "Disabling ADB", "Please wait...", Modal::BUSY);
    struct Context {
        Developer *developer;
        UISetupPage *page;
        AsyncOperationLifecycle::Token operation;
        bool desired;
        bool previous;
    };
    auto ctx = std::shared_ptr<Context>(
        new (std::nothrow) Context{this, &page, operation, want_on, previous});
    if (!ctx) {
        async_operation_.abort(operation);
        update_toggle(page, previous, false);
        show_status("ADB update failed", "Out of memory", Modal::ERROR);
        return;
    }
    uint64_t request_id = 0;
    int rc = -1;
    cp0_signal_system_admin_async({"AdbSet", want_on ? "1" : "0"}, 60000, 300000,
        [ctx](int result_code, int exit_code) noexcept {
            try {
            cp0_sudo_result_t result = static_cast<cp0_sudo_result_t>(result_code);
            if (!developer_async_completion_allowed(
                    ctx->operation.complete(), ctx->developer, ctx->page)) return;
            if (adb_toggle_succeeded(result, exit_code)) {
                const AdbPersistenceResult persistence = ctx->developer->update_toggle(
                    *ctx->page, ctx->desired, true, ctx->previous);
                if (persistence != AdbPersistenceResult::SAVED) {
                    ctx->developer->show_status(
                        "ADB changed; save failed",
                        adb_persistence_error_detail(persistence), Modal::ERROR);
                    return;
                }
                ctx->developer->close_status();
                if (adb_reboot_required(result, exit_code))
                    ctx->developer->enter_usb_guide(*ctx->page, ctx->desired);
                else
                    SetupPageAccess(*ctx->page).build_sub_view();
                return;
            }
            if (!ctx->developer->refresh_adb_status(*ctx->page))
                ctx->developer->update_toggle(*ctx->page, ctx->previous, false);
            if (result == CP0_SUDO_RESULT_CANCELLED) {
                ctx->developer->close_status();
                SetupPageAccess(*ctx->page).build_sub_view();
            } else {
                ctx->developer->show_result_error(result, exit_code);
            }
            } catch (...) {
            }
        }, [&](int code, uint64_t id) { rc = code; request_id = id; });
    if (rc != 0) {
        async_operation_.abort(operation);
        update_toggle(page, previous, false);
        show_status("ADB update failed", "Unable to start request", Modal::ERROR);
        return;
    }
    async_operation_.activate(operation, request_id);
}

bool Developer::refresh_adb_status(UISetupPage &page)
{
    MenuItem *menu = SetupPageAccess(page).find_menu("Developer");
    if (!menu || menu->sub_items.size() < 3) return false;
    int rc = -1;
    std::string out;
    cp0_signal_process_api({"AdbStatus"},
                           [&](int code, std::string data) { rc = code; out = std::move(data); });
    if (rc != 0) return false;
    AdbStatus status = parse_adb_status(out.c_str());
    if (!status.valid) return false;
    const bool previous = menu->sub_items[0].toggle_state;
    update_toggle(page, adb_state_after_failure(status, false), true, previous);
    char count[32];
    snprintf(count, sizeof(count), "Authorizations: %d", status.authorizations);
    menu->sub_items[2].label = count;
    authorizations_ = parse_adb_authorizations(out.c_str());
    model_.reconcile_authorization_count(authorizations_.size());
    return true;
}

void Developer::submit_pairing(UISetupPage &page)
{
    if (!adb_public_key_valid(model_.pair_key())) {
        if (pair_hint_label_) {
            lv_label_set_text(pair_hint_label_, "Invalid adbkey.pub public key");
            lv_obj_set_style_text_color(pair_hint_label_, lv_color_hex(0xEB5F5F), 0);
        }
        return;
    }
    run_admin_action(page, {"AdbAuthorize", model_.pair_key()}, "Pairing ADB host",
                     "Authorizing this key...", model_.enable_after_pair());
}

void Developer::clear_authorizations(UISetupPage &page)
{
    run_admin_action(page, {"AdbClearAuthorizations"}, "Clearing ADB keys",
                     "Revoking every host...", false);
}

void Developer::run_admin_action(UISetupPage &page, std::list<std::string> args,
                                 const char *title, const char *detail, bool enable_after)
{
    const auto operation = async_operation_.begin();
    if (!operation)
        return;
    show_status(title, detail, Modal::BUSY);
    struct Context { Developer *self; UISetupPage *page; AsyncOperationLifecycle::Token operation; bool enable; };
    auto ctx = std::shared_ptr<Context>(
        new (std::nothrow) Context{this, &page, operation, enable_after});
    if (!ctx) {
        async_operation_.abort(operation);
        show_status("ADB authorization failed", "Out of memory", Modal::ERROR);
        return;
    }
    uint64_t request_id = 0;
    int rc = -1;
    cp0_signal_system_admin_async(std::move(args), 60000, 30000,
        [ctx](int result_code, int exit_code) {
            if (!ctx->operation.complete()) return;
            auto result = static_cast<cp0_sudo_result_t>(result_code);
            if (result != CP0_SUDO_RESULT_SUCCESS) {
                ctx->self->show_result_error(result, exit_code);
                return;
            }
            ctx->self->model_.clear_pairing();
            ctx->self->close_status();
            SetupPageAccess access(*ctx->page);
            access.set_view(SetupViewState::SUB);
            ctx->self->refresh_adb_status(*ctx->page);
            access.build_sub_view();
            if (ctx->enable) {
                MenuItem *menu = access.find_menu("Developer");
                if (menu && !menu->sub_items.empty()) menu->sub_items[0].toggle_state = true;
                ctx->self->toggle_adb(*ctx->page);
            }
        }, [&](int code, uint64_t id) { rc = code; request_id = id; });
    if (rc != 0) {
        async_operation_.abort(operation);
        show_status("ADB authorization failed", "Unable to start request", Modal::ERROR);
        return;
    }
    async_operation_.activate(operation, request_id);
}

AdbPersistenceResult Developer::update_toggle(UISetupPage &page, bool enabled, bool save,
                                               bool rollback_value)
{
    SetupPageAccess access(page);
    MenuItem *menu = access.find_menu("Developer");
    if (!menu || menu->sub_items.empty()) return AdbPersistenceResult::NOT_REQUESTED;
    menu->sub_items[0].toggle_state = enabled;
    if (!save) return AdbPersistenceResult::NOT_REQUESTED;

    const bool set_succeeded = access.config_set_int("adb_debug", enabled ? 1 : 0);
    if (!set_succeeded) {
        const AdbPersistenceResult result =
            adb_persistence_result(false, false, false, false);
        menu->sub_items[0].toggle_state =
            adb_visible_state_after_persistence(result, enabled, rollback_value);
        return result;
    }
    const bool save_succeeded = access.config_save();
    if (save_succeeded)
        return adb_persistence_result(true, true, false, false);

    const bool rollback_set_succeeded =
        access.config_set_int("adb_debug", rollback_value ? 1 : 0);
    const bool rollback_save_succeeded = rollback_set_succeeded && access.config_save();
    const AdbPersistenceResult result = adb_persistence_result(
        true, false, rollback_set_succeeded, rollback_save_succeeded);
    menu->sub_items[0].toggle_state =
        adb_visible_state_after_persistence(result, enabled, rollback_value);
    return result;
}

void Developer::show_result_error(cp0_sudo_result_t result, int exit_code)
{
    const char *reason = "Command failed";
    switch (classify_privileged_result(static_cast<int>(result))) {
    case PrivilegedResultKind::AUTH_FAILED: reason = "Authentication failed"; break;
    case PrivilegedResultKind::CANCELLED: reason = "Request cancelled"; break;
    case PrivilegedResultKind::TIMED_OUT: reason = "Request timed out"; break;
    case PrivilegedResultKind::EXEC_FAILED:
        reason = exit_code < 0 ? "Unable to start command" : "Command returned an error";
        break;
    default: break;
    }
    show_status("ADB update failed", reason, Modal::ERROR);
}

} // namespace setting
