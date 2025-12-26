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

void hexdump(CLI &cli, const uint32_t &address, const size_t &size) {
    const size_t bytes_per_line = 16;
    uint8_t *buf = reinterpret_cast<uint8_t *>(address);
    for (size_t i = 0; i < size; i += bytes_per_line) {
        cli.printf("%08x: ", address + i);
        for (size_t j = 0; j < bytes_per_line; j++) {
            if (i + j < size) {
                cli.printf("%02x ", buf[i + j]);
            }
            else {
                cli.print("   ");
            }
        }
        cli.print(' ');
        for (size_t j = 0; j < bytes_per_line; j++) {
            if (i + j < size) {
                cli.print(isprint(buf[i + j]) ? buf[i + j] : '.');
            }
        }
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
