#include "zclaw_input_model.h"

#include <utility>

namespace zclaw {

InputSubmission input_submission(InputMode mode, std::string value)
{
    InputSubmission submission;
    submission.value = std::move(value);
    switch (mode) {
    case InputMode::Chat:
        submission.action = submission.value.empty() ? InputSubmissionAction::None
                                                     : InputSubmissionAction::SendChat;
        break;
    case InputMode::SetupEdit:
        submission.action = InputSubmissionAction::ApplySetupEdit;
        break;
    case InputMode::ProviderEdit:
        submission.action = InputSubmissionAction::ApplyProviderEdit;
        break;
    case InputMode::PairingCode:
        submission.action = submission.value.empty() ? InputSubmissionAction::None
                                                     : InputSubmissionAction::StartPairing;
        break;
    }
    return submission;
}

}  // namespace zclaw
