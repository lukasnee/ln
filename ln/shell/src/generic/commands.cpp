

#include "ln/shell/generic/commands.hpp"

namespace ln::shell::generic::commands {

Result on_off_command_parser(std::function<bool(bool)> onOffF, const char *strOnOffControlName, Command::Context ctx) {
    Result result = Result::fail;

    if (ctx.argc != 2) {
        ctx.cli.print("no arg\n");
    }
    else if (!std::strcmp(ctx.argv[1], "on") || !std::strcmp(ctx.argv[1], "1") || !std::strcmp(ctx.argv[1], "true")) {
        if (onOffF(true)) {
            ctx.cli.printf("%s %s %s\n", strOnOffControlName, "turned", "on");
            result = Result::ok;
        }
    }
    else if (!std::strcmp(ctx.argv[1], "off") || !std::strcmp(ctx.argv[1], "0") || !std::strcmp(ctx.argv[1], "false")) {
        if (onOffF(false)) {
            ctx.cli.printf("%s %s %s\n", strOnOffControlName, "turned", "off");
            result = Result::ok;
        }
    }
    else {
        ctx.cli.print("bad arg\n");
    }

    return result;
};

Result on_off_command_parser(bool &onOffControl, const char *strOnOffControlName, Command::Context ctx) {
    return on_off_command_parser(
        [&](bool state) {
            onOffControl = state;
            return true;
        },
        strOnOffControlName, ctx);
};

} // namespace ln::shell::generic::commands