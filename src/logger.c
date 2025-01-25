#include "logger.h"

#include <stdio.h>
#include <stdarg.h>

void xab_log(log_level level, const char *format, ...) {
    if (level < LOG_LEVEL)
        return;
    const char *logtype_prefix = "";
    switch (level) {
    case LOG_TRACE:
        logtype_prefix = "TRACE";
        break;
    case LOG_VERBOSE:
        logtype_prefix = "VERBOSE";
        break;
    case LOG_DEBUG:
        logtype_prefix = "DEBUG";
        break;
    case LOG_INFO:
        logtype_prefix = "INFO";
        break;
    case LOG_WARN:
        logtype_prefix = "WARN";
        break;
    case LOG_ERROR:
        logtype_prefix = "ERROR";
        break;
    case LOG_FATAL:
        logtype_prefix = "FATAL";
        break;
    default:
        fprintf(stderr, "XAB LOG ERROR: INVALID LOG LEVEL!\n");
        return;
        break; // idk why
    };
    va_list args;
    va_start(args, format);
    fprintf(stderr, XAB_LOG_PREFIX_THINGY "[%s%s%s] ", get_logtype_color(level),
            logtype_prefix, COLOR_RESET);
    vfprintf(stderr, format, args);
    // no \n, deal with it
    va_end(args);
}
