#pragma once

#include "fonas/fonas.hpp"

#include <cstdio> // for FILE, stdout

extern "C"
{
#include "logger.h"
}

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace fonas {

/**
 * @brief RTOS logger with Python logging style.
 */
class Logger {
public:
    using Level = LoggerLevel;

    struct Config {
        /* Output stream */
        FILE *out_file = stdout;
        /* Switch logger on/off at compile time */
        static constexpr bool enabled_compile_time = true;
        /* Switch logger on/off at run-time */
        bool enabled_run_time = true;
        /* Global log level printing threshold */
        Level log_level = LOGGER_LEVEL_INFO;
        /* Colorize log messages */
        bool color = false;
        /* Print log message header */
        bool print_header_enabled = true;
    };

    static Logger &get_instance();

    void enable();

    bool is_enabled();

    void set_level(Level log_level);
    const Config &get_config() const { return config; }

    class Module : public LoggerModule {
    public:
        Module(const std::string_view name, Level log_level = LOGGER_LEVEL_NOTSET);

        template <typename... Args> void debug(const std::string_view fmt, Args &&...args) {
            log(LOGGER_LEVEL_DEBUG, fmt, std::forward<Args>(args)...);
        }
        template <typename... Args> void info(const std::string_view fmt, Args &&...args) {
            log(LOGGER_LEVEL_INFO, fmt, std::forward<Args>(args)...);
        }
        template <typename... Args> void warning(const std::string_view fmt, Args &&...args) {
            log(LOGGER_LEVEL_WARNING, fmt, std::forward<Args>(args)...);
        }
        template <typename... Args> void error(const std::string_view fmt, Args &&...args) {
            log(LOGGER_LEVEL_ERROR, fmt, std::forward<Args>(args)...);
        }
        template <typename... Args> void critical(const std::string_view fmt, Args &&...args) {
            log(LOGGER_LEVEL_CRITICAL, fmt, std::forward<Args>(args)...);
        }
        void log(const Level &level, const std::string_view fmt, ...);

        void set_level(Level log_level);
    };

    int log(const LoggerModule &module, const Level &level, const std::string_view fmt, const va_list &argList);

    struct Hex {

        template <std::size_t N>
        static const char *format(std::array<char, N> &out_buff, const uint8_t *in_data, size_t in_size,
                                  const char *delimiter = "", const char *byte_fmt = "%02X") {
            return format(out_buff.data(), out_buff.size(), in_data, in_size, delimiter, byte_fmt);
        }

        static const char *format(char *out_buff_data, size_t out_buff_size, const uint8_t *in_data, size_t in_size,
                                  const char *delimiter = "", const char *byte_fmt = "%02X") {
            if (!in_data) {
                return nullptr;
            }
            if (in_size == 0) {
                return "";
            }
            if (!out_buff_data) {
                return nullptr;
            }
            if (out_buff_size < 2) {
                return nullptr;
            }
            size_t cp = 0;
            size_t i = 0;
            for (; i < in_size && cp < out_buff_size - 1; i++) {
                cp += std::snprintf(out_buff_data + cp, out_buff_size - cp, byte_fmt, in_data[i]);
                if (i + 1 < in_size) {
                    cp += std::snprintf(out_buff_data + cp, out_buff_size - cp, "%s", delimiter);
                }
            }
            if (i < in_size) { // Indicate truncation
                const auto offset = std::min(cp, out_buff_size - 2);
                std::snprintf(out_buff_data + offset, out_buff_size - offset, "*");
            }
            return out_buff_data;
        }
    };

protected:
    Config config;

private:
    Logger() = default;
    Logger(Logger const &) = delete;
    void operator=(Logger const &) = delete;
    ~Logger() = default;

    int print_header(const LoggerModule &module, const Level &level);

    int printf(const char *fmt, ...);

    cpp_freertos::MutexStandard mutex;
};

} // namespace fonas
