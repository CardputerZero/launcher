#include "zclaw_cli_output_model.h"

#include <cassert>

int main()
{
    using zclaw::CliServiceCommand;
    using zclaw::cli_agent_list_contains;
    using zclaw::cli_service_command_succeeded;

    assert(cli_service_command_succeeded(true, "", CliServiceCommand::Install));
    assert(cli_service_command_succeeded(true, "", CliServiceCommand::Start));
    assert(cli_service_command_succeeded(false, "already installed",
                                         CliServiceCommand::Install));
    assert(cli_service_command_succeeded(false, "service exists",
                                         CliServiceCommand::Install));
    assert(!cli_service_command_succeeded(false, "service active",
                                          CliServiceCommand::Install));
    assert(cli_service_command_succeeded(false, "already started",
                                         CliServiceCommand::Start));
    assert(cli_service_command_succeeded(false, "service active",
                                         CliServiceCommand::Start));
    assert(!cli_service_command_succeeded(false, "service exists",
                                          CliServiceCommand::Start));
    assert(!cli_service_command_succeeded(false, "Already installed",
                                          CliServiceCommand::Install));
    assert(!cli_service_command_succeeded(false, "failed",
                                          CliServiceCommand::Start));

    assert(cli_agent_list_contains("primary\nzclaw\nsecondary\n", "zclaw"));
    assert(cli_agent_list_contains("primary\n  zclaw \t\r\n", "zclaw"));
    assert(!cli_agent_list_contains("primary\nzclaw-dev\n", "zclaw"));
    assert(!cli_agent_list_contains("primary\nZClaw\n", "zclaw"));
    assert(!cli_agent_list_contains("", "zclaw"));
    return 0;
}
