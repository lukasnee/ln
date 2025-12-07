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
    RepeatCommandThread(CLI &cli, Clock::duration period, const std::string_view line)
        : Task{tskIDLE_PRIORITY + 1, 1000, "repeat"}, cli{cli}, period{period} {
        if (line.size() >= line_buf.size()) {
            this->cli.printf("error: arguments too long for repeat command\n");
            return;
        }
        this->line = std::string_view{this->line_buf.data(), line.copy(this->line_buf.data(), this->line_buf.size())};
    }

private:
    void taskFunction() override {
        while (true) {
            // TODO: could be optimized, prasing args only once
            this->cli.execute_line(this->line);
            this->delayUntil(this->period);
        }
    }

    CLI &cli;
    Clock::duration period;
    std::array<char, 256> line_buf;
    std::string_view line{};
};

// TODO: implement quotes for allowing multiple COMMAND arguments with spaces etc.
Cmd repeat{Cmd::Cfg{.cmd_list = Cmd::general_cmd_list,
                    .name = "repeat,r",
                    .usage = "<period_ms:u32> <expr:str>",
                    .short_description = "repeat command at a given period",
                    .fn = [](Cmd::Ctx ctx) {
                        static RepeatCommandThread *thread = nullptr;
                        if (ctx.args.size() == 0 && thread) {
                            thread->~RepeatCommandThread();
                            vPortFree(thread);
                            thread = nullptr;
                            ctx.cli.printf("repeat thread stopped\n");
                            return Err::ok;
                        }
                        else if (ctx.args.size() == 2) {
                            if (thread) {
                                return Err::fail;
                            }
                            auto expr_sv = ctx.args[1];
                            void *thread_obj_mem = pvPortMalloc(sizeof(RepeatCommandThread));
                            if (!thread_obj_mem) {
                                ctx.cli.printf("error: could not allocate memory for repeat thread\n");
                                return Err::fail;
                            }
                            const auto repeat_period =
                                std::chrono::milliseconds(std::strtoul(ctx.args[0].data(), nullptr, 10));
                            ctx.cli.printf("repeating \'%.*s\' every %lu ms\n\n", expr_sv.size(), expr_sv.data(),
                                           repeat_period.count());
                            thread = new (thread_obj_mem) RepeatCommandThread(ctx.cli, repeat_period, expr_sv);
                            return Err::ok;
                        }
                        return Err::badArg;
                    }}};

} // namespace ln::shell
