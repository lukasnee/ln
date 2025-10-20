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
    RepeatCommandThread(CLI &cli, Clock::duration period, const Cmd &cmd, std::size_t argc, const char **argv,
                        std::size_t cmd_arg_offset)
        : Task(tskIDLE_PRIORITY + 1, 1000, "repeat"), cli(cli), period(period), cmd(cmd),
          cmd_arg_offset(cmd_arg_offset), args(this->arg_buf, argc, argv) {}

private:
    void taskFunction() override {
        while (true) {
            std::array<char, 256> array;
            this->args.print_to(array.data(), array.size(), " ");
            this->cli.printf("repeating command \'%s\' every %lu ms\n\n", array.data(),
                             std::chrono::duration_cast<std::chrono::milliseconds>(this->period).count());
            this->cli.execute(this->cmd, this->args.get_argc() - this->cmd_arg_offset,
                              this->args.get_argv() + this->cmd_arg_offset, "\e[36m");
            this->delayUntil(this->period);
        }
    }

    CLI &cli;
    Clock::duration period;
    const Cmd &cmd;
    std::size_t cmd_arg_offset;
    Args args;
    std::array<char, 256> arg_buf;
};

Cmd repeat("repeat,r", "<period_ms> <COMMAND...>", "repeat command at a given period", [](Cmd::Ctx ctx) -> Err {
    static bool is_started = false;
    static RepeatCommandThread *thread = nullptr;

    if (ctx.argc > 2) {
        constexpr std::size_t arg_offset = 2;
        auto [cmd, _] = ctx.cli.find_cmd(ctx.argc - arg_offset, ctx.argv + arg_offset);

        if (!cmd) {
            return Err::fail;
        }
        if (is_started) {
            return Err::fail3;
        }
        void *threadMemory = pvPortMalloc(sizeof(RepeatCommandThread));
        if (!threadMemory) {
            return Err::fail5;
        }
        thread = new (threadMemory)
            RepeatCommandThread(ctx.cli, std::chrono::milliseconds(std::strtoul(ctx.argv[1], nullptr, 10)), *cmd,
                                ctx.argc - arg_offset, ctx.argv + arg_offset, arg_offset);
        is_started = true;
        if (!is_started) {
            return Err::fail4;
        }
        return Err::ok;
    }
    else if (ctx.argc == 1 && is_started) {
        thread->~RepeatCommandThread();
        vPortFree(thread);
        is_started = false;
        ctx.cli.printf("repeat thread stopped\n");
        return Err::ok;
    }

    return Err::badArg;
});

} // namespace ln::shell
