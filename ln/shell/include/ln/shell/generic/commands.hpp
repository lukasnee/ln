#include "ln/shell/CLI.hpp"

namespace ln::shell::generic::commands {

static constexpr const char *on_off_command_usage = "<on|off>";

Result on_off_command_parser(std::function<bool(bool)> onOffF, const char *strOnOffControlName,
                                      ln::shell::CLI &cli, std::size_t argc, const char *argv[]);
Result on_off_command_parser(bool &onOffControl, const char *strOnOffControlName, ln::shell::CLI &cli,
                                      std::size_t argc, const char *argv[]);

} // namespace ln::shell::generic::commands