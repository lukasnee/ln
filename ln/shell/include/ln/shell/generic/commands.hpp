#include "ln/shell/Shell.hpp"

namespace ln::shell::generic::commands {

static constexpr const char *on_off_command_usage = "<on|off>";

Command::Result help(Shell &shell, const Command *pCommand, bool recurse = false, const std::size_t maxDepth = 1,
                     std::size_t depth = 0, std::size_t indent = 0);
Command helpCommand;

Command::Result on_off_command_parser(std::function<bool(bool)> onOffF, const char *strOnOffControlName,
                               ShellCommandFunctionParams);
Command::Result on_off_command_parser(bool &onOffControl, const char *strOnOffControlName, ShellCommandFunctionParams);

} // namespace ln::shell::generic::commands