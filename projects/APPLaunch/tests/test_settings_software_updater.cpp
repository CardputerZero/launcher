/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "../main/ui/settings/settings_software_updater.hpp"

#include <atomic>
#include <cassert>
#include <map>
#include <string>
#include <vector>

namespace {

struct Fixture {
    std::map<std::string, std::string> installed = {
        {"applaunch", "0.6.100\n"},
        {"m5cardputerzero-recorder", "0.1.8\n"},
        {"m5cardputerzero-compass", "0.2.4\n"},
        {"m5cardputerzero-files", "1.0.0\n"},
    };
    std::map<std::string, std::string> releases = {
        {"Recorder",
         R"({"tag_name":"v1.0.0","assets":[{"name":"M5CardputerZero-Recorder","url":"x"},{"name":"m5cardputerzero-recorder_0.1.9_m5stack1_arm64.deb","url":"y"}]})"},
        {"Compass",
         R"({"tag_name":"v1.0.0","assets":[{"name":"m5cardputerzero-compass_0.2.4_m5stack1_arm64.deb","url":"y"}]})"},
        {"Files",
         R"({"tag_name":"v1.0.2","assets":[{"name":"m5cardputerzero-files_1.0.2_m5stack1_arm64.deb","url":"y"}]})"},
    };
    std::string launcher_metadata =
        "format=1\nversion=0.7.0\npackage_version=0.7.0-1\ncommit=abcdef1\n";
    std::string launcher_url = "https://example.invalid/launcher";
    std::vector<std::string> newer = {"0.1.9", "1.0.2", "0.7.0-1"};

    int capture(const std::vector<std::string> &arguments, std::string &output)
    {
        if (arguments.empty()) return -1;
        if (arguments[0] == "dpkg-query") {
            const auto found = installed.find(arguments.back());
            if (found == installed.end()) return 1;
            output = found->second;
            return 0;
        }
        if (arguments[0] == "curl") {
            const std::string &url = arguments.back();
            if (url.find(".update-info") != std::string::npos) {
                output = launcher_metadata;
                return 0;
            }
            for (const auto &entry : releases) {
                if (url.find("/" + entry.first + "/") == std::string::npos) continue;
                output = entry.second;
                return 0;
            }
            return 22;
        }
        if (arguments[0] == "dpkg" && arguments.size() >= 5 &&
            arguments[1] == "--compare-versions") {
            const std::string &candidate = arguments[2];
            const std::string &current = arguments[4];
            for (const std::string &value : newer) {
                if (value == candidate && value != current) return 0;
            }
            return 1;
        }
        return -1;
    }
};

settings_software::AppSpec spec(const std::string &package, const std::string &repo,
                            const std::string &prefix)
{
    return {package, repo, prefix, "_m5stack1_arm64.deb"};
}

} // namespace

int main()
{
    using namespace settings_software;

    AppSpec parsed;
    assert(parse_app_spec("m5cardputerzero-recorder|Recorder|m5cardputerzero-recorder_|_m5stack1_arm64.deb", parsed));
    assert(parsed.package == "m5cardputerzero-recorder");
    assert(parsed.repo == "Recorder");
    assert(!parse_app_spec("bad", parsed));
    assert(!parse_app_spec("pkg|repo|prefix|suffix", parsed));
    assert(!parse_app_spec("pkg|repo|prefix|suffix.deb|extra", parsed));

    const AppSpec recorder = spec("m5cardputerzero-recorder", "Recorder",
                                  "m5cardputerzero-recorder_");
    Fixture fixture;
    std::string version;
    assert(parse_asset_version(fixture.releases["Recorder"], recorder, version));
    assert(version == "0.1.9");
    assert(!parse_asset_version("{}", recorder, version));
    assert(safe_version("0.1.9"));
    assert(!safe_version("0.1.9|bad"));
    assert(!safe_version("0.1.9;bad"));

    LauncherUpdateInfo launcher_info;
    assert(launcher_update_info(
        "format=1\nversion=0.7.0\npackage_version=0.7.0-1\ncommit=abcdef1\n",
        launcher_info));
    assert(launcher_info.version == "0.7.0");
    assert(launcher_info.package_version == "0.7.0-1");
    assert(launcher_info.commit == "abcdef1");
    assert(!launcher_update_info("version=0.7.0\ncommit=abcdef1\n", launcher_info));

    const std::string html =
        "<a href=\"/CardputerZero/Piano/releases/download/v1.0.0/"
        "m5cardputerzero-piano_0.1.1_m5stack1_arm64.deb\">download</a>";
    assert(parse_asset_version_from_text(html, spec("m5cardputerzero-piano", "Piano",
                                                    "m5cardputerzero-piano_"), version));
    assert(version == "0.1.1");

    clear_release_cache();
    const AppSpec piano = spec("m5cardputerzero-piano", "Piano",
                               "m5cardputerzero-piano_");
    const auto html_capture = [&html](const std::vector<std::string> &arguments,
                                      std::string &output) {
        const std::string &last = arguments.back();
        if (last.find("api.github.com") != std::string::npos) return 22;
        if (last.find("/releases/latest") != std::string::npos) {
            output = "https://github.com/CardputerZero/Piano/releases/tag/v1.0.0\n";
            return 0;
        }
        if (last.find("expanded_assets") != std::string::npos) {
            output = html;
            return 0;
        }
        return -1;
    };
    std::string html_version;
    assert(latest_version(html_capture, piano, html_version));
    assert(html_version == "0.1.1");

    const std::vector<AppSpec> specs = {
        recorder,
        spec("m5cardputerzero-compass", "Compass", "m5cardputerzero-compass_"),
        spec("m5cardputerzero-files", "Files", "m5cardputerzero-files_"),
        spec("m5cardputerzero-piano", "Piano", "m5cardputerzero-piano_"),
    };
    std::atomic<bool> cancel{false};
    int last_progress = -1;
    clear_release_cache();
    const settings_software::Result result = check_software(
        specs,
        [&fixture](const std::vector<std::string> &arguments, std::string &output) {
            return fixture.capture(arguments, output);
        },
        fixture.launcher_url, cancel,
        [&last_progress](int value) { last_progress = value; });

    assert(result.code == 0);
    assert(last_progress == 100);
    const std::string &payload = result.stage;
    assert(payload.find("software|") == 0);
    assert(payload.find("checked=4") != std::string::npos);
    assert(payload.find("failed=0") != std::string::npos);
    assert(payload.find("skipped=1") != std::string::npos);
    assert(payload.find("updates=m5cardputerzero-recorder=0.1.8>0.1.9;"
                        "m5cardputerzero-files=1.0.0>1.0.2") != std::string::npos);
    assert(payload.find("launcher_known=1") != std::string::npos);
    assert(payload.find("launcher_available=1") != std::string::npos);
    assert(payload.find("launcher_installed=0.6.100") != std::string::npos);
    assert(payload.find("launcher_version=0.7.0") != std::string::npos);
    assert(payload.find("launcher_commit=abcdef1") != std::string::npos);

    fixture.releases.erase("Files");
    clear_release_cache();
    const settings_software::Result partial = check_software(
        specs,
        [&fixture](const std::vector<std::string> &arguments, std::string &output) {
            return fixture.capture(arguments, output);
        },
        fixture.launcher_url, cancel);
    assert(partial.code == 0);
    assert(partial.stage.find("failed=1") != std::string::npos);
    assert(partial.stage.find("m5cardputerzero-recorder=0.1.8>0.1.9") != std::string::npos);
    assert(partial.stage.find("m5cardputerzero-files") == std::string::npos);

    cancel.store(true);
    clear_release_cache();
    const settings_software::Result cancelled = check_software(
        specs,
        [&fixture](const std::vector<std::string> &arguments, std::string &output) {
            return fixture.capture(arguments, output);
        },
        fixture.launcher_url, cancel);
    assert(cancelled.code == -ECANCELED);

    return 0;
}
