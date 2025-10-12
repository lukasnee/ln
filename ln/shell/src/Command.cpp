#include "ln/shell/Command.hpp"
#include "ln/shell/CLI.hpp"

// TODO: make arrow up repeat buffer
#include <cstring>

namespace ln::shell {

Command *Command::globalCommandList = nullptr;

void Command::linkTo(Command *&pParent) {
    if (!pParent) {
        pParent = this;
    }
    else {
        Command *pNext = pParent;
        while (pNext->pNext) {
            pNext = pNext->pNext;
        }
        pNext->pNext = this;
    }
}

Command::Command(const char *name, const char *usage, const char *description, Command::Function function,
                 std::function<void()> ctorCallback)
    : name(name), usage(usage), description(description), function(function) {
    this->linkTo(Command::globalCommandList);
    if (ctorCallback) {
        ctorCallback();
    }
}

Command::Command(Command &parent, const char *name, const char *usage, const char *description,
                 Command::Function function, std::function<void()> ctorCallback)
    : name(name), usage(usage), description(description), function(function) {
    this->linkTo(parent.pSubcommands);
    if (ctorCallback) {
        ctorCallback();
    }
}

Command::Command(const char *name, Command::Function function)
    : name(name), usage(nullptr), description(nullptr), function(function) {
    this->linkTo(Command::globalCommandList);
}

bool Command::matchToken(const char *strTokens, const char *strToken) {
    bool result = false;

    const std::size_t strTokenLength = std::strlen(strToken);
    const char *strThisToken = strTokens;

    for (const char *strCharIt = strThisToken; *strCharIt != '\0'; strCharIt++) {
        const bool itAtLastChar = (*(strCharIt + 1) == '\0');
        if (*strCharIt == ',' || itAtLastChar) {
            const std::size_t thisTokenLength = strCharIt + (itAtLastChar ? 1 : 0) - strThisToken;
            if (strTokenLength == thisTokenLength && 0 == std::strncmp(strToken, strThisToken, thisTokenLength)) {
                result = true;
                break;
            }
            else if (*strCharIt == ',') {
                strThisToken = strCharIt + 1;
            }
        }
    }

    return result;
}

const Command *Command::findNeighbourCommand(const char *name) const {
    const Command *result = nullptr;

    for (const Command *pNext = this; pNext != nullptr; pNext = pNext->pNext) {
        if (Command::matchToken(pNext->name, name)) {
            result = pNext;
            break;
        }
    }

    return result;
}

const Command *Command::findSubcommand(const char *name) const {
    const Command *result = nullptr;

    for (const Command *pNext = this->pSubcommands; pNext != nullptr; pNext = pNext->pNext) {
        if (Command::matchToken(pNext->name, name)) {
            result = pNext;
            break;
        }
    }

    return result;
}

Result Command::print_help(CLI &cli, bool recurse, const std::size_t maxDepth, std::size_t depth,
                           std::size_t indent) const {
    Result result = Result::ok;

    constexpr int commandColumnWidth = 40;

    for (const Command *pCmdIt = this; pCmdIt != nullptr; pCmdIt = pCmdIt->pNext) {
        if (indent >= 3) {
            cli.print(' ', indent - 3);
            // cli.print("|\n");
            // cli.print(' ', indent - 3);
            cli.print("`- ");
        }

        int charsPrinted = 0;

        if (pCmdIt->name) {
            if (pCmdIt->usage) {
                charsPrinted = cli.printf("%s %s ", pCmdIt->name, pCmdIt->usage);
            }
            else {
                charsPrinted = cli.printf("%s  ", pCmdIt->name);
            }
        }

        if (charsPrinted > 0) {
            if (charsPrinted < commandColumnWidth) {
                cli.print(' ', commandColumnWidth - charsPrinted - indent);
            }

            if (pCmdIt->description) {
                charsPrinted = cli.print(pCmdIt->description);
            }

            cli.print('\n');

            if (charsPrinted >= 0) {
                result = Result::ok;
            }
        }

        if (result == Result::ok && recurse && depth < maxDepth && pCmdIt->pSubcommands) {
            result = pCmdIt->pSubcommands->print_help(cli, recurse, maxDepth, depth + 1,
                                                      indent + strlen(pCmdIt->name) + sizeof(' '));
        }

        if (depth == 0) {
            break;
        }
    }
    return result;
}

Command Command::help_cmd = Command("help,?", "[all|[COMMAND...]]", "show command usage", [](Context ctx) -> Result {
    if (ctx.argc == 1) {
        for (const Command *pCmdIt = Command::globalCommandList; pCmdIt; pCmdIt = pCmdIt->pNext) {
            const auto res = pCmdIt->print_help(ctx.cli, false, 0);
            if (res != Result::ok) {
                return res;
            }
        }
    }
    else if (ctx.argc == 2 && !std::strcmp(ctx.argv[1], "all")) {
        for (const Command *pCmdIt = Command::globalCommandList; pCmdIt; pCmdIt = pCmdIt->pNext) {
            const auto res = pCmdIt->print_help(ctx.cli, true, 7);
            if (res != Result::ok) {
                return res;
            }
        }
    }
    else if (ctx.argc > 1) {
        constexpr std::size_t helpCommandOffset = 1;
        std::size_t argOffset;
        const Command *pCommandFound =
            ctx.cli.findCommand(ctx.argc - helpCommandOffset, ctx.argv + helpCommandOffset, argOffset);
        if (pCommandFound) {
            const auto res = pCommandFound->print_help(ctx.cli, true, 1);
            if (res != Result::ok) {
                return res;
            }
        }
    }
    else {
        return Result::badArg;
    }
    return Result::ok;
});

} // namespace ln::shell
