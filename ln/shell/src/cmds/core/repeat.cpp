/*
 * Copyright (c) 2025 Lukas Neverauskis https://github.com/lukasnee
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ln/shell/CLI.hpp"

// #include "system.hpp"

#include "FreeRTOS/Task.hpp"
#include "FreeRTOS/Addons/Clock.hpp"

#include <array>
#include <cstdint>

namespace ln::shell {

// TODO: consider basing on Timer instead of Task

class RepeatCommandThread : public FreeRTOS::Task {
public:
    using Clock = FreeRTOS::Addons::Clock;
    RepeatCommandThread(CLI &cli, Clock::duration period, const Cmd &cmd, const std::string_view args_str)
        : Task{tskIDLE_PRIORITY + 1, 1000, "repeat"}, cli{cli}, period{period}, cmd{cmd} {
        if (args_str.size() >= args_str_buf.size()) {
            this->cli.printf("error: arguments too long for repeat command\n");
            return;
        }
        this->args_str = std::string_view{this->args_str_buf.data(),
                                          args_str.copy(this->args_str_buf.data(), this->args_str_buf.size())};
    }

private:
    void taskFunction() override {
        std::array<std::string_view, 16> args_buf;
        auto opt_args = Args::tokenize(this->args_str, args_buf);
        if (!opt_args) {
            this->cli.printf("error: too many arguments for repeat command\n");
            return;
        }
        const auto args = *opt_args;
        while (true) {
            this->cli.execute(cmd, args, "\e[36m");
            this->delayUntil(this->period);
        }
    }

    CLI &cli;
    Clock::duration period;
    const Cmd &cmd;
    std::array<char, 256> args_str_buf;
    std::string_view args_str{};
    // std::span<std::string_view> args;
};

// TODO: implement quotes for allowing multiple COMMAND arguments with spaces etc.
Cmd repeat("repeat,r", "<period_ms> <COMMAND...>", "repeat command at a given period", [](Cmd::Ctx ctx) -> Err {
    static RepeatCommandThread *thread = nullptr;
    if (ctx.args.size() == 0 && thread) {
        thread->~RepeatCommandThread();
        vPortFree(thread);
        thread = nullptr;
        ctx.cli.printf("repeat thread stopped\n");
        return Err::ok;
    }
    else if (ctx.args.size() > 1) {
        if (thread) {
            return Err::fail;
        }
        auto cmd_with_args = ctx.args.subspan(1);
        auto [cmd_ptr, cmd_args] = ctx.cli.find_cmd(cmd_with_args);
        if (!cmd_ptr) {
            ctx.cli.printf("error: could not find command: \'%.*s\'\n",
                           cmd_with_args.back().cend() - cmd_with_args.front().cbegin(), cmd_with_args.front().data());
            return Err::fail;
        }
        void *thread_obj_mem = pvPortMalloc(sizeof(RepeatCommandThread));
        if (!thread_obj_mem) {
            ctx.cli.printf("error: could not allocate memory for repeat thread\n");
            return Err::fail;
        }
        const auto repeat_period = std::chrono::milliseconds(std::strtoul(ctx.args[0].data(), nullptr, 10));
        ctx.cli.printf("repeating \'%.*s\' every %lu ms\n\n",
                       cmd_with_args.back().cend() - cmd_with_args.front().cbegin(), cmd_with_args.front().data(),
                       repeat_period.count());
        const std::string_view cmd_args_str = std::string_view{cmd_args.front().data(), cmd_args.back().cend()};
        thread = new (thread_obj_mem) RepeatCommandThread(ctx.cli, repeat_period, *cmd_ptr, cmd_args_str);
        return Err::ok;
    }
    return Err::badArg;
});

} // namespace ln::shell
