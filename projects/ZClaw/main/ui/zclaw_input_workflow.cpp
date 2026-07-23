#include "zclaw_input_workflow.h"

#include "zclaw_chat_workflow.h"
#include "zclaw_input_dialog.h"
#include "zclaw_input_model.h"
#include "zclaw_settings_workflow.h"

namespace zclaw {

InputWorkflow::InputWorkflow(InputDialog &input, SettingsWorkflow &settings,
                             ChatWorkflow &chat)
    : input_(input), settings_(settings), chat_(chat)
{
}

void InputWorkflow::submit_current()
{
    if (!input_.is_open())
        return;

    const InputSubmission submission = input_submission(input_.mode(), input_.text());
    input_.close();
    switch (submission.action) {
    case InputSubmissionAction::None:
        break;
    case InputSubmissionAction::SendChat:
        chat_.submit(submission.value);
        break;
    case InputSubmissionAction::ApplySetupEdit:
        settings_.apply_setup_edit(submission.value);
        break;
    case InputSubmissionAction::ApplyProviderEdit:
        settings_.apply_provider_edit(submission.value);
        break;
    case InputSubmissionAction::StartPairing:
        settings_.start_pairing(submission.value);
        break;
    }
}

}  // namespace zclaw
