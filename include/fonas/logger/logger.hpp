#pragma once

#include "fonas/fonas.hpp"

extern "C"
{
#include "logger.h"
}

#include <cstdarg>
#include <string_view>

namespace fonas {

class Logger {
public:
    using Level = LoggerLevel;

    struct Config {
        /* Switch logger on/off at compile time */
        static constexpr bool enabled_compile_time = true;
        /* Switch logger on/off at run-time */
        bool enabled_run_time = true;
        /* Global log level printing threshold */
        Level log_level = LOGGER_LEVEL_LOWEST;
        /* Colorize log messages */
        bool color = false;
        /* Print log message header */
        bool header = true;
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

protected:
    Config config;

private:
    Logger() = default;
    Logger(Logger const &) = delete;
    void operator=(Logger const &) = delete;
    ~Logger() = default;

    int print_header(const LoggerModule &module, const Level &level);

    cpp_freertos::MutexStandard mutex;
};

} // namespace fonas
