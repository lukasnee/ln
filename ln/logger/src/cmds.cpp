// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/logger/logger.h"
#include "ln/shell/CLI.hpp"

#include "ln/shell/generic/cmds.hpp"

LN_LOG_MODULE(cmd_log, ln::logger::Level::notset);

namespace ln::shell {

static Cmd cmd_log{
    Cmd::Cfg{.name = "log",
             .usage = generic::cmds::on_off_command_usage,
             .short_description = "enable or disable logging",
             .fn = [](Cmd::Ctx ctx) {
                 auto config = ln::logger::Logger::get_instance().get_config();
                 auto result = generic::cmds::on_off_command_parser(
                     config.enabled_run_time, "logging", ctx);
                 if (result != Err::ok) {
                     return result;
                 }
                 if (!ln::logger::Logger::get_instance().set_config(config)) {
                     ctx.cli.print("failed to set logger config\n");
                     return Err::fail;
                 }
                 return Err::ok;
             }}};

Cmd cmd_log_inf{Cmd::Cfg{.parent_cmd = &cmd_log,
                         .name = "inf",
                         .usage = "<msg:str>",
                         .short_description = "log info message",
                         .fn = [](Cmd::Ctx ctx) {
                             LN_LOG_INF("{}", ctx.args[0]);
                             return Err::ok;
                         }}};

Cmd cmd_log_wrn{Cmd::Cfg{.parent_cmd = &cmd_log,
                         .name = "wrn",
                         .usage = "<msg:str>",
                         .short_description = "log warning message",
                         .fn = [](Cmd::Ctx ctx) {
                             LN_LOG_WRN("{}", ctx.args[0]);
                             return Err::ok;
                         }}};

Cmd cmd_log_err{Cmd::Cfg{.parent_cmd = &cmd_log,
                         .name = "err",
                         .usage = "<msg:str>",
                         .short_description = "log error message",
                         .fn = [](Cmd::Ctx ctx) {
                             LN_LOG_ERR("{}", ctx.args[0]);
                             return Err::ok;
                         }}};

// TODO: this is ROM inefficient and weird - improve
#define LOG_CONFIG_BOOL_CMD(cmd_name, config_field, description)               \
    Cmd cmd_log_##cmd_name {                                                   \
        Cmd::Cfg {                                                             \
            .parent_cmd = &cmd_log, .name = #cmd_name,                         \
            .usage = generic::cmds::on_off_command_usage,                      \
            .fn = [](Cmd::Ctx ctx) {                                           \
                auto config = ln::logger::Logger::get_instance().get_config(); \
                if (ctx.args.empty()) {                                        \
                    ctx.cli.print(config.config_field ? "1" : "0");            \
                    ctx.cli.print('\n');                                       \
                    return Err::ok;                                            \
                }                                                              \
                auto result = generic::cmds::on_off_command_parser(            \
                    config.config_field, #description, ctx);                   \
                if (result != Err::ok) {                                       \
                    return result;                                             \
                }                                                              \
                if (!ln::logger::Logger::get_instance().set_config(config)) {  \
                    ctx.cli.print("failed to set logger config\n");            \
                    return Err::fail;                                          \
                }                                                              \
                return Err::ok;                                                \
            }                                                                  \
        }                                                                      \
    }
static LOG_CONFIG_BOOL_CMD(color, color, log coloring);
static LOG_CONFIG_BOOL_CMD(prefix, print_header_enabled, log prefix);
#undef LOG_CONFIG_BOOL_CMD
} // namespace ln::shell
