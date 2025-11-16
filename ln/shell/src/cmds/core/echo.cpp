#include "ln/shell/CLI.hpp"

namespace ln::shell {

Cmd echo("echo", "echos typed content", [](Cmd::Ctx ctx) -> Err {
    if (ctx.args.size() == 0) {
        ctx.cli.print('\n');
        return Err::okQuiet;
    }
    ctx.cli.printf("%.*s\n", ctx.args.back().cend() - ctx.args.front().cbegin(), ctx.args.front().data());
    return Err::okQuiet;
});

} // namespace ln::shell
