/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "../main/ui/settings/settings_software_model.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

std::string write_status_file()
{
    char path[] = "/tmp/settings-software-status-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    close(descriptor);

    std::ofstream file(path);
    file << "Package: m5cardputerzero-recorder\n"
            "Status: install ok installed\n"
            "Version: 0.1.8\n"
            "Description: Recorder application for M5CardputerZero APPLaunch\n"
            " Recorder application, launcher entry, and runtime icon.\n"
            "\n"
            "Package: m5cardputerzero-compass\n"
            "Status: install ok installed\n"
            "Version: 0.2.4\n"
            "Description: Compass application for M5CardputerZero APPLaunch\n"
            " Compass application, launcher entry, and runtime icon.\n"
            "\n"
            "Package: camera\n"
            "Status: install ok installed\n"
            "Version: 1.0.1-m5stack1\n"
            "Description: Camera application for M5CardputerZero APPLaunch\n"
            "\n"
            "Package: obsolete-package\n"
            "Status: deinstall ok config-files\n"
            "Version: 9.9.9\n"
            "Description: Removed package\n"
            "\n";
    file.close();
    return path;
}

} // namespace

int main()
{
    using namespace settings_software;

    assert(app_definitions().size() == 14);
    assert(std::string(app_definitions().front().package) ==
           "m5cardputerzero-recorder");
    assert(std::string(app_definitions().back().package) ==
           "m5cardputerzero-keyboard-guide");
    assert(std::string(kAssetSuffix) == "_m5stack1_arm64.deb");

    assert(short_name("m5cardputerzero-recorder") == "recorder");
    assert(short_name("camera") == "camera");
    assert(short_name("m5cardputerzero-") == "m5cardputerzero-");

    const std::vector<InstalledApp> missing = read_installed_apps(
        "/nonexistent/settings-software-status");
    assert(missing.size() == app_definitions().size());
    for (const InstalledApp &app : missing) {
        assert(!app.installed);
        assert(app.version == "--");
        assert(app.description == "Not installed");
    }

    const std::string path = write_status_file();
    const std::vector<InstalledApp> apps = read_installed_apps(path);
    std::remove(path.c_str());
    assert(apps.size() == app_definitions().size());

    const auto app_for = [&apps](const std::string &package) -> const InstalledApp & {
        for (const InstalledApp &app : apps)
            if (app.package == package) return app;
        assert(false);
        return apps.front();
    };

    const InstalledApp &recorder = app_for("m5cardputerzero-recorder");
    assert(recorder.installed);
    assert(recorder.short_name == "recorder");
    assert(recorder.version == "0.1.8");
    assert(recorder.description ==
           "Recorder application for M5CardputerZero APPLaunch\n"
           "Recorder application, launcher entry, and runtime icon.");

    const InstalledApp &compass = app_for("m5cardputerzero-compass");
    assert(compass.installed);
    assert(compass.version == "0.2.4");

    const InstalledApp &camera = app_for("camera");
    assert(camera.installed);
    assert(camera.version == "1.0.1-m5stack1");
    assert(camera.description == "Camera application for M5CardputerZero APPLaunch");

    const InstalledApp &files = app_for("m5cardputerzero-files");
    assert(!files.installed);
    assert(files.version == "--");
    assert(files.description == "Not installed");

    CheckResult result;
    assert(parse_check_result(
        "succeeded:software|checked=14|failed=1|skipped=0|"
        "updates=m5cardputerzero-recorder=0.1.8>0.1.9;"
        "m5cardputerzero-files=1.0.0>1.0.2|"
        "launcher_known=1|launcher_available=1|"
        "launcher_installed=0.6.100|launcher_version=0.7.0|"
        "launcher_commit=abcdef1|launcher_failed=0",
        result));
    assert(result.checked == 14);
    assert(result.failed == 1);
    assert(result.skipped == 0);
    assert(result.updates.size() == 2);
    assert(result.updates[0].package == "m5cardputerzero-recorder");
    assert(result.updates[0].installed == "0.1.8");
    assert(result.updates[0].latest == "0.1.9");
    assert(result.updates[1].latest == "1.0.2");
    assert(result.launcher_known);
    assert(result.launcher_available);
    assert(result.launcher_installed == "0.6.100");
    assert(result.launcher_version == "0.7.0");
    assert(result.launcher_commit == "abcdef1");
    assert(!result.launcher_failed);

    CheckResult current;
    assert(parse_check_result(
        "succeeded:software|checked=14|failed=0|skipped=0|updates=|"
        "launcher_known=1|launcher_available=0|launcher_installed=0.6.100|"
        "launcher_version=0.6.100|launcher_commit=abcdef1|launcher_failed=0",
        current));
    assert(current.updates.empty());
    assert(!current.launcher_available);
    assert(!current.launcher_failed);

    CheckResult malformed;
    assert(!parse_check_result("succeeded:up-to-date|version=0.6.100", malformed));
    assert(!parse_check_result("failed:software-check:1", malformed));

    AppUpdateState app_state;
    assert(parse_app_update_status("starting", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Starting);
    assert(parse_app_update_status("checking:2:7:m5cardputerzero-recorder", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Checking);
    assert(app_state.index == 2);
    assert(app_state.total == 7);
    assert(app_state.package == "m5cardputerzero-recorder");
    // The live counters are optional so older status files stay readable.
    assert(parse_app_update_status("checking:2:7:m5cardputerzero-recorder:1:0",
                                   app_state));
    assert(app_state.updated_count == 1);
    assert(app_state.failed_count == 0);
    assert(parse_app_update_status("downloading:3:7:camera", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Downloading);
    assert(parse_app_update_status("installing:4:7:factorytest:1:1", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Installing);
    assert(app_state.updated_count == 1);
    assert(app_state.failed_count == 1);
    assert(parse_app_update_status(
        "succeeded:updated=m5cardputerzero-recorder:0.1.9,"
        "m5cardputerzero-files:1.0.2|failed=camera:install,", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Succeeded);
    assert(app_state.updated_count == 2);
    assert(app_state.failed_count == 1);
    assert(app_state.updated.size() == 2);
    assert(app_state.updated[0].package == "m5cardputerzero-recorder");
    assert(app_state.updated[0].version == "0.1.9");
    assert(app_state.failures.size() == 1);
    assert(app_state.failures[0].package == "camera");
    assert(app_state.failures[0].reason == "install");
    assert(parse_app_update_status("succeeded:updated=|failed=", app_state));
    assert(app_state.updated_count == 0);
    assert(app_state.failed_count == 0);
    assert(app_state.updated.empty());
    assert(app_state.failures.empty());
    assert(parse_app_update_status("failed:unexpected", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Failed);
    assert(app_state.detail == "unexpected");
    assert(parse_app_update_status("cancelled", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Cancelled);
    assert(parse_app_update_status("cancelling:installing", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Cancelling);
    assert(!parse_app_update_status("garbage", app_state));

    AppUpdateFailure warning;
    assert(parse_app_update_warning("m5cardputerzero-files:download\n", warning));
    assert(warning.package == "m5cardputerzero-files");
    assert(warning.reason == "download");
    assert(!parse_app_update_warning("", warning));
    assert(!parse_app_update_warning("download", warning));
    assert(!parse_app_update_warning(":download", warning));

    assert(app_update_failure_label("download") == "download failed");
    assert(app_update_failure_label("install") == "install failed");
    assert(app_update_failure_label("verify") == "verification failed");
    assert(app_update_failure_label("invalid-package") == "invalid package");
    assert(app_update_failure_label("metadata") == "check failed");
    assert(app_update_failure_label("release") == "check failed");
    assert(app_update_failure_label("version") == "check failed");
    assert(app_update_failure_label("") == "failed");
    assert(app_update_failure_label("custom") == "custom");

    {
        char warnings_path[] = "/tmp/settings-software-warnings-XXXXXX";
        const int warnings_descriptor = mkstemp(warnings_path);
        assert(warnings_descriptor >= 0);
        close(warnings_descriptor);
        {
            std::ofstream warnings_file(warnings_path);
            warnings_file << "camera:download\n"
                             "m5cardputerzero-files:install\n"
                             "\n";
        }
        const std::vector<AppUpdateFailure> warnings =
            read_app_update_warnings(warnings_path);
        std::remove(warnings_path);
        assert(warnings.size() == 2);
        assert(warnings[0].package == "camera");
        assert(warnings[0].reason == "download");
        assert(warnings[1].package == "m5cardputerzero-files");
        assert(warnings[1].reason == "install");
    }
    assert(read_app_update_warnings("/nonexistent/settings-software-warnings").empty());

    {
        const std::vector<UpdateInfo> expected = {
            {"m5cardputerzero-recorder", "0.1.8", "0.1.9"},
            {"m5cardputerzero-compass", "0.2.4", "1.0.1"},
            {"factorytest", "1.0.2-m5stack1", "1.0.3-m5stack1"},
        };
        const std::vector<InstalledPackage> installed = {
            {"m5cardputerzero-recorder", "0.1.9", true},
            {"m5cardputerzero-compass", "0.2.4", true},
            {"factorytest", "--", false},
        };
        AppUpdateState backend;
        backend.phase = AppUpdateState::Phase::Succeeded;
        backend.updated.push_back({"m5cardputerzero-recorder", "0.1.9"});
        backend.failures.push_back({"m5cardputerzero-compass", "install"});
        const std::vector<AppUpdateFailure> warnings = {
            {"m5cardputerzero-compass", "download"},
        };
        const AppUpdateResolution resolution =
            resolve_app_update(expected, installed, backend, warnings);
        assert(resolution.updated.size() == 1);
        assert(resolution.updated[0].package == "m5cardputerzero-recorder");
        assert(resolution.updated[0].version == "0.1.9");
        assert(resolution.failed.size() == 2);
        // Compass still reports the old version: the backend reason wins over
        // the warning recorded next to it.
        assert(resolution.failed[0].package == "m5cardputerzero-compass");
        assert(resolution.failed[0].reason == "install");
        // A package removed as an apt side effect is a failure, not an update.
        assert(resolution.failed[1].package == "factorytest");
        assert(resolution.failed[1].reason == "not installed");
    }

    {
        // A package the backend never mentions and that did not change is
        // reported as "not updated" instead of silently disappearing.
        const std::vector<UpdateInfo> expected = {
            {"m5cardputerzero-files", "1.0.2", "1.0.3"},
        };
        const std::vector<InstalledPackage> installed = {
            {"m5cardputerzero-files", "1.0.2", true},
        };
        AppUpdateState backend;
        backend.phase = AppUpdateState::Phase::Succeeded;
        const AppUpdateResolution resolution =
            resolve_app_update(expected, installed, backend, {});
        assert(resolution.updated.empty());
        assert(resolution.failed.size() == 1);
        assert(resolution.failed[0].reason == "not updated");
    }

    {
        // The installed version is authoritative: a stale backend failure must
        // not hide a successful install.
        const std::vector<UpdateInfo> expected = {
            {"camera", "1.0.3-m5stack1", "1.0.4-m5stack1"},
        };
        const std::vector<InstalledPackage> installed = {
            {"camera", "1.0.4-m5stack1", true},
        };
        AppUpdateState backend;
        backend.failures.push_back({"camera", "install"});
        const AppUpdateResolution resolution =
            resolve_app_update(expected, installed, backend, {});
        assert(resolution.updated.size() == 1);
        assert(resolution.updated[0].package == "camera");
        assert(resolution.failed.empty());
    }

    UpdateInfo update{"m5cardputerzero-recorder", "0.1.8", "0.1.9"};
    assert(update_changes_version(update));
    assert(!update_changes_version(UpdateInfo{"pkg", "1.0.0", "1.0.0"}));
    assert(!update_changes_version(UpdateInfo{"pkg", "", "1.0.0"}));
    assert(update_line(update) == "recorder 0.1.8 -> 0.1.9");
    assert(launcher_update_line("0.6.100", "0.7.0") ==
           "APPLaunch 0.6.100 -> 0.7.0");

    return 0;
}
