#include "zclaw_binary_installer.h"
#include "zclaw_binary_installer_backend.h"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

namespace {

class FakeBackend final : public zclaw::BinaryInstallerBackend {
public:
    bool prepare_ok = true;
    bool ready = false;
    bool install_ok = true;
    mutable int prepare_calls = 0;
    mutable int ready_calls = 0;
    mutable int install_calls = 0;

    bool prepare_destination(std::string *error) const override
    {
        ++prepare_calls;
        if (!prepare_ok && error)
            *error = "prepare failed";
        return prepare_ok;
    }

    bool binary_ready() const override
    {
        ++ready_calls;
        return ready;
    }

    bool install(std::string *error,
                 const ProgressHandler &progress_handler) const override
    {
        ++install_calls;
        if (progress_handler) {
            progress_handler({zclaw::BinaryInstallPhase::Downloading,
                              25, 100, 2048.0,
                              "https://example.test/zeroclaw.tar.gz",
                              "/tmp/install/zeroclaw.tar.gz"});
            progress_handler({zclaw::BinaryInstallPhase::Downloading,
                              40, 0, 1024.0,
                              "https://example.test/zeroclaw.tar.gz",
                              "/tmp/install/zeroclaw.tar.gz"});
            progress_handler(
                {zclaw::BinaryInstallPhase::Installing, 0, 0, 0.0});
        }
        if (!install_ok && error)
            *error = "install failed";
        return install_ok;
    }
};

}  // namespace

int main()
{
    std::string error;
    std::vector<zclaw::SetupProgress> progress;

    auto backend = std::make_shared<FakeBackend>();
    backend->prepare_ok = false;
    assert(!zclaw::BinaryInstaller(backend).ensure_installed(
        &error, [&](const zclaw::SetupProgress &item) { progress.push_back(item); }));
    assert(error == "prepare failed");
    assert(backend->prepare_calls == 1);
    assert(backend->ready_calls == 0);
    assert(backend->install_calls == 0);
    assert(progress.size() == 1 && progress[0].percent == 3);

    backend = std::make_shared<FakeBackend>();
    backend->ready = true;
    progress.clear();
    assert(zclaw::BinaryInstaller(backend).ensure_installed(
        &error, [&](const zclaw::SetupProgress &item) { progress.push_back(item); }));
    assert(backend->install_calls == 0);
    assert(progress.size() == 2);
    assert(progress[1].status == "ZeroClaw is ready");
    assert(progress[1].percent == 65);

    backend = std::make_shared<FakeBackend>();
    backend->install_ok = false;
    progress.clear();
    assert(!zclaw::BinaryInstaller(backend).ensure_installed(
        &error, [&](const zclaw::SetupProgress &item) { progress.push_back(item); }));
    assert(error == "install failed");
    assert(progress.size() == 4);
    assert(progress[1].status == "Downloading ZeroClaw");
    assert(progress[1].percent == 20);
    assert(progress[1].downloaded_bytes == 25);
    assert(progress[1].total_bytes == 100);
    assert(progress[1].bytes_per_second == 2048.0);
    assert(progress[1].source_url ==
           "https://example.test/zeroclaw.tar.gz");
    assert(progress[1].destination_path ==
           "/tmp/install/zeroclaw.tar.gz");
    assert(progress[2].percent == 5);
    assert(progress[2].source_url == progress[1].source_url);
    assert(progress[2].destination_path == progress[1].destination_path);
    assert(progress[3].status == "Installing ZeroClaw");
    assert(progress[3].percent == 67);

    error.clear();
    assert(!zclaw::BinaryInstaller(nullptr).ensure_installed(&error, {}));
    assert(error == "Missing binary installer backend.");
    return 0;
}
