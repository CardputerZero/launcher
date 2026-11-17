/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace settings_software {

/**
 * The applications installed by the CardputerZero image stage
 * (`pi-gen/stage2/05-cardputerzero/03-run.sh`).
 *
 * The order is the order used by that script and is also the order shown in
 * Settings -> System -> Software.
 */
struct AppDefinition {
    const char *short_name;
    const char *package;
    const char *repo;
    const char *asset_prefix;
};

inline constexpr const char *kAssetSuffix = "_m5stack1_arm64.deb";

const std::vector<AppDefinition> &app_definitions();

/** `m5cardputerzero-recorder` -> `recorder`; other names are returned as-is. */
std::string short_name(const std::string &package);

struct InstalledApp {
    std::string package;
    std::string short_name;
    std::string version = "--";
    std::string description;
    bool installed = false;
};

/**
 * Read the installed versions and descriptions of the known applications from
 * a dpkg status database.  Missing packages are reported as not installed
 * instead of being dropped, so the Software page always shows the full list.
 */
std::vector<InstalledApp> read_installed_apps(
    const std::string &status_path = "/var/lib/dpkg/status");

struct UpdateInfo {
    std::string package;
    std::string installed;
    std::string latest;
};

struct CheckResult {
    std::vector<UpdateInfo> updates;
    int checked = 0;
    int failed = 0;
    int skipped = 0;
    bool launcher_known = false;
    bool launcher_available = false;
    bool launcher_failed = false;
    std::string launcher_installed;
    std::string launcher_version;
    std::string launcher_commit;
};

/**
 * Parse the payload produced by settings_software::check_software.
 *
 * The payload has the form
 * `software|checked=N|failed=N|skipped=N|updates=pkg=installed>latest;...|
 *  launcher_known=0|1|launcher_available=0|1|launcher_installed=...|
 *  launcher_version=...|launcher_commit=...|launcher_failed=0|1`.
 * A `succeeded:` prefix produced by `UpdateJobStatus` is accepted as well.
 */
bool parse_check_result(const std::string &payload, CheckResult &result);

struct AppUpdateEntry {
    std::string package;
    std::string version;
};

struct AppUpdateFailure {
    std::string package;
    std::string reason;
};

struct AppUpdateState {
    enum class Phase {
        Unknown,
        Starting,
        Checking,
        Downloading,
        Installing,
        Succeeded,
        Failed,
        Cancelled,
        Cancelling,
    };

    Phase phase = Phase::Unknown;
    int index = 0;
    int total = 0;
    std::string package;
    int updated_count = 0;
    int failed_count = 0;
    std::string detail;
    std::vector<AppUpdateEntry> updated;
    std::vector<AppUpdateFailure> failures;
};

/**
 * Parse one line from `/var/lib/applaunch-updater/apps-status`.
 *
 * Progress lines are
 * `starting` and `checking|downloading|installing:<index>:<total>:<pkg>`,
 * optionally followed by the running `:<updated>:<failed>` counters.  Terminal
 * lines are `succeeded:updated=pkg:ver,...|failed=pkg:reason,...`,
 * `failed:<detail>`, or `cancelled`.
 */
bool parse_app_update_status(const std::string &status, AppUpdateState &result);

/**
 * Parse one line from `/var/lib/applaunch-updater/apps-warnings`.
 *
 * The updater appends `package:reason` for every package that failed while the
 * run continues, so the progress dialog can show (for example) a download
 * failure before the run finishes.
 */
bool parse_app_update_warning(const std::string &line, AppUpdateFailure &failure);

/** Human readable text for a backend failure reason: `download` -> "download failed". */
std::string app_update_failure_label(const std::string &reason);

/** Read the append-only warnings file; a missing file yields no entries. */
std::vector<AppUpdateFailure> read_app_update_warnings(const std::string &path);

struct InstalledPackage {
    std::string package;
    std::string version;
    bool installed = false;
};

struct AppUpdateResolution {
    std::vector<AppUpdateEntry> updated;
    std::vector<AppUpdateFailure> failed;
};

/**
 * Resolve what an app update run actually achieved.
 *
 * `expected` is the update check result, `installed` the versions read after
 * the run, `backend` the terminal status line and `warnings` the per-package
 * failure lines.  A package that is no longer installed or still reports the
 * old version is a failure, never an update.
 */
AppUpdateResolution resolve_app_update(
    const std::vector<UpdateInfo> &expected,
    const std::vector<InstalledPackage> &installed,
    const AppUpdateState &backend,
    const std::vector<AppUpdateFailure> &warnings);

/**
 * Defensive display guard: the backend only reports versions newer than the
 * installed one, but never show an entry whose two version strings are equal.
 */
bool update_changes_version(const UpdateInfo &update);

/** `recorder 0.1.8 -> 0.1.9` */
std::string update_line(const UpdateInfo &update);

/** `APPLaunch 0.6.100 -> 0.7.0` */
std::string launcher_update_line(const std::string &installed,
                                 const std::string &latest);

} // namespace settings_software
