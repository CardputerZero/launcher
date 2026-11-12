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
    assert(parse_app_update_status("checking:2:7:m5cardputerzero-recorder", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Checking);
    assert(app_state.index == 2);
    assert(app_state.total == 7);
    assert(app_state.package == "m5cardputerzero-recorder");
    assert(parse_app_update_status("downloading:3:7:camera", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Downloading);
    assert(parse_app_update_status("installing:4:7:factorytest", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Installing);
    assert(parse_app_update_status(
        "succeeded:updated=m5cardputerzero-recorder:0.1.9,"
        "m5cardputerzero-files:1.0.2|failed=camera:install,", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Succeeded);
    assert(app_state.updated_count == 2);
    assert(app_state.failed_count == 1);
    assert(parse_app_update_status("succeeded:updated=|failed=", app_state));
    assert(app_state.updated_count == 0);
    assert(app_state.failed_count == 0);
    assert(parse_app_update_status("failed:unexpected", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Failed);
    assert(app_state.detail == "unexpected");
    assert(parse_app_update_status("cancelled", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Cancelled);
    assert(parse_app_update_status("cancelling:installing", app_state));
    assert(app_state.phase == AppUpdateState::Phase::Cancelling);
    assert(!parse_app_update_status("garbage", app_state));

    UpdateInfo update{"m5cardputerzero-recorder", "0.1.8", "0.1.9"};
    assert(update_changes_version(update));
    assert(!update_changes_version(UpdateInfo{"pkg", "1.0.0", "1.0.0"}));
    assert(!update_changes_version(UpdateInfo{"pkg", "", "1.0.0"}));
    assert(update_line(update) == "recorder 0.1.8 -> 0.1.9");
    assert(launcher_update_line("0.6.100", "0.7.0") ==
           "APPLaunch 0.6.100 -> 0.7.0");

    return 0;
}
