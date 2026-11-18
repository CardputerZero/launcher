/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

// Runs the real WizardService::apply() pipeline with LAUNCH_WIZARD_DRY_RUN=1:
// every system command is printed instead of executed, so the test can pin the
// step count, the step order (in particular that the first-boot root
// filesystem expansion runs after the APPLaunch handoff and before the final
// step), and the exact command that is issued, without touching the host.

#include "wizard_model.h"
#include "wizard_service.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

std::string make_test_directory()
{
    char pattern[] = "/tmp/launch-wizard-apply-XXXXXX";
    char *directory = mkdtemp(pattern);
    return directory ? directory : std::string{};
}

void remove_test_directory(const std::string &directory)
{
    unlink((directory + "/apply.state").c_str());
    unlink((directory + "/commands.log").c_str());
    rmdir(directory.c_str());
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

} // namespace

int main()
{
    using namespace launch_wizard;

    int failures = 0;
    const auto expect = [&failures](bool condition, const char *message) {
        if (condition) return;
        std::cerr << message << '\n';
        ++failures;
    };

    const std::string directory = make_test_directory();
    if (directory.empty()) {
        std::cerr << "could not create the apply pipeline test directory\n";
        return 1;
    }
    const std::string checkpoint_path = directory + "/apply.state";
    const std::string log_path = directory + "/commands.log";
    if (setenv("LAUNCH_WIZARD_APPLY_CHECKPOINT", checkpoint_path.c_str(), 1) != 0) {
        std::cerr << "could not point the apply checkpoint at the test directory\n";
        remove_test_directory(directory);
        return 1;
    }

    WizardModel model;
    // Skip the Wi-Fi/Ethernet branch: the pipeline's network step is not what
    // this test covers, and skipping it keeps the dry-run log deterministic.
    model.network_skipped = true;
    model.ssh_enabled = true;

    std::vector<int> steps;
    std::vector<std::string> labels;
    std::string error;
    {
        StdoutCapture capture(log_path);
        expect(capture.active(), "could not capture the dry-run command log");
        error = WizardService::apply(
            model, [&steps, &labels](const ProgressEvent &event) {
                steps.push_back(event.step);
                labels.push_back(event.label);
            });
    }
    unsetenv("LAUNCH_WIZARD_APPLY_CHECKPOINT");

    const bool checkpoint_cleared = access(checkpoint_path.c_str(), F_OK) != 0;
    const std::string commands = read_file(log_path);
    remove_test_directory(directory);

    if (!error.empty())
        std::cerr << "apply pipeline error: " << error << '\n';
    expect(error.empty(), "dry-run apply returned an error");

    const std::size_t expected_steps = static_cast<std::size_t>(kApplyStepCount);
    expect(steps.size() == expected_steps,
           "apply reported a different number of progress events than kApplyStepCount");
    expect(labels.size() == steps.size(),
           "apply progress labels and step numbers diverged");
    for (std::size_t index = 0; index < steps.size(); ++index)
        expect(steps[index] == static_cast<int>(index) + 1,
               "apply progress steps were not sequential");

    const auto find_label = [&labels](const char *label) -> std::size_t {
        for (std::size_t index = 0; index < labels.size(); ++index)
            if (labels[index] == label) return index;
        return labels.size();
    };
    expect(find_label("Enabling APPLaunch service...") + 1 < labels.size() &&
               find_label("Expanding root filesystem...") ==
                   static_cast<std::size_t>(kApplyStepCount) - 2,
           "root expansion is not the step before finalization");
    expect(find_label("Finalizing configuration...") ==
               static_cast<std::size_t>(kApplyStepCount) - 1,
           "finalization is not the last apply step");
    expect(find_label("Enabling APPLaunch service...") + 1 ==
               find_label("Expanding root filesystem..."),
           "root expansion does not follow the APPLaunch service step");

    // The requirement itself: the packaged raspi-config entry point is invoked
    // with --expand-rootfs before the wizard disables its own service.
    const std::size_t expansion = commands.find("/usr/bin/raspi-config --expand-rootfs");
    const std::size_t finalization = commands.find("systemctl disable LaunchWizard.service");
    expect(expansion != std::string::npos,
           "dry-run apply did not issue /usr/bin/raspi-config --expand-rootfs");
    expect(finalization != std::string::npos,
           "dry-run apply did not disable LaunchWizard.service");
    expect(expansion != std::string::npos && finalization != std::string::npos &&
               expansion < finalization,
           "root expansion was not issued before the wizard disabled its service");

    expect(checkpoint_cleared, "successful apply did not clear its checkpoint");

    if (failures != 0) {
        std::cerr << "Wizard apply pipeline tests failed (" << failures << ")\n";
        return 1;
    }
    std::cout << "Wizard apply pipeline tests passed\n";
    return 0;
}
