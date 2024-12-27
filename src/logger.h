#pragma once

#ifndef LOG_LEVEL
#warning "no LOG_LEVEL macro set! defaulting to LOG_INFO(3)"
#define LOG_LEVEL 3
#endif

typedef enum log_level {
    LOG_TRACE = 0,
    LOG_VERBOSE = 1,
    LOG_DEBUG = 2,
    LOG_INFO = 3,
    LOG_WARN = 4,
    LOG_ERROR = 5,
    LOG_FATAL = 6,
} log_level;

#define XAB_LOG_PREFIX_THINGY "-- "
void xab_log(log_level level, const char *format, ...);
