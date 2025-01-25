#pragma once

#ifdef LOG_ANSII_COLORS
/* ANSI Reset */
#define COLOR_RESET                                                            \
    "\x1b[0m\x1b[?25h" /* first one: reset stuff | second one: fix wierd       \
                          cursor problem */

/* Foreground Colors */
#define TRACE_COLOR "\x1b[37m"         /* White */
#define VERBOSE_COLOR "\x1b[36m"       /* Cyan */
#define DEBUG_COLOR "\x1b[32m"         /* Green */
#define INFO_COLOR "\x1b[35m"          /* Magenta */
#define WARN_COLOR "\x1b[33m"          /* Yellow */
#define ERROR_COLOR "\x1b[31m"         /* Red */
#define FATAL_COLOR "\x1b[41m\x1b[37m" /* White on Red */

#else
#define COLOR_RESET ""
#endif
