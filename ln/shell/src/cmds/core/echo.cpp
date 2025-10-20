#include "ln/shell/CLI.hpp"

namespace ln::shell {

Cmd echo("echo", "echos typed content", [](Cmd::Ctx ctx) -> Err {
    if (ctx.argc <= 1) {
        return Err::fail;
    }
    std::array<char, 256> args_buf;
    Args args(args_buf, ctx.argc - 1, ctx.argv + 1);
    std::array<char, 256> echo_buf;
    args.print_to(echo_buf.data(), echo_buf.size(), " ", false);
    ctx.cli.printf("%s\n", echo_buf.data());
    return Err::okQuiet;
});

} // namespace ln::shell
