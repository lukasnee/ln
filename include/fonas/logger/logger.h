#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum {
        LOGGER_LEVEL_NOTSET = 0,
        LOGGER_LEVEL_LOWEST = 1,
        LOGGER_LEVEL_DEBUG3 = 8,
        LOGGER_LEVEL_DEBUG2 = 9,
        LOGGER_LEVEL_DEBUG = 10,
        LOGGER_LEVEL_INFO = 20,
        LOGGER_LEVEL_WARNING = 30,
        LOGGER_LEVEL_ERROR = 40,
        LOGGER_LEVEL_CRITICAL = 50,

        LOGGER_LEVEL_MAX_ = LOGGER_LEVEL_CRITICAL,
    } LoggerLevel;

    typedef struct {
        const char *name;
        LoggerLevel log_level;
    } LoggerModule;

    void fonas_logger_enable();

    void fonas_logger_log(LoggerModule *module, LoggerLevel level, const char *fmt, ...);

#define LOG_SCOPE(scope) LoggerModule logger_module = {.name = #scope, .log_level = LOGGER_LEVEL_NOTSET};
#define LOG(level, ...) fonas_logger_log(&logger_module, level, __VA_ARGS__);
#define LOG_DEBUG(...) LOG(LOGGER_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) LOG(LOGGER_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARNING(...) LOG(LOGGER_LEVEL_WARNING, __VA_ARGS__)
#define LOG_ERROR(...) LOG(LOGGER_LEVEL_ERROR, __VA_ARGS__)
#define LOG_CRITICAL(...) LOG(LOGGER_LEVEL_CRITICAL, __VA_ARGS__)

#ifdef __cplusplus
}
#endif
