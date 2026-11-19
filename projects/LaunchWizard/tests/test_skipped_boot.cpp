/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

// Covers the skip-path first boot with LAUNCH_WIZARD_DRY_RUN=1: when the OOBE is
// skipped on a factory card (Imager provisioned the account), the wizard must
// expand the root filesystem and reboot instead of handing off, and the next
// boot must hand off without touching the partition table again. Every system
// command is printed instead of executed, so the test pins the command order
// without touching the host.

#include "wizard_service.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

// Keep in sync with the dry-run marker paths in wizard_service.cpp.
constexpr const char *kDryRunDirectory = "/tmp/LaunchWizard-dry-run";
constexpr const char *kFactoryMarker = "/tmp/LaunchWizard-dry-run/run-oobe";
constexpr const char *kRearmMarker = "/tmp/LaunchWizard-dry-run/rearm-run-oobe";
// The one-shot expansion attempt is redirected here via
// LAUNCH_WIZARD_EXPANSION_ATTEMPT so the test never sees another run's marker.
constexpr const char *kExpansionAttempt =
    "/tmp/LaunchWizard-dry-run/skipped-boot-expansion.attempt";

int failures = 0;

void expect(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "skipped boot test failed: " << message << '\n';
        ++failures;
    }
}

std::string read_file(const std::string &path)
{
    std::ifstream input(path);
    if (!input) return {};
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

// Redirects std::printf output produced by the dry-run command logger.
class StdoutCapture {
public:
    explicit StdoutCapture(const std::string &path)
    {
        file_ = std::fopen(path.c_str(), "w");
        std::fflush(stdout);
        original_ = dup(STDOUT_FILENO);
        if (file_ && original_ >= 0)
            dup2(fileno(file_), STDOUT_FILENO);
    }

    ~StdoutCapture() { stop(); }

    bool active() const { return file_ != nullptr && original_ >= 0; }

    void stop()
    {
        if (stopped_) return;
        stopped_ = true;
        std::fflush(stdout);
        if (file_) {
            if (original_ >= 0)
                dup2(original_, STDOUT_FILENO);
            std::fclose(file_);
            file_ = nullptr;
        }
        if (original_ >= 0) {
            close(original_);
            original_ = -1;
        }
    }

private:
    std::FILE *file_ = nullptr;
    int original_ = -1;
    bool stopped_ = false;
};

void enter_dry_run_directory()
{
    if (mkdir(kDryRunDirectory, 0700) != 0 && errno != EEXIST)
        std::cerr << "could not create " << kDryRunDirectory << ": "
                  << std::strerror(errno) << '\n';
}

void clear_markers()
{
    unlink(kFactoryMarker);
    unlink(kRearmMarker);
    unlink(kExpansionAttempt);
}

void create_factory_marker()
{
    std::FILE *file = std::fopen(kFactoryMarker, "w");
    if (file) std::fclose(file);
}

}  // namespace

int main()
{
    enter_dry_run_directory();
    clear_markers();
    if (setenv("LAUNCH_WIZARD_EXPANSION_ATTEMPT", kExpansionAttempt, 1) != 0) {
        std::cerr << "could not point the expansion attempt at the test marker\n";
        return 1;
    }

    const std::string log_path = std::string(kDryRunDirectory) + "/skipped-boot.log";

    // First boot: factory marker still present -> expand and reboot.
    create_factory_marker();
    std::string commands;
    {
        StdoutCapture capture(log_path);
        expect(capture.active(), "could not capture the dry-run command log");
        const bool rebooted = launch_wizard::WizardService::expand_rootfs_for_skipped_boot();
        capture.stop();
        expect(rebooted, "factory skip did not request a reboot");
        commands = read_file(log_path);
    }

    const std::size_t expansion = commands.find("/usr/bin/raspi-config --expand-rootfs");
    const std::size_t factory_removed = commands.find(std::string("rm -f ") + kFactoryMarker);
    const std::size_t rearm_removed = commands.find(std::string("rm -f ") + kRearmMarker);
    const std::size_t reboot = commands.find("systemctl reboot");
    const std::size_t handoff = commands.find("loginctl enable-linger");
    expect(expansion != std::string::npos,
           "factory skip did not run /usr/bin/raspi-config --expand-rootfs");
    expect(reboot != std::string::npos, "factory skip did not reboot");
    expect(factory_removed != std::string::npos,
           "factory skip did not drop the factory marker");
    expect(rearm_removed != std::string::npos,
           "factory skip did not drop the re-arm marker");
    expect(expansion != std::string::npos && factory_removed != std::string::npos &&
               factory_removed < reboot,
           "factory marker was not dropped between expansion and reboot");
    expect(expansion != std::string::npos && expansion < reboot,
           "root expansion was not issued before the reboot");
    expect(handoff == std::string::npos,
           "skip expansion also handed off to APPLaunch");
    expect(access(kExpansionAttempt, F_OK) == 0,
           "factory skip did not record the one-shot expansion attempt");

    // The attempt is recorded before raspi-config runs, so even with the factory
    // marker still in place (power loss before the marker cleanup) a second skip
    // must not expand or reboot again.
    std::string retry_commands;
    {
        StdoutCapture capture(log_path);
        expect(capture.active(), "could not capture the retry dry-run log");
        const bool rebooted = launch_wizard::WizardService::expand_rootfs_for_skipped_boot();
        capture.stop();
        expect(!rebooted, "a second skip retried the expansion");
        retry_commands = read_file(log_path);
    }
    expect(retry_commands.empty(), "a second skip issued commands again");

    // The dry-run logger never deletes files; model the reboot by clearing the
    // OOBE markers only (the attempt marker persists on the device), then check
    // the next boot hands off without expanding again.
    unlink(kFactoryMarker);
    unlink(kRearmMarker);
    std::string second_commands;
    {
        StdoutCapture capture(log_path);
        expect(capture.active(), "could not capture the second dry-run log");
        const bool rebooted = launch_wizard::WizardService::expand_rootfs_for_skipped_boot();
        capture.stop();
        expect(!rebooted, "skip after the expansion reboot requested another reboot");
        second_commands = read_file(log_path);
    }
    expect(second_commands.empty(),
           "skip after the expansion reboot issued commands again");

    // The skip branch's entry point must reach the expansion on a factory card.
    unlink(kExpansionAttempt);
    create_factory_marker();
    std::string finish_commands;
    {
        StdoutCapture capture(log_path);
        expect(capture.active(), "could not capture the finish dry-run log");
        const int result = launch_wizard::WizardService::finish_configured_system();
        capture.stop();
        expect(result == 0, "finish_configured_system reported a failure");
        finish_commands = read_file(log_path);
    }
    expect(finish_commands.find("/usr/bin/raspi-config --expand-rootfs") !=
               std::string::npos,
           "finish_configured_system did not expand the skipped factory card");
    expect(finish_commands.find("systemctl reboot") != std::string::npos,
           "finish_configured_system did not reboot after expansion");

    clear_markers();
    unlink(log_path.c_str());

    if (failures != 0) {
        std::cerr << "Wizard skipped boot tests failed (" << failures << ")\n";
        return 1;
    }
    std::cout << "Wizard skipped boot tests passed\n";
    return 0;
}
