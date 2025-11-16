#pragma once

#include <functional>
#include <limits>
#include <cstdint>
#include <span>
#include <string_view>

namespace ln::shell {

enum Err : std::int8_t {
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

class Cmd {
public:
    struct Ctx {
        CLI &cli;
        std::span<const std::string_view> args;
    };

    using Function = std::function<Err(Ctx)>;
    using CtorCallback = std::function<void()>;

    Cmd(const char *name, const char *usage, const char *description, Function function,
        CtorCallback ctor_cb = nullptr);
    Cmd(Cmd &parent, const char *name, const char *usage, const char *description, Function function,
        CtorCallback ctor_cb = nullptr);
    Cmd(const char *name, const char *description, Function function);
    Cmd(const char *name, Function function);

    const Cmd *find_neighbour_cmd(std::string_view name) const;
    const Cmd *find_subcmd(std::string_view name) const;

    static bool match_token(const char *str_tokens, std::string_view str_token);

    const char *name = nullptr;
    const char *usage = nullptr;
    const char *description = nullptr;
    const Function function = nullptr;

    void print_help(CLI &cli, bool recurse, const std::size_t max_depth = 1, std::size_t depth = 0,
                    std::size_t indent = 0) const;

protected:
    void link_to(Cmd *&parent_cmd);

    Cmd *subcmd = nullptr;
    Cmd *next = nullptr;

private:
    friend CLI;
    static Cmd *global_command_list;

    static Cmd help_cmd;
};

} // namespace ln::shell
