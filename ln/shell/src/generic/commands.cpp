

#include "ln/shell/generic/commands.hpp"

namespace ln::shell::generic::commands {

Result on_off_command_parser(std::function<bool(bool)> onOffF, const char *strOnOffControlName,
                                      ln::shell::CLI &cli, std::size_t argc, const char *argv[]) {
    Result result = Result::fail;

    if (argc != 2) {
        cli.print("no arg\n");
    }
    else if (!std::strcmp(argv[1], "on") || !std::strcmp(argv[1], "1") || !std::strcmp(argv[1], "true")) {
        if (onOffF(true)) {
            cli.printf("%s %s %s\n", strOnOffControlName, "turned", "on");
            result = Result::ok;
        }
    }
    else if (!std::strcmp(argv[1], "off") || !std::strcmp(argv[1], "0") || !std::strcmp(argv[1], "false")) {
        if (onOffF(false)) {
            cli.printf("%s %s %s\n", strOnOffControlName, "turned", "off");
            result = Result::ok;
        }
    }
    else {
        cli.print("bad arg\n");
    }

    return result;
};

Result on_off_command_parser(bool &onOffControl, const char *strOnOffControlName, ln::shell::CLI &cli,
                                      std::size_t argc, const char *argv[]) {
    return on_off_command_parser(
        [&](bool state) {
            onOffControl = state;
            return true;
        },
        strOnOffControlName, cli, argc, argv);
};

} // namespace ln::shell::generic::commands