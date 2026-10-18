// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ln/shell/CLI.hpp"

namespace ln::shell {

class CLId : public ln::Thread {
public:
    CLId(CLI &cli, const ln::Thread::Cfg &cfg) : ln::Thread(cfg), cli(cli) {}

    void start() { this->ln::Thread::start(); }

private:
    void routine() final { this->cli.routine(); }
    CLI &cli;
};

} // namespace ln::shell
