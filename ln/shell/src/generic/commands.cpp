

#include "ln/shell/generic/commands.hpp"

namespace ln::shell::generic::commands {

static Command::Result help(Shell &shell, const Command *pCommand, bool recurse, const std::size_t maxDepth,
                            std::size_t depth, std::size_t indent) {
    Command::Result result = Command::Result::ok;

    if (!pCommand) {
        result = Command::Result::badArg;
    }
    else {
        constexpr int commandColumnWidth = 40;

        for (const Command *pCmdIt = pCommand; pCmdIt != nullptr; pCmdIt = pCmdIt->pNext) {
            if (indent >= 3) {
                shell.print(' ', indent - 3);
                // shell.print("|\n");
                // shell.print(' ', indent - 3);
                shell.print("`- ");
            }

            int charsPrinted = 0;

            if (pCmdIt->name) {
                if (pCmdIt->usage) {
                    charsPrinted = shell.printf("%s %s ", pCmdIt->name, pCmdIt->usage);
                }
                else {
                    charsPrinted = shell.printf("%s  ", pCmdIt->name);
                }
            }

            if (charsPrinted > 0) {
                if (charsPrinted < commandColumnWidth) {
                    shell.print(' ', commandColumnWidth - charsPrinted - indent);
                }

                if (pCmdIt->description) {
                    charsPrinted = shell.print(pCmdIt->description);
                }

                shell.print('\n');

                if (charsPrinted >= 0) {
                    result = Command::Result::ok;
                }
            }

            if (result == Command::Result::ok && recurse && depth < maxDepth && pCmdIt->pSubcommands) {
                result = help(shell, pCmdIt->pSubcommands, recurse, maxDepth, depth + 1,
                              indent + strlen(pCmdIt->name) + sizeof(' '));
            }

            if (depth == 0) {
                break;
            }
        }
    }
    return result;
}

Command helpCommand =
    Command("help,?", "[all|[COMMAND...]]", "show command usage", [] ShellCommandFunctionLambdaSignature {
        Command::Result result = Command::Result::unknown;

        if (argc) {
            if (argc == 1) {
                for (const Command *pCmdIt = Command::globalCommandList; pCmdIt != nullptr; pCmdIt = pCmdIt->pNext) {
                    result = Shell::help(shell, pCmdIt, 0, false);
                }
            }
            else if (argc == 2 && !std::strcmp(argv[1], "all")) {
                for (const Command *pCmdIt = Command::globalCommandList; pCmdIt != nullptr; pCmdIt = pCmdIt->pNext) {
                    result = Shell::help(shell, pCmdIt, true, 7);
                }
            }
            else if (argc > 1) {
                constexpr std::size_t helpCommandOffset = 1;
                std::size_t argOffset;
                const Command *pCommandFound =
                    shell.findCommand(argc - helpCommandOffset, argv + helpCommandOffset, argOffset);
                if (pCommandFound) {
                    result = Shell::help(shell, pCommandFound, 1, true);
                }
            }
        }
        return result;
    });

Command::Result on_off_command_parser(std::function<bool(bool)> onOffF, const char *strOnOffControlName,
                                  ShellCommandFunctionParams) {
    Command::Result result = Command::Result::fail;

    if (argc != 2) {
        shell.print("no arg\n");
    }
    else if (!std::strcmp(argv[1], "on") || !std::strcmp(argv[1], "1") || !std::strcmp(argv[1], "true")) {
        if (onOffF(true)) {
            shell.printf("%s %s %s\n", strOnOffControlName, "turned", "on");
            result = Command::Result::ok;
        }
    }
    else if (!std::strcmp(argv[1], "off") || !std::strcmp(argv[1], "0") || !std::strcmp(argv[1], "false")) {
        if (onOffF(false)) {
            shell.printf("%s %s %s\n", strOnOffControlName, "turned", "off");
            result = Command::Result::ok;
        }
    }
    else {
        shell.print("bad arg\n");
    }

    return result;
};

Command::Result on_off_command_parser(bool &onOffControl, const char *strOnOffControlName, ShellCommandFunctionParams) {
    return on_off_command_parser(
        [&](bool state) {
            onOffControl = state;
            return true;
        },
        strOnOffControlName, ShellCommandFunctionArgs);
};

} // namespace ln::shell::generic::commands