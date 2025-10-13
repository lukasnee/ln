#include "ln/shell/CLI.hpp"

namespace ln::shell::generic::cmds {

static constexpr const char *on_off_command_usage = "<on|off>";

Err on_off_command_parser(std::function<bool(bool)> onOffF, const char *strOnOffControlName, Cmd::Ctx ctx);
Err on_off_command_parser(bool &onOffControl, const char *strOnOffControlName, Cmd::Ctx ctx);

} // namespace ln::shell::generic::cmds