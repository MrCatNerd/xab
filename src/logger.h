#pragma once

#ifndef LOG_LEVEL
#warning "no LOG_LEVEL macro set! defaulting to LOG_INFO(3)"
#define LOG_LEVEL 3
#endif

#include "pch.h"
#include "ansii_colors.h"

typedef enum log_level {
    LOG_TRACE = 0,
    LOG_VERBOSE = 1,
    LOG_DEBUG = 2,
    LOG_INFO = 3,
    LOG_WARN = 4,
    LOG_ERROR = 5,
    LOG_FATAL = 6,
} log_level;

// get the logtype text color
static inline const char *get_logtype_color(int level) {
#ifdef LOG_ANSII_COLORS
    switch (level) {
    case LOG_TRACE:
        return TRACE_COLOR;
    case LOG_VERBOSE:
        return VERBOSE_COLOR;
    case LOG_DEBUG:
        return DEBUG_COLOR;
    case LOG_INFO:
        return INFO_COLOR;
    case LOG_WARN:
        return WARN_COLOR;
    case LOG_ERROR:
        return ERROR_COLOR;
    case LOG_FATAL:
        return FATAL_COLOR;
    }
#endif
    return "";
}

#define XAB_LOG_PREFIX_THINGY " "

void xab_log(log_level level, const char *format, ...);
