#pragma once

#include <functional>
#include <limits>
#include <cstdint>

namespace ln::shell {

class Shell;

class Command {
public:
    enum Result : std::int8_t {
        unknown = std::numeric_limits<std::int8_t>::min(),
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

#define ShellCommandFunctionParams                                                                                     \
    [[maybe_unused]] ln::shell::Shell &shell, [[maybe_unused]] std::size_t argc, [[maybe_unused]] const char *argv[]
#define ShellCommandFunctionArgs shell, argc, argv
    using Function = std::function<Result(ShellCommandFunctionParams)>;
    using CtorCallback = std::function<void()>;
#define ShellCommandFunctionLambdaSignature (ShellCommandFunctionParams)->ln::shell::Command::Result

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

protected:
    void linkTo(Command *&pParent);

    Command *pSubcommands = nullptr;
    Command *pNext = nullptr;


private:
    friend Shell;
    static Command *globalCommandList;
};

} // namespace ln::shell
