

#include "ln/shell/generic/cmds.hpp"

namespace ln::shell::generic::cmds {

Err on_off_command_parser(std::function<bool(bool)> onOffF, const char *strOnOffControlName, Cmd::Ctx ctx) {
    Err result = Err::fail;

    if (ctx.argc != 2) {
        ctx.cli.print("no arg\n");
    }
    else if (!std::strcmp(ctx.argv[1], "on") || !std::strcmp(ctx.argv[1], "1") || !std::strcmp(ctx.argv[1], "true")) {
        if (onOffF(true)) {
            ctx.cli.printf("%s %s %s\n", strOnOffControlName, "turned", "on");
            result = Err::ok;
        }
    }
    else if (!std::strcmp(ctx.argv[1], "off") || !std::strcmp(ctx.argv[1], "0") || !std::strcmp(ctx.argv[1], "false")) {
        if (onOffF(false)) {
            ctx.cli.printf("%s %s %s\n", strOnOffControlName, "turned", "off");
            result = Err::ok;
        }
    }
    else {
        ctx.cli.print("bad arg\n");
    }

    return result;
};

Err on_off_command_parser(bool &onOffControl, const char *strOnOffControlName, Cmd::Ctx ctx) {
    return on_off_command_parser(
        [&](bool state) {
            onOffControl = state;
            return true;
        },
        strOnOffControlName, ctx);
};

} // namespace ln::shell::generic::cmds