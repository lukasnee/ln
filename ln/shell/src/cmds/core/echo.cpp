#include "ln/shell/CLI.hpp"

namespace ln::shell {

Cmd echo("echo", "echos typed content", [](Cmd::Ctx ctx) -> Err {
    if (ctx.argc <= 1) {
        return Err::fail;
    }
    ArgVector argVector(ctx.argc - 1, ctx.argv + 1);
    std::array<char, ArgBuffer::Config::bufferSize> echoBuffer;
    argVector.printTo(echoBuffer.data(), echoBuffer.size(), " ", false);
    ctx.cli.printf("%s\n", echoBuffer.data());
    return Err::okQuiet;
});

} // namespace ln::shell
