#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ln::shell {

class Args {
public:
    struct Config {
        static constexpr std::size_t max_args = 16;
        static constexpr std::size_t max_delimiter_length = 8;
    };

    Args(std::span<char> buf);
    Args(std::span<char> buf, std::string_view str);
    Args(std::span<char> buf, std::size_t argc, const char **argv);

    void clear();

    bool copy_from(std::size_t argc, const char *argv[]);
    bool resolve_into_args();
    bool restore_into_string();

    /**
     * @brief copy out arguments (contents) by given format into buffer.
     *
     * @param delimiter Delimiter put printed after every argument, e.g.: " ", "\n"...
     * @param buf Buffer to print to.
     * @param size Buffer to print to size (max).
     * @param nullSeparated Puts null at the end of every delimiter between arguments. passing empty delimiter "" to
     * copy arguments to some other buffer
     * @return true Printed successfully.
     * @return false Could not format the delimter (delimiter too long) or could not finish printing (no space left in
     * buffer).
     */
    bool print_to(char *buf, std::size_t size, const char *delimiter = " ", bool nullSeparated = false);

    const std::size_t &get_argc() { return this->count; }

    const char **get_argv() { return this->args.data(); }

private:
    /**
     * @attention this function modifies the input string !
     */
    bool resolve_string_to_args(char *str, std::size_t len);

    std::span<char> buf;
    std::size_t count = 0;
    std::array<const char *, Config::max_args> args{{nullptr}};
};

} // namespace ln::shell
