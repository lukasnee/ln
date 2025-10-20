#include "ln/shell/Args.hpp"

#include <string_view>
#include <algorithm> // for std::min, std::copy_n
#include <cstdio>
#include <limits>

namespace ln::shell {

Args::Args(std::span<char> buf) : buf(buf) {}

Args::Args(std::span<char> buf, std::string_view str) : buf(buf) {
    const auto safe_len = std::min(str.length(), buf.size() - 1);
    std::copy_n(str.begin(), safe_len, buf.data());
    buf[safe_len] = '\0';
    this->resolve_into_args();
}

Args::Args(std::span<char> buf, std::size_t argc, const char **argv) : buf(buf) { this->copy_from(argc, argv); }

void Args::clear() {
    this->count = 0;
    this->args.fill(nullptr);
}

bool Args::copy_from(std::size_t argc, const char *argv[]) {
    if (!argv) {
        return false;
    }
    this->count = 0;
    this->args.fill(nullptr);
    std::size_t buff_left = this->buf.size();
    for (const char **argv_it = argv; (*argv_it && argv_it < (argv + argc) && count < this->args.size()); argv_it++) {
        const std::string_view arg_it_sv{*argv_it};
        if ((arg_it_sv.length() + sizeof('\0')) > buff_left) {
            return false;
        }
        char *arg = &this->buf[this->buf.size() - buff_left];
        std::copy_n(arg_it_sv.begin(), arg_it_sv.length(), arg);
        arg[arg_it_sv.length()] = '\0';
        buff_left -= (arg_it_sv.length() + sizeof('\0'));
        this->args[this->count++] = arg;
    }
    return true;
}

bool Args::resolve_into_args() {
    return this->resolve_string_to_args(reinterpret_cast<char *>(this->buf.data()), this->buf.size());
}

bool Args::restore_into_string() {
    for (std::size_t arg_it = 1; arg_it < this->count; arg_it++) {
        std::size_t char_buff_offset = this->args[arg_it] - this->buf.data();
        if (char_buff_offset >= this->buf.size()) {
            return false;
        }
        if (this->buf[char_buff_offset] == '\0') {
            this->buf[char_buff_offset] = ' ';
        }
    }
    return true;
}

bool Args::resolve_string_to_args(char *str, std::size_t len) {
    this->count = 0;
    this->args.fill(nullptr);

    if (!str || len == 0) {
        return false;
    }
    char *next_arg = nullptr;
    for (char *string_head = str; static_cast<std::size_t>(string_head - str) < len; ++string_head) {
        char *const ch = string_head;
        if (*ch != ' ' && *ch != '\0') {
            *ch = '\0';
            if (next_arg) {
                if (this->count >= this->args.size()) {
                    return false;
                }
                this->args[this->count++] = next_arg;
                next_arg = nullptr;
            }
        }
        else if (!next_arg) {
            next_arg = ch;
        }
    }
    return true;
}

bool Args::print_to(char *buf, std::size_t size, const char *delimiter, bool null_separated) {
    if (this->count == 0 || !this->args[0] || !buf || size == 0 || size > std::numeric_limits<int>::max() ||
        !delimiter) {
        return false;
    }
    std::array<char, (Config::max_delimiter_length + sizeof("%s"))> fmt;
    int snprintf_res = snprintf(fmt.data(), fmt.size(), "%%s%s", delimiter);
    if (snprintf_res <= 0 || static_cast<std::size_t>(snprintf_res) >= fmt.size()) {
        return false;
    }
    char *buffer_out_head = buf;
    int buffer_out_size_left = static_cast<int>(size);
    std::size_t cnt = this->count;
    for (auto &arg : this->args) {
        if (cnt == 0 || !arg || buffer_out_size_left <= 0) {
            return true;
        }
        int snprintf_res = snprintf(buffer_out_head, buffer_out_size_left, arg);
        if (snprintf_res > 0) {
            buffer_out_head += snprintf_res;
            buffer_out_size_left -= snprintf_res;
            if (null_separated && buffer_out_size_left) {
                *buffer_out_head = '\0';
                buffer_out_head += sizeof('\0');
                buffer_out_size_left -= sizeof('\0');
            }
        }
        if (cnt > 1) {
            int snprintf_res = snprintf(buffer_out_head, buffer_out_size_left, delimiter);
            if (snprintf_res > 0) {
                buffer_out_head += snprintf_res;
                buffer_out_size_left -= snprintf_res;
            }
        }
        cnt--;
    }
    return false;
}

} // namespace ln::shell
