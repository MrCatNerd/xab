#pragma once

// debug
#ifndef NDEBUG
#ifndef LOGGER_STDIO_INCLUDED
#define LOGGER_STDIO_INCLUDED
#include <stdio.h>
#endif // !LOGGER_STDIO_INCLUDED

#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif // !NDEBUG

// verbose
#ifndef NVERBOSE
#ifndef LOGGER_STDIO_INCLUDED
#define LOGGER_STDIO_INCLUDED
#include <stdio.h>
#endif // !LOGGER_STDIO_INCLUDED

#define VLOG(...) printf(__VA_ARGS__)
#else
#define VLOG(...)
#endif // !NVERBOSE

#undef LOGGER_STDIO_INCLUDED // cuz its useless
