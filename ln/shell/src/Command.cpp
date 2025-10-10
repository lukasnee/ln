#include "ln/shell/Command.hpp"

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

} // namespace ln::shell
