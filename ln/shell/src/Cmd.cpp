#include "ln/shell/Cmd.hpp"
#include "ln/shell/CLI.hpp"

// TODO: make arrow up repeat buffer
#include <cstring>

namespace ln::shell {

Cmd *Cmd::globalCommandList = nullptr;

void Cmd::linkTo(Cmd *&pParent) {
    if (!pParent) {
        pParent = this;
    }
    else {
        Cmd *pNext = pParent;
        while (pNext->pNext) {
            pNext = pNext->pNext;
        }
        pNext->pNext = this;
    }
}

Cmd::Cmd(const char *name, const char *usage, const char *description, Cmd::Function function,
         std::function<void()> ctorCallback)
    : name(name), usage(usage), description(description), function(function) {
    this->linkTo(Cmd::globalCommandList);
    if (ctorCallback) {
        ctorCallback();
    }
}

Cmd::Cmd(Cmd &parent, const char *name, const char *usage, const char *description, Cmd::Function function,
         std::function<void()> ctorCallback)
    : name(name), usage(usage), description(description), function(function) {
    this->linkTo(parent.pSubcommands);
    if (ctorCallback) {
        ctorCallback();
    }
}

Cmd::Cmd(const char *name, Cmd::Function function)
    : name(name), usage(nullptr), description(nullptr), function(function) {
    this->linkTo(Cmd::globalCommandList);
}

bool Cmd::matchToken(const char *strTokens, const char *strToken) {
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

const Cmd *Cmd::findNeighbourCommand(const char *name) const {
    const Cmd *result = nullptr;

    for (const Cmd *pNext = this; pNext != nullptr; pNext = pNext->pNext) {
        if (Cmd::matchToken(pNext->name, name)) {
            result = pNext;
            break;
        }
    }

    return result;
}

const Cmd *Cmd::findSubcommand(const char *name) const {
    const Cmd *result = nullptr;

    for (const Cmd *pNext = this->pSubcommands; pNext != nullptr; pNext = pNext->pNext) {
        if (Cmd::matchToken(pNext->name, name)) {
            result = pNext;
            break;
        }
    }

    return result;
}

Err Cmd::print_help(CLI &cli, bool recurse, const std::size_t maxDepth, std::size_t depth, std::size_t indent) const {
    Err result = Err::ok;

    constexpr int commandColumnWidth = 40;

    for (const Cmd *pCmdIt = this; pCmdIt != nullptr; pCmdIt = pCmdIt->pNext) {
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
                result = Err::ok;
            }
        }

        if (result == Err::ok && recurse && depth < maxDepth && pCmdIt->pSubcommands) {
            result = pCmdIt->pSubcommands->print_help(cli, recurse, maxDepth, depth + 1,
                                                      indent + strlen(pCmdIt->name) + sizeof(' '));
        }

        if (depth == 0) {
            break;
        }
    }
    return result;
}

Cmd Cmd::help_cmd = Cmd("help,?", "[all|[COMMAND...]]", "show command usage", [](Ctx ctx) -> Err {
    if (ctx.argc == 1) {
        for (const Cmd *pCmdIt = Cmd::globalCommandList; pCmdIt; pCmdIt = pCmdIt->pNext) {
            const auto res = pCmdIt->print_help(ctx.cli, false, 0);
            if (res != Err::ok) {
                return res;
            }
        }
    }
    else if (ctx.argc == 2 && !std::strcmp(ctx.argv[1], "all")) {
        for (const Cmd *pCmdIt = Cmd::globalCommandList; pCmdIt; pCmdIt = pCmdIt->pNext) {
            const auto res = pCmdIt->print_help(ctx.cli, true, 7);
            if (res != Err::ok) {
                return res;
            }
        }
    }
    else if (ctx.argc > 1) {
        constexpr std::size_t helpCommandOffset = 1;
        std::size_t argOffset;
        const Cmd *pCommandFound =
            ctx.cli.findCommand(ctx.argc - helpCommandOffset, ctx.argv + helpCommandOffset, argOffset);
        if (pCommandFound) {
            const auto res = pCommandFound->print_help(ctx.cli, true, 1);
            if (res != Err::ok) {
                return res;
            }
        }
    }
    else {
        return Err::badArg;
    }
    return Err::ok;
});

} // namespace ln::shell
