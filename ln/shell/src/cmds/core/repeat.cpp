// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/shell/CLI.hpp"

#include "ln/Clock.hpp"

// #include "system.hpp"

#include <array>
#include <cstdint>

namespace ln::shell {

// TODO: consider basing on Timer instead of Task

class RepeatCommandThread : public ln::Task {
public:
    using Clock = ln::Clock;
    RepeatCommandThread(CLI &cli, Clock::duration period,
                        const std::string_view line)
        : Task{tskIDLE_PRIORITY + 1, 1000, "repeat"}, cli{cli}, period{period} {
        if (line.size() >= line_buf.size()) {
            this->cli.print("error: arguments too long for repeat command\n");
            return;
        }
        this->line = std::string_view{
            this->line_buf.data(),
            line.copy(this->line_buf.data(), this->line_buf.size())};
    }

private:
    void routine() final {
        const auto start_time = Clock::now();
        // TODO: could be optimized, prasing args only once
        this->cli.execute_line(this->line);
        ln::sleep_until(start_time + this->period);
    }

    CLI &cli;
    Clock::duration period;
    std::array<char, 256> line_buf;
    std::string_view line{};
};

// TODO: implement quotes for allowing multiple COMMAND arguments with spaces
// etc.
Cmd repeat{Cmd::Cfg{
    .cmd_list = Cmd::get_general_cmd_list(),
    .name = "repeat,r",
    .usage = "<period_ms:u32> <expr:str>",
    .short_description = "repeat command at a given period",
    .fn = [](Cmd::Ctx ctx) {
        static RepeatCommandThread *thread = nullptr;
        if (ctx.args.size() == 0 && thread) {
            thread->~RepeatCommandThread();
            vPortFree(thread);
            thread = nullptr;
            ctx.cli.print("repeat thread stopped\n");
            return Err::ok;
        }
        else if (ctx.args.size() == 2) {
            if (thread) {
                return Err::fail;
            }
            auto expr_sv = ctx.args[1];
            void *thread_obj_mem = pvPortMalloc(sizeof(RepeatCommandThread));
            if (!thread_obj_mem) {
                ctx.cli.print(
                    "error: could not allocate memory for repeat thread\n");
                return Err::fail;
            }
            const auto repeat_period = std::chrono::milliseconds(
                std::strtoul(ctx.args[0].data(), nullptr, 10));
            ctx.cli.print("repeating \'{}\' every {}\n\n", expr_sv,
                          repeat_period);
            thread = new (thread_obj_mem)
                RepeatCommandThread(ctx.cli, repeat_period, expr_sv);
            return Err::ok;
        }
        return Err::badArg;
    }}};

} // namespace ln::shell
