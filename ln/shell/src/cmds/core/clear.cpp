#include "ln/shell/CLI.hpp"

namespace ln::shell {

Cmd clear("clear,c", "clear screen", [](Cmd::Ctx ctx) -> Err {
    std::size_t i = 0x30;
    while (i--) {
        ctx.cli.print('\n');
    }
    return Err::okQuiet;
});

} // namespace ln::shell
