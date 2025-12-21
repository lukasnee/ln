/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

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

static constexpr std::array<Arg, 2> cmd_hexdump_positional_args{{
    Arg{.name = "address", .type = Arg::Type::num, .description = "Starting address"},
    Arg{.name = "size", .type = Arg::Type::num, .description = "Size to print in bytes"},
}};

Cmd cmd_hexdump{Cmd::Cfg{.cmd_list = Cmd::general_cmd_list,
                         .name = "hexdump,hd",
                         .argp_cfg = ArgParser::Cfg{.positional_args = cmd_hexdump_positional_args},
                         .short_description = "hex dump",
                         .fn = [](Cmd::Ctx ctx) {
                             if (ctx.args.size() != 2) {
                                 return Err::fail;
                             }
                             auto opt_address = ctx.argp.get_positional(0).as_u32();
                             if (!opt_address.has_value()) {
                                 return Err::badArg;
                             }
                             auto address = *opt_address;
                             auto opt_size = ctx.argp.get_positional(1).as_u32();
                             if (!opt_size.has_value()) {
                                 return Err::badArg;
                             }
                             auto size = *opt_size;
                             hexdump(ctx.cli, address, size);
                             return Err::ok;
                         }}};

} // namespace ln::shell
