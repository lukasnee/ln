#pragma once

#include <functional>
#include <limits>
#include <cstdint>

namespace ln::shell {

enum Result : std::int8_t {
    unknown = std::numeric_limits<std::int8_t>::min(),
    unexpected = unknown + 1,
    fail8 = -8,
    fail7 = -7,
    fail6 = -6,
    fail5 = -5,
    fail4 = -4,
    fail3 = -3,
    badArg = -2,
    fail = -1,
    ok = 0,
    okQuiet,
};

class CLI;

class Command {
public:
    struct Context {
        CLI &cli;
        std::size_t argc;
        const char **argv;
    };

    using Function = std::function<Result(Context)>;
    using CtorCallback = std::function<void()>;

    Command(const char *name, const char *usage, const char *description, Function function,
            CtorCallback ctorCallback = nullptr);

    Command(Command &parent, const char *name, const char *usage, const char *description, Function function,
            CtorCallback ctorCallback = nullptr);

    Command(const char *name, Function function);

    const Command *findNeighbourCommand(const char *name) const;
    const Command *findSubcommand(const char *name) const;

    static bool matchToken(const char *strTokens, const char *strToken);

    const char *name = nullptr;
    const char *usage = nullptr;
    const char *description = nullptr;
    const Function function = nullptr;

    Result print_help(CLI &cli, bool recurse, const std::size_t maxDepth = 1, std::size_t depth = 0,
                      std::size_t indent = 0) const;

protected:
    void linkTo(Command *&pParent);

    Command *pSubcommands = nullptr;
    Command *pNext = nullptr;

private:
    friend CLI;
    static Command *globalCommandList;

    static Command help_cmd;
};

} // namespace ln::shell
