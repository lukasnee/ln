#include "ln/shell/CLI.hpp"

namespace ln::shell {

void hexdump(CLI &cli, const std::uint32_t &address, const std::size_t &size) {
    unsigned char *buf = reinterpret_cast<unsigned char *>(address);
    int buflen = static_cast<int>(size);
    int i, j;
    for (i = 0; i < buflen; i += 16) {
        cli.printf("%08x: ", address + i);
        for (j = 0; j < 16; j++)
            if (i + j < buflen)
                cli.printf("%02x ", buf[i + j]);
            else
                cli.print("   ");
        cli.print(' ');
        for (j = 0; j < 16; j++)
            if (i + j < buflen)
                cli.print(isprint(buf[i + j]) ? buf[i + j] : '.');
        cli.print('\n');
    }
}

Cmd hexdumpShellCommand("hexdump,hd", "<addressHex> <sizeHex>", "hex dump", [](Cmd::Ctx ctx) -> Err {
    if (ctx.argc != 3) {
        return Err::fail;
    }
    unsigned int address;
    unsigned int size;
    if (!std::sscanf(ctx.argv[1], "%x", &address) || !std::sscanf(ctx.argv[2], "%x", &size)) {
        return Err::fail;
    }
    hexdump(ctx.cli, address, size);
    return Err::okQuiet;
});

} // namespace ln::shell
