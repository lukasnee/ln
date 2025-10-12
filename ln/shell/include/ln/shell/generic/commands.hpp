#include "ln/shell/CLI.hpp"

namespace ln::shell::generic::commands {

static constexpr const char *on_off_command_usage = "<on|off>";

Result on_off_command_parser(std::function<bool(bool)> onOffF, const char *strOnOffControlName, Command::Context ctx);
Result on_off_command_parser(bool &onOffControl, const char *strOnOffControlName, Command::Context ctx);

} // namespace ln::shell::generic::commands