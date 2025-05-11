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

    void fonas_logger_log(LoggerModule *module, LoggerLevel level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif
