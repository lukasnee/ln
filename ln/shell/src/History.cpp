// Copyright (c) 2026 Lukas Neverauskis <lukas.neverauskis@gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "ln/shell/History.hpp"
#include "ln/shell/CLI.hpp"

#include "ln/log.h"
#include <string_view>
#include <ranges>
#include <algorithm>

LN_LOG_MODULE(cli_history, ln::logger::Level::info);

namespace ln::shell {

const char entry_separator = '\0';

std::ranges::subrange<ln::RingBufferView<char>::iterator> History::get() {
    auto range =
        std::ranges::subrange{this->recall_it, this->ring_buffer.end()};
    return std::ranges::subrange{this->recall_it,
                                 std::ranges::find(range, entry_separator)};
}

void History::add(std::string_view entry) {
    if (!this->ring_buffer.push_overwrite(entry)) {
        LN_PANIC();
        return;
    }
    if (!this->ring_buffer.push_overwrite(entry_separator)) {
        LN_PANIC();
        return;
    }
    this->recall_it = this->ring_buffer.end();
}

std::ranges::subrange<ln::RingBufferView<char>::iterator> History::previous() {
    auto begin_it = this->ring_buffer.begin();
    auto end_it = this->recall_it;
    auto range = std::ranges::subrange{begin_it, end_it} | std::views::reverse;
    if (auto it = std::ranges::find(range, entry_separator);
        it != std::ranges::end(range)) {
        end_it = std::prev(it.base());
    }
    range = std::ranges::subrange{begin_it, end_it} | std::views::reverse;
    auto it = std::ranges::find(range, entry_separator);
    this->recall_it = it.base();
    return std::ranges::subrange{this->recall_it, end_it};
}

std::ranges::subrange<ln::RingBufferView<char>::iterator> History::next() {
    auto begin_it = this->recall_it;
    auto end_it = this->ring_buffer.end();
    auto range = std::ranges::subrange{begin_it, end_it};
    if (auto it = std::ranges::find(range, entry_separator);
        it != std::ranges::end(range)) {
        begin_it = std::next(it);
    }
    range = std::ranges::subrange{begin_it, end_it};
    auto it = std::ranges::find(range, entry_separator);
    if (it != std::ranges::end(range)) {
        this->recall_it = begin_it;
    }
    return std::ranges::subrange{begin_it, it};
}

Err History::cmd_history_fn(Cmd::Ctx ctx) {
    for (const char c : ctx.cli.history.ring_buffer) {
        ctx.cli.print(c);
    }
    return Err::ok;
};

Cmd History::cmd_history =
    Cmd{Cmd::Cfg{.cmd_list = Cmd::get_base_cmd_list(),
                 .name = "history,hist",
                 .short_description = "print command history",
                 .fn = [](Cmd::Ctx ctx) {
                     for (const char c : ctx.cli.history.ring_buffer) {
                         if (c == entry_separator) {
                             ctx.cli.print('\n');
                             continue;
                         }
                         ctx.cli.print(c);
                     }
                     return Err::ok;
                 }}};

} // namespace ln::shell
