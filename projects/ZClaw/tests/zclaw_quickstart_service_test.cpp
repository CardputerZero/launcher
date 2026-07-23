#include "zclaw_quickstart_backend.h"
#include "zclaw_quickstart_service.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeQuickstartBackend : public zclaw::QuickstartBackend {
public:
    zclaw::PreparedSetup prepared;
    zclaw::OperationResult paired;
    mutable std::vector<std::string> calls;
    mutable std::string received_code;

    zclaw::PreparedSetup prepare(
        zclaw::UiConfig, zclaw::ProviderConfig,
        const ProgressHandler &progress_handler) const override
    {
        calls.push_back("prepare");
        if (progress_handler)
            progress_handler({"Prepared", 94});
        return prepared;
    }

    zclaw::OperationResult pair(zclaw::UiConfig,
                                const std::string &pairing_code) const override
    {
        calls.push_back("pair");
        received_code = pairing_code;
        return paired;
    }
};

}  // namespace

int main()
{
    FakeQuickstartBackend backend;
    backend.prepared.ok = true;
    backend.prepared.pairing_code = "123456";
    backend.prepared.config.agent_alias = "prepared-agent";
    backend.paired.ok = true;
    backend.paired.config = backend.prepared.config;
    backend.paired.config.bearer_token = "token";

    std::vector<int> progress;
    const zclaw::OperationResult result = zclaw::QuickstartService(backend).run(
        {}, {}, [&progress](const zclaw::SetupProgress &update) {
            progress.push_back(update.percent);
        });
    assert(result.ok);
    assert(result.config.setup_complete);
    assert(result.config.bearer_token == "token");
    assert(result.text == "Quickstart complete.\nChat and approvals use WS.");
    assert(backend.received_code == "123456");
    assert((backend.calls == std::vector<std::string>{"prepare", "pair"}));
    assert((progress == std::vector<int>{94, 100}));

    FakeQuickstartBackend prepare_failure;
    prepare_failure.prepared.error = "prepare failed";
    const zclaw::OperationResult failed_prepare =
        zclaw::QuickstartService(prepare_failure).run({}, {}, {});
    assert(!failed_prepare.ok);
    assert(failed_prepare.text == "prepare failed");
    assert((prepare_failure.calls == std::vector<std::string>{"prepare"}));

    FakeQuickstartBackend failed_pairing;
    failed_pairing.prepared.ok = true;
    failed_pairing.prepared.pairing_code = "654321";
    failed_pairing.paired.ok = false;
    failed_pairing.paired.config.bearer_token = "stale-token";
    failed_pairing.paired.text = "pair rejected";
    const zclaw::OperationResult rejected =
        zclaw::QuickstartService(failed_pairing).run({}, {}, {});
    assert(!rejected.ok);
    assert(!rejected.config.setup_complete);
    assert(rejected.text == "Automatic pairing failed.\npair rejected");

    FakeQuickstartBackend missing_token;
    missing_token.prepared.ok = true;
    missing_token.paired.ok = true;
    const zclaw::OperationResult tokenless =
        zclaw::QuickstartService(missing_token).run({}, {}, {});
    assert(!tokenless.ok);
    assert(!tokenless.config.setup_complete);
    return 0;
}
