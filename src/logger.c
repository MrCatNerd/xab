#include "logger.h"

#include <stdio.h>
#include <stdarg.h>

void xab_log(log_level level, const char *format, ...) {
    if (level < LOG_LEVEL)
        return;
    const char *prefix = "";
    switch (level) {
    case LOG_TRACE:
        prefix = XAB_LOG_PREFIX_THINGY "TRACE: ";
        break;
    case LOG_VERBOSE:
        prefix = XAB_LOG_PREFIX_THINGY "VERBOSE: ";
        break;
    case LOG_DEBUG:
        prefix = XAB_LOG_PREFIX_THINGY "DEBUG: ";
        break;
    case LOG_INFO:
        prefix = XAB_LOG_PREFIX_THINGY "INFO: ";
        break;
    case LOG_WARN:
        prefix = XAB_LOG_PREFIX_THINGY "WARN: ";
        break;
    case LOG_ERROR:
        prefix = XAB_LOG_PREFIX_THINGY "ERROR: ";
        break;
    case LOG_FATAL:
        prefix = XAB_LOG_PREFIX_THINGY "FATAL: ";
        break;
    default:
        fprintf(stderr, "XAB LOG ERROR: INVALID LOG LEVEL!\n");
        return;
        break; // idk why
    };
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s", prefix);
    vfprintf(stderr, format, args);
    // no \n, deal with it
    va_end(args);
}
